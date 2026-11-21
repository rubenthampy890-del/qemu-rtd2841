#!/usr/bin/env python3
"""
tools/rtd2841_cli.py - Unified Turnkey CLI & Runtime Engine
OpenDTV Smart TV Emulation Engine (Realtek RTD2841 / TCL RT41 Chassis, Profile tv030)

Features:
1. Zero-argument invocation: auto-detects dependencies/ports, offers an interactive terminal
   menu and launches Android TV with non-destructive qcow2 copy-on-write overlays over output/emmc.raw.
2. CLI flags:
   --system <img|tar>      Mount/boot custom system image or raw emmc disk
   --vendor <img|tar>      Mount custom vendor partition
   --update-img <path>     Report the extracted inputs required for an OTA
   --debloat               Run the automated 3-tiered debloating pipeline (tools/build_debloated_rom.py)
   --export-usb [dev|dir]  Export Update.img with SHA256 checksum to USB drive
   --adb                   Connect ADB shell to running emulator (localhost:5555)
   --vnc [display]         Launch QEMU with VNC display backend (e.g. :0 -> port 5900)
   --serial                Interactive serial console mode (stdio)
   --reset                 Wipe and recreate qcow2 copy-on-write overlay (restores fresh state)
   --diag                  Boot bare-metal diagnostic kernel shell
   --headless / --no-gui   Run headless without graphical window (-display none)
   --display <backend>     Set display backend explicitly (cocoa, gtk, sdl, vnc, none)
   --check                 Run dependency & environment health check and exit
   --menu                  Force interactive terminal menu
   --adb-port <port>       Set ADB host forwarding port (default: 5555)
3. Strict adherence to RTD2841 Project Inviolable Doctrines:
   - Doctrine 1: Zero-Assumption Engineering (checked syscalls and subprocess returns)
   - Doctrine 2: Strict Target Architecture & ABI Ground Truth (ARM32 EABI)
   - Doctrine 3: Hardware Model Fidelity & Capability Transparency
   - Doctrine 4: Subprocess Safety & Non-Blocking CI
   - Doctrine 5: DMA Ring & Memory Safety
   - Doctrine 6: Branch & Push Safety
"""

import argparse
import glob
import hashlib
import json
import os
import pathlib
import re
import selectors
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import uuid
import zlib

# Resolve Repository Root dynamically
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent

# ANSI Color Codes for Clean Terminal Presentation
CLR_RESET = "\033[0m"
CLR_BOLD = "\033[1m"
CLR_RED = "\033[1;31m"
CLR_GREEN = "\033[1;32m"
CLR_YELLOW = "\033[1;33m"
CLR_BLUE = "\033[1;34m"
CLR_CYAN = "\033[1;36m"
CLR_GRAY = "\033[0;90m"

# Track active child processes for clean signal handling (Doctrine 4)
ACTIVE_PROCESS = None


def sig_handler(signum, frame):
    """Graceful signal termination handler ensuring zero zombie or dangling processes."""
    global ACTIVE_PROCESS
    if ACTIVE_PROCESS and ACTIVE_PROCESS.poll() is None:
        try:
            print(f"\n{CLR_YELLOW}[!] Received interrupt signal ({signum}). Terminating child processes...{CLR_RESET}")
            child_pgid = os.getpgid(ACTIVE_PROCESS.pid)
            # Only signal process group if isolated in its own session (Doctrine 4)
            if child_pgid != os.getpgrp():
                os.killpg(child_pgid, signal.SIGTERM)
                time.sleep(0.5)
                if ACTIVE_PROCESS.poll() is None:
                    os.killpg(child_pgid, signal.SIGKILL)
            else:
                ACTIVE_PROCESS.terminate()
                time.sleep(0.5)
                if ACTIVE_PROCESS.poll() is None:
                    ACTIVE_PROCESS.kill()
        except Exception:
            pass
    sys.exit(128 + signum)


try:
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)
except Exception:
    pass


# ==============================================================================
# 1. Dependency Resolution & Environment Diagnostics (Checked Returns)
# ==============================================================================

def find_tool(name: str, extra_search_dirs: list = None) -> str:
    """Dynamically resolves tool executable across PATH and known system/Homebrew paths."""
    # 1. Check PATH
    p = shutil.which(name)
    if p and os.path.isfile(p) and os.access(p, os.X_OK):
        return os.path.abspath(p)

    # 2. Check predefined Homebrew and Unix search paths
    search_dirs = [
        "/opt/homebrew/bin",
        "/opt/homebrew/sbin",
        "/opt/homebrew/opt/e2fsprogs/sbin",
        "/opt/homebrew/opt/e2fsprogs/bin",
        "/usr/local/bin",
        "/usr/local/sbin",
        "/usr/bin",
        "/bin",
        "/usr/sbin",
        "/sbin",
    ]
    if extra_search_dirs:
        search_dirs = extra_search_dirs + search_dirs

    for d in search_dirs:
        cand = os.path.join(d, name)
        if os.path.isfile(cand) and os.access(cand, os.X_OK):
            return os.path.abspath(cand)

    # 3. Glob Cellar fallback for macOS Homebrew
    cellar_matches = sorted(glob.glob(f"/opt/homebrew/Cellar/*/*/bin/{name}") +
                            glob.glob(f"/opt/homebrew/Cellar/*/*/sbin/{name}"))
    if cellar_matches and os.access(cellar_matches[-1], os.X_OK):
        return os.path.abspath(cellar_matches[-1])

    return ""


def find_qemu() -> str:
    """Locate qemu-system-aarch64 with RTD2841 SoC emulation support."""
    if "QEMU_BIN" in os.environ and os.path.isfile(os.environ["QEMU_BIN"]) and os.access(os.environ["QEMU_BIN"], os.X_OK):
        return os.path.abspath(os.environ["QEMU_BIN"])

    candidates = [
        str(REPO_ROOT / "build/navigation/qemu-system-aarch64"),
        str(REPO_ROOT / "build/qemu-src/build/qemu-system-aarch64"),
        str(REPO_ROOT / "qemu/build/qemu-system-aarch64"),
        str(REPO_ROOT / "qemu-upstream/build/qemu-system-aarch64"),
        str(REPO_ROOT.parent / "tcl-firmware-re/qemu/build/qemu-system-aarch64"),
    ]
    for c in candidates:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return os.path.abspath(c)

    p = find_tool("qemu-system-aarch64")
    if p:
        return p
    return ""


def find_qemu_img() -> str:
    """Locate qemu-img for copy-on-write overlay manipulation."""
    if "QEMU_IMG" in os.environ and os.path.isfile(os.environ["QEMU_IMG"]) and os.access(os.environ["QEMU_IMG"], os.X_OK):
        return os.path.abspath(os.environ["QEMU_IMG"])

    qemu_bin = find_qemu()
    if qemu_bin:
        sibling = os.path.join(os.path.dirname(qemu_bin), "qemu-img")
        if os.path.isfile(sibling) and os.access(sibling, os.X_OK):
            return os.path.abspath(sibling)

    candidates = [
        str(REPO_ROOT / "build/qemu-src/build/qemu-img"),
        str(REPO_ROOT / "qemu/build/qemu-img"),
        str(REPO_ROOT / "qemu-upstream/build/qemu-img"),
    ]
    for c in candidates:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return os.path.abspath(c)

    p = find_tool("qemu-img")
    if p:
        return p
    return ""


def find_dtb() -> str:
    """Locate or compile Device Tree Blob (rtd284x-tv030.dtb)."""
    if "DTB" in os.environ and os.path.isfile(os.environ["DTB"]):
        return os.path.abspath(os.environ["DTB"])

    candidates = [
        str(REPO_ROOT / "dts/rtd284x-tv030.dtb"),
        str(REPO_ROOT / "output/kernel/rtd284x-tv030.dtb"),
    ]
    for c in candidates:
        if os.path.isfile(c) and os.path.getsize(c) > 0:
            return os.path.abspath(c)

    # Auto-compile DTB from DTS if dtc is available
    dts_src = REPO_ROOT / "dts/rtd284x-tv030.dts"
    dtb_out = REPO_ROOT / "dts/rtd284x-tv030.dtb"
    dtc_bin = find_tool("dtc")
    if dtc_bin and dts_src.is_file():
        print(f"[*] Compiling Device Tree: {dts_src} -> {dtb_out}")
        res = subprocess.run([dtc_bin, "-I", "dts", "-O", "dtb", "-o", str(dtb_out), str(dts_src)],
                             capture_output=True, text=True)
        if res.returncode == 0 and dtb_out.is_file():
            return str(dtb_out)

    return ""


def find_kernel(mode: str = "android") -> str:
    """Locate Linux kernel zImage or test binary."""
    if "KERNEL" in os.environ and os.path.isfile(os.environ["KERNEL"]):
        return os.path.abspath(os.environ["KERNEL"])
    if "KERNEL_BIN" in os.environ and os.path.isfile(os.environ["KERNEL_BIN"]):
        return os.path.abspath(os.environ["KERNEL_BIN"])

    if mode == "diag":
        diag_candidates = [
            str(REPO_ROOT / "tests/firmware_test.bin"),
            str(REPO_ROOT / "output/kernel/zImage"),
            str(REPO_ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage"),
        ]
        for c in diag_candidates:
            if os.path.isfile(c) and os.path.getsize(c) > 0:
                return os.path.abspath(c)

    candidates = [
        str(REPO_ROOT / "build/navigation/zImage"),
        str(REPO_ROOT / "output/kernel/zImage"),
        str(REPO_ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage"),
        str(REPO_ROOT / "staging/kernel.img"),
    ]
    for c in candidates:
        if os.path.isfile(c) and os.path.getsize(c) > 0:
            return os.path.abspath(c)

    return ""


def find_base_disk(preferred: str = "debloated") -> tuple:
    """
    Locates the base disk image.
    Returns (path: str, is_debloated: bool).
    """
    debloated_raw = REPO_ROOT / "output/emmc.raw"
    stock_raw = REPO_ROOT / "staging/emmc.raw"

    if preferred == "debloated":
        if debloated_raw.is_file() and debloated_raw.stat().st_size > 0:
            return (str(debloated_raw.resolve()), True)
        if stock_raw.is_file() and stock_raw.stat().st_size > 0:
            return (str(stock_raw.resolve()), False)
    else:
        if stock_raw.is_file() and stock_raw.stat().st_size > 0:
            return (str(stock_raw.resolve()), False)
        if debloated_raw.is_file() and debloated_raw.stat().st_size > 0:
            return (str(debloated_raw.resolve()), True)

    return ("", False)


def find_available_port(preferred: int = 5555) -> int:
    """Finds an available TCP port for host port forwarding (Doctrine 1: Checked)."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(("127.0.0.1", preferred))
            return preferred
        except OSError:
            pass

    for p in range(preferred + 1, preferred + 25):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                s.bind(("127.0.0.1", p))
                return p
            except OSError:
                continue

    return preferred


def is_socket_active(sock_path: str) -> bool:
    """Returns True if the UNIX domain socket exists and is actively accepting connections."""
    if not sock_path or not os.path.exists(sock_path):
        return False
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.settimeout(0.2)
            s.connect(sock_path)
            return True
    except (ConnectionRefusedError, OSError):
        return False


def is_file_in_use_by_qemu(file_path: str) -> bool:
    """Checks whether any running qemu-system process currently has the given file open."""
    if not file_path or not os.path.exists(file_path):
        return False
    target_abs = os.path.abspath(file_path)
    lsof_bin = shutil.which("lsof")
    if lsof_bin:
        try:
            res = subprocess.run([lsof_bin, target_abs], capture_output=True, text=True, timeout=2.0)
            if res.returncode == 0 and "qemu" in res.stdout.lower():
                return True
        except Exception:
            pass
    try:
        ps_res = subprocess.run(["ps", "aux"], capture_output=True, text=True, timeout=2.0)
        if ps_res.returncode == 0:
            for line in ps_res.stdout.splitlines():
                if "qemu-system" in line and target_abs in line:
                    return True
    except Exception:
        pass
    return False


def clean_stale_sockets(sock_paths: list):
    """Safely cleans up stale UNIX domain sockets if no active QEMU server is bound."""
    for sock in sock_paths:
        if os.path.exists(sock):
            try:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                    s.settimeout(0.2)
                    s.connect(sock)
                    # Socket is actively listening; do not delete!
            except (ConnectionRefusedError, OSError):
                try:
                    os.unlink(sock)
                except OSError:
                    pass


def check_environment() -> dict:
    """Performs deep dependency and readiness audit across system components."""
    status = {}

    qemu_bin = find_qemu()
    if qemu_bin:
        # Verify machine model support
        res = subprocess.run([qemu_bin, "-M", "help"], capture_output=True, text=True)
        rtd_ok = "rtd2841-tv030" in res.stdout or "rtd2841" in res.stdout
        status["qemu"] = {"ok": rtd_ok, "path": qemu_bin, "detail": "RTD2841 machine supported" if rtd_ok else "Missing rtd2841 model"}
    else:
        status["qemu"] = {"ok": False, "path": "", "detail": "qemu-system-aarch64 not found"}

    qemu_img = find_qemu_img()
    status["qemu_img"] = {"ok": bool(qemu_img), "path": qemu_img, "detail": "qcow2 overlay manager"}

    adb_bin = find_tool("adb")
    status["adb"] = {"ok": bool(adb_bin), "path": adb_bin, "detail": "Android Debug Bridge"}

    dtc_bin = find_tool("dtc")
    status["dtc"] = {"ok": bool(dtc_bin), "path": dtc_bin, "detail": "Device Tree Compiler"}

    dtb_path = find_dtb()
    status["dtb"] = {"ok": bool(dtb_path), "path": dtb_path, "detail": "rtd284x-tv030.dtb"}

    kernel_path = find_kernel()
    status["kernel"] = {"ok": bool(kernel_path), "path": kernel_path, "detail": "Linux zImage (SMP)"}

    base_disk, is_deb = find_base_disk("debloated")
    disk_detail = f"{'Debloated' if is_deb else 'Stock'} (4096 MiB)" if base_disk else "No emmc.raw found"
    status["base_disk"] = {"ok": bool(base_disk), "path": base_disk, "debloated": is_deb, "detail": disk_detail}

    # Port 5555 check
    adb_port = find_available_port(5555)
    status["adb_port"] = {"ok": True, "port": adb_port, "in_use": adb_port != 5555}

    return status


def print_diagnostic_report():
    """Prints a structured, high-visibility diagnostic table."""
    status = check_environment()
    print(f"\n{CLR_BOLD}================================================================================{CLR_RESET}")
    print(f"{CLR_CYAN}{CLR_BOLD} Realtek RTD2841 / TCL RT41 Chassis - Environment Diagnostics{CLR_RESET}")
    print(f"{CLR_BOLD}================================================================================{CLR_RESET}")

    items = [
        ("QEMU Engine", status["qemu"]),
        ("QEMU-Img Tool", status["qemu_img"]),
        ("Android ADB", status["adb"]),
        ("DTC Compiler", status["dtc"]),
        ("Device Tree Blob", status["dtb"]),
        ("Kernel zImage", status["kernel"]),
        ("Base eMMC Disk", status["base_disk"]),
    ]

    for label, info in items:
        if info["ok"]:
            tag = f"{CLR_GREEN}[OK]{CLR_RESET}"
            path_str = f"{CLR_RESET}{info['path']} ({info['detail']})"
        else:
            tag = f"{CLR_RED}[FAIL]{CLR_RESET}"
            path_str = f"{CLR_YELLOW}{info['detail']}{CLR_RESET}"
        print(f" {tag} {CLR_BOLD}{label:<18}{CLR_RESET} : {path_str}")

    port_info = status["adb_port"]
    if port_info["in_use"]:
        print(f" {CLR_YELLOW}[WARN]{CLR_RESET} {CLR_BOLD}{'ADB Host Port':<18}{CLR_RESET} : Port 5555 in use; will use {port_info['port']}")
    else:
        print(f" {CLR_GREEN}[OK]{CLR_RESET} {CLR_BOLD}{'ADB Host Port':<18}{CLR_RESET} : Port 5555 ready")

    print(f"{CLR_BOLD}================================================================================\n{CLR_RESET}")


# ==============================================================================
# 2. Non-Destructive Copy-on-Write (QCOW2) Overlay Manager
# ==============================================================================

def verify_overlay_backing(qemu_img, overlay, base):
    result = subprocess.run([qemu_img, "info", "--output=json", str(overlay)],
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"Cannot inspect {overlay}: {result.stderr.strip()}")
    info = json.loads(result.stdout)
    backing = info.get("full-backing-filename") or info.get("backing-filename")
    if (info.get("format") != "qcow2" or not backing or
            (overlay.parent / backing).resolve() != base):
        raise ValueError(f"{overlay} belongs to a different disk. Expected backing file {base}.")


def ensure_cow_overlay(base_disk: str, overlay_path: str = None, force_reset: bool = False) -> str:
    """
    Creates or verifies a non-destructive qcow2 copy-on-write overlay backed by base_disk.
    Ensures that the base raw disk is NEVER modified directly during execution.
    """
    if not base_disk or not os.path.isfile(base_disk):
        raise FileNotFoundError(f"Base disk image not found: '{base_disk}'")

    qemu_img = find_qemu_img()
    if not qemu_img:
        raise RuntimeError("qemu-img binary not found. Cannot manage copy-on-write overlay.")

    base = pathlib.Path(base_disk).resolve()
    base_format = "raw"
    prepared_images = [REPO_ROOT / "output" / (name + ".qcow2") for name in
                       ("emmc-projectivy-usb", "emmc-projectivy-ready", "emmc-live-overlay")]
    if not overlay_path and base == (REPO_ROOT / "output/emmc.raw").resolve():
        for prepared in prepared_images:
            if prepared.is_file():
                verify_overlay_backing(qemu_img, prepared, base)
                base = prepared.resolve()
                base_format = "qcow2"
                overlay_path = prepared.with_name(prepared.stem + "-session.qcow2")
                break
    if not overlay_path:
        directory = REPO_ROOT / "build/phase3"
        directory.mkdir(parents=True, exist_ok=True)
        legacy = directory / "emmc-android-overlay.qcow2"
        if base in ((REPO_ROOT / "staging/emmc.raw").resolve(),
                    (REPO_ROOT / "output/emmc.raw").resolve()):
            overlay_path = legacy
        else:
            key = hashlib.sha256(os.fsencode(base)).hexdigest()[:16]
            overlay_path = directory / f"emmc-{key}.qcow2"

    overlay = pathlib.Path(overlay_path).absolute()
    if overlay.is_symlink() or (overlay.exists() and overlay.samefile(base)):
        raise ValueError(f"Choose a separate regular file for --overlay: {overlay}")
    replacing = overlay.exists()
    if replacing:
        if any(p.exists() and p.samefile(overlay) for p in prepared_images):
            raise ValueError(f"{overlay} is a prepared image. Choose a separate session file.")
        verify_overlay_backing(qemu_img, overlay, base)
        if not force_reset:
            return str(overlay)
        if is_file_in_use_by_qemu(str(overlay)):
            raise RuntimeError(f"{overlay} is in active use by running QEMU process.")

    with tempfile.TemporaryDirectory(dir=overlay.parent) as tmp:
        fresh = pathlib.Path(tmp) / "session.qcow2"
        result = subprocess.run([qemu_img, "create", "-f", "qcow2", "-F", base_format,
                                 "-b", str(base), str(fresh)], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"Cannot create {overlay}: {result.stderr.strip()}")
        if replacing:
            os.replace(fresh, overlay)
        else:
            os.link(fresh, overlay)
    return str(overlay)


# ==============================================================================
# 3. Custom Firmware Mounting & Partition Mapping Engine
# ==============================================================================

def mount_custom_firmware(custom_system: str = None,
                           custom_vendor: str = None,
                           custom_update_img: str = None) -> str:
    """
    Seamless custom firmware ingestion:
    1. If a full raw composite disk is passed, uses it directly.
    2. If individual partition images (.img) or tarballs (.tar) are passed,
       assembles an output composite GPT disk (emmc_custom.raw) with automated partition mapping.
    3. OTA packages must be extracted before importing partitions.
    """
    workspace_root = str(REPO_ROOT)
    output_dir = REPO_ROOT / "output"
    staging_dir = REPO_ROOT / "staging"
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(staging_dir, exist_ok=True)

    for path in (custom_update_img, custom_system, custom_vendor):
        if path and not os.path.isfile(path):
            raise FileNotFoundError(f"Firmware file does not exist: {path}")
    if custom_update_img:
        raise ValueError("Extract the OTA first. Supply a prepared GPT disk with --system, "
                         "or supply prepared ext4 partitions with --system and --vendor.")

    # 2. Raw composite disk image passed via --system
    if custom_system and os.path.isfile(custom_system):
        size = os.path.getsize(custom_system)
        # Check if file has GPT signature at sector 1 (LBA 1, offset 512)
        with open(custom_system, "rb") as f:
            f.seek(512)
            sig = f.read(8)
            if sig == b"EFI PART":
                if custom_vendor:
                    raise ValueError("A complete GPT disk already contains its vendor partition.")
                print(f"{CLR_GREEN}[+] Detected custom composite raw disk: {custom_system} ({size // (1024*1024)} MiB){CLR_RESET}")
                return os.path.abspath(custom_system)

    # 3. Individual partition assembly (system.img/tar + vendor.img/tar)
    print(f"\n{CLR_CYAN}[*] Assembling custom composite GPT disk from partition inputs...{CLR_RESET}")
    mke2fs = find_tool("mke2fs")
    if not mke2fs:
        raise RuntimeError("mke2fs utility (e2fsprogs) is required for custom partition formatting but was not found.")

    for path in (custom_system, custom_vendor):
        if path and not path.endswith(".tar"):
            with open(path, "rb") as partition:
                partition.seek(1080)
                if partition.read(2) != b"\x53\xef":
                    raise ValueError(f"Expected a raw ext4 partition or a prepared GPT disk: {path}")
    import_dir = pathlib.Path(tempfile.mkdtemp(prefix="firmware-", dir=output_dir))
    sys_img = ""
    tmp_dirs_to_clean = []
    try:
        if custom_system and os.path.isfile(custom_system):
            if custom_system.endswith(".tar"):
                print(f"[*] Extracting system tarball: {custom_system}")
                tmp_sys_dir = tempfile.mkdtemp(prefix="custom_sys_")
                tmp_dirs_to_clean.append(tmp_sys_dir)
                res = subprocess.run(["tar", "-xf", os.path.abspath(custom_system), "-C", tmp_sys_dir], capture_output=True, text=True)
                if res.returncode != 0:
                    raise RuntimeError(f"Failed to extract system tar: {res.stderr}")
                sys_img = str(import_dir / "custom_system.img")
                res = subprocess.run([
                    mke2fs, "-t", "ext4", "-F",
                    "-O", "^metadata_csum,^orphan_file,^dir_index",
                    "-L", "system", "-b", "4096", "-I", "256",
                    "-E", "root_owner=0:0",
                    "-d", tmp_sys_dir, sys_img, "2048M"
                ], capture_output=True, text=True)
                if res.returncode != 0:
                    raise RuntimeError(f"Failed to create ext4 system.img: {res.stderr}")
            else:
                sys_img = os.path.abspath(custom_system)
        else:
            # Fallback to existing staging/system.img
            sys_img = str(staging_dir / "system.img")
            if not os.path.isfile(sys_img):
                raise FileNotFoundError(
                    f"Base system image not found at '{sys_img}'. Run --debloat first or supply --system <path>."
                )

        ven_img = ""
        if custom_vendor and os.path.isfile(custom_vendor):
            if custom_vendor.endswith(".tar"):
                print(f"[*] Extracting vendor tarball: {custom_vendor}")
                tmp_ven_dir = tempfile.mkdtemp(prefix="custom_ven_")
                tmp_dirs_to_clean.append(tmp_ven_dir)
                res = subprocess.run(["tar", "-xf", os.path.abspath(custom_vendor), "-C", tmp_ven_dir], capture_output=True, text=True)
                if res.returncode != 0:
                    raise RuntimeError(f"Failed to extract vendor tar: {res.stderr}")
                ven_img = str(import_dir / "custom_vendor.img")
                res = subprocess.run([
                    mke2fs, "-t", "ext4", "-F",
                    "-O", "^metadata_csum,^orphan_file,^dir_index",
                    "-L", "vendor", "-b", "4096", "-I", "256",
                    "-E", "root_owner=0:0",
                    "-d", tmp_ven_dir, ven_img, "1536M"
                ], capture_output=True, text=True)
                if res.returncode != 0:
                    raise RuntimeError(f"Failed to create ext4 vendor.img: {res.stderr}")
            else:
                ven_img = os.path.abspath(custom_vendor)
        else:
            ven_img = str(staging_dir / "vendor.img")
            if not os.path.isfile(ven_img):
                raise FileNotFoundError(
                    f"Base vendor image not found at '{ven_img}'. Run --debloat first or supply --vendor <path>."
                )

        data_img = str(staging_dir / "userdata.img")
        if not os.path.isfile(data_img):
            data_img = str(import_dir / "userdata.img")
            # Generate minimal userdata ext4 image
            res = subprocess.run([
                mke2fs, "-t", "ext4", "-F",
                "-O", "^metadata_csum,^orphan_file,^dir_index",
                "-L", "userdata", "-b", "4096", "-I", "256",
                data_img, "510M"
            ], capture_output=True, text=True)
            if res.returncode != 0:
                raise RuntimeError(f"Failed to create userdata.img: {res.stderr}")

        custom_composite_raw = str(import_dir / "emmc.raw")

        # Import and call composite GPT disk builder
        sys.path.insert(0, str(REPO_ROOT / "tools"))
        import build_debloated_rom as bdr
        sizes = [os.path.getsize(path) for path in (sys_img, ven_img, data_img)]
        if any(size <= 0 or size % 512 for size in sizes) or sum(sizes) > (4 << 30) - (2048 + 33) * 512:
            raise ValueError("Partitions must fit inside the 4 GiB eMMC and align to 512-byte sectors.")
        bdr.build_composite_emmc(
            workspace_root=workspace_root,
            system_img=sys_img,
            vendor_img=ven_img,
            userdata_img=data_img,
            output_path=custom_composite_raw
        )
        print(f"{CLR_GREEN}[+] Custom composite disk built successfully: {custom_composite_raw}{CLR_RESET}")
        return custom_composite_raw
    finally:
        for d in tmp_dirs_to_clean:
            shutil.rmtree(d, ignore_errors=True)


# ==============================================================================
# 4. Debloated ROM Build Execution
# ==============================================================================

def execute_debloat_pipeline(extra_args: list = None) -> bool:
    """Executes the verified Phase 4 debloating and packaging tool."""
    debloat_script = REPO_ROOT / "tools/build_debloated_rom.py"
    if not debloat_script.is_file():
        print(f"{CLR_RED}[-] Debloater script not found at {debloat_script}{CLR_RESET}")
        return False

    print(f"\n{CLR_CYAN}{CLR_BOLD}================================================================================{CLR_RESET}")
    print(f"{CLR_CYAN}{CLR_BOLD} Executing Debloated ROM Builder & Multi-Target Packaging Engine{CLR_RESET}")
    print(f"{CLR_CYAN}{CLR_BOLD}================================================================================{CLR_RESET}\n")

    cmd = [sys.executable, str(debloat_script)]
    if extra_args:
        cmd.extend(extra_args)

    res = subprocess.run(cmd)
    if res.returncode == 0:
        print(f"\n{CLR_GREEN}[✓] Debloated ROM build completed successfully.{CLR_RESET}")
        return True
    else:
        print(f"\n{CLR_RED}[-] Debloated ROM build failed with exit code {res.returncode}.{CLR_RESET}")
        return False


# ==============================================================================
# 5. Physical USB Recovery OTA Export Engine
# ==============================================================================

def export_usb(dest_dir: str = None) -> bool:
    """
    Exports the debloated physical OTA package (Update.img) with SHA256 checksum to a USB drive.
    Validates post-write integrity and reports the destination checksum.
    """
    src_img = REPO_ROOT / "output/Update_RT41_Debloated.img"
    if not src_img.is_file():
        # Fallback to stock update image if debloated not built yet
        fallback = REPO_ROOT / "staging/Update.img"
        if fallback.is_file():
            print(f"{CLR_YELLOW}[!] Debloated OTA not found in output/. Using baseline staging/Update.img.{CLR_RESET}")
            src_img = fallback
        else:
            print(f"{CLR_RED}[-] No Update.img found to export. Run --debloat first to build it.{CLR_RESET}")
            return False

    target_dir = dest_dir
    if not target_dir:
        # Detect mounted removable volumes on macOS
        system_vols = {"Macintosh HD", "Recovery", "Data", "VM", "Preboot", "boot"}
        mounted_vols = []
        if os.path.isdir("/Volumes"):
            for v in os.listdir("/Volumes"):
                if v not in system_vols and not v.startswith("."):
                    full_p = os.path.join("/Volumes", v)
                    if os.path.isdir(full_p):
                        try:
                            usage = shutil.disk_usage(full_p)
                            mounted_vols.append((v, full_p, usage.free // (1024 * 1024)))
                        except Exception:
                            pass

        if mounted_vols:
            print(f"\n{CLR_CYAN}[?] Detected mounted volumes for USB Export:{CLR_RESET}")
            for idx, (name, path, free_mb) in enumerate(mounted_vols, start=1):
                print(f"  [{idx}] {name} ({free_mb} MB free) -> {path}")
            print(f"  [C] Enter custom directory path")
            choice = input(f"{CLR_BOLD}Select volume [1-{len(mounted_vols)}] or C: {CLR_RESET}").strip()
            if choice.isdigit() and 1 <= int(choice) <= len(mounted_vols):
                target_dir = mounted_vols[int(choice) - 1][1]
            elif choice.upper() == "C":
                target_dir = input(f"{CLR_BOLD}Enter destination directory: {CLR_RESET}").strip()
            else:
                target_dir = mounted_vols[0][1]
        else:
            target_dir = input(f"{CLR_BOLD}Enter USB flash drive mount point or directory path: {CLR_RESET}").strip()

    if not target_dir or not os.path.isdir(target_dir):
        print(f"{CLR_RED}[-] Invalid destination directory: '{target_dir}'{CLR_RESET}")
        return False

    # Check available free space on destination before copying
    src_size = src_img.stat().st_size
    try:
        dest_free = shutil.disk_usage(target_dir).free
        if dest_free < src_size + (10 * 1024 * 1024):  # 10MB safety margin
            print(f"{CLR_RED}[-] Error: Insufficient free space on destination drive!{CLR_RESET}")
            print(f"    Required: {src_size // (1024*1024)} MiB")
            print(f"    Available: {dest_free // (1024*1024)} MiB")
            return False
    except Exception as e:
        print(f"{CLR_YELLOW}[!] Warning: Could not determine destination free space: {e}{CLR_RESET}")

    # TCL Recovery looks for 'Update.img' at the root of the USB drive
    dest_file = os.path.join(target_dir, "Update.img")
    dest_sha = os.path.join(target_dir, "Update.img.sha256")

    print(f"\n[*] Copying '{src_img.name}' -> '{dest_file}'...")
    print(f"[*] Size: {src_size // (1024*1024)} MiB. Please wait...")

    try:
        shutil.copy2(src_img, dest_file)
    except Exception as e:
        print(f"{CLR_RED}[-] FATAL: Failed to copy file to destination: {e}{CLR_RESET}")
        return False

    # Compute source SHA256 and write checksum file
    hasher = hashlib.sha256()
    with open(src_img, "rb") as f:
        while chunk := f.read(1024 * 1024):
            hasher.update(chunk)
    src_digest = hasher.hexdigest()

    with open(dest_sha, "w") as f:
        f.write(f"{src_digest}  Update.img\n")

    # Post-write verification on USB media (Doctrine 1: Checked Returns)
    print(f"[*] Verifying written data integrity on target storage media...")
    hasher_verify = hashlib.sha256()
    with open(dest_file, "rb") as f:
        while chunk := f.read(1024 * 1024):
            hasher_verify.update(chunk)
    dst_digest = hasher_verify.hexdigest()

    if src_digest != dst_digest:
        print(f"{CLR_RED}[-] FATAL: Checksum mismatch on written USB media! Write corruption occurred.{CLR_RESET}")
        return False

    print(f"\n{CLR_GREEN}{CLR_BOLD}[✓] USB Flash Drive Prepared Successfully!{CLR_RESET}")
    print(f"  Target File:     {dest_file}")
    print(f"  SHA-256 Digest:  {dst_digest}")
    print("File copy verified. Compatibility with physical TV recovery is unverified.")
    return True


# ==============================================================================
# 6. ADB Connection Assistant
# ==============================================================================

def connect_adb(port: int = 5555):
    """Connects interactive ADB shell to running RTD2841 emulator."""
    adb_bin = find_tool("adb")
    if not adb_bin:
        print(f"{CLR_RED}[-] adb binary not found in PATH or Homebrew.{CLR_RESET}")
        print("    Install with: brew install android-platform-tools")
        sys.exit(1)

    print(f"{CLR_CYAN}[*] Connecting to RTD2841 Android ADB on 127.0.0.1:{port}...{CLR_RESET}")
    try:
        res = subprocess.run([adb_bin, "connect", f"127.0.0.1:{port}"],
                             capture_output=True, text=True, timeout=15)
    except subprocess.TimeoutExpired:
        sys.exit(f"ADB connection to port {port} timed out.")
    print(res.stdout.strip())
    if res.returncode or "failed to connect" in res.stdout.lower() or "connection refused" in res.stdout.lower():
        print(f"{CLR_RED}[-] Failed to connect to emulator ADB port 127.0.0.1:{port}.{CLR_RESET}")
        print("    Ensure QEMU emulator is running with network enabled.")
        sys.exit(1)

    print("[*] Waiting for device readiness (15s timeout)...")
    try:
        res = subprocess.run([adb_bin, "-s", f"127.0.0.1:{port}", "wait-for-device"],
                             timeout=15.0, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"{CLR_RED}[-] Failed waiting for device: {res.stderr.strip()}{CLR_RESET}")
            sys.exit(res.returncode or 1)
    except subprocess.TimeoutExpired:
        print(f"{CLR_RED}[-] Timed out waiting for Android device on 127.0.0.1:{port}.{CLR_RESET}")
        print("    Ensure QEMU emulator is running with network enabled and guest has finished boot.")
        sys.exit(1)

    print(f"{CLR_GREEN}[+] Connected! Opening interactive root shell...{CLR_RESET}\n")
    # Replace current process with interactive adb shell
    os.execv(adb_bin, [adb_bin, "-s", f"127.0.0.1:{port}", "shell"])


# ==============================================================================
# 7. QEMU Runtime Launcher (Subprocess & Signal Safety)
# ==============================================================================

def boot_qemu(base_disk: str,
              kernel_bin: str,
              dtb_path: str,
              overlay_path: str = None,
              mode: str = "android",
              display_backend: str = None,
              vnc_display: str = None,
              adb_port: int = 5555,
              mem: str = "2048M",
              force_reset: bool = False,
              headless: bool = False,
              serial_stdio: bool = True):
    """
    Launches QEMU for RTD2841 with full doctrine compliance:
    - Non-destructive QCOW2 overlay on base disk
    - Dynamic port binding
    - UNIX domain socket cleanup
    - Subprocess safety & signal forwarding
    """
    global ACTIVE_PROCESS

    qemu_bin = find_qemu()
    if not qemu_bin:
        print(f"{CLR_RED}[-] Error: qemu-system-aarch64 binary not found.{CLR_RESET}")
        print("    Build QEMU following BUILD.md or set QEMU_BIN=/path/to/binary")
        sys.exit(1)

    if not kernel_bin or not os.path.isfile(kernel_bin):
        print(f"{CLR_RED}[-] Error: Kernel binary not found: '{kernel_bin}'{CLR_RESET}")
        sys.exit(1)

    if not dtb_path or not os.path.isfile(dtb_path):
        print(f"{CLR_RED}[-] Error: Device tree blob not found: '{dtb_path}'{CLR_RESET}")
        sys.exit(1)

    # Prepare non-destructive copy-on-write overlay
    disk_overlay = ensure_cow_overlay(base_disk, overlay_path=overlay_path, force_reset=force_reset)

    # Clean up stale dead sockets; isolate sockets if another QEMU instance or custom port is active
    qmp_sock = "/tmp/qemu-qmp.sock"
    mon_sock = "/tmp/qemu-monitor.sock"
    if adb_port != 5555 or is_socket_active(qmp_sock):
        qmp_sock = f"/tmp/qemu-qmp-{adb_port}.sock"
        mon_sock = f"/tmp/qemu-monitor-{adb_port}.sock"
        if is_socket_active(qmp_sock):
            qmp_sock = f"/tmp/qemu-qmp-{adb_port}-{os.getpid()}.sock"
            mon_sock = f"/tmp/qemu-monitor-{adb_port}-{os.getpid()}.sock"
    clean_stale_sockets([qmp_sock, mon_sock])

    # Display configuration
    resolved_display = display_backend
    extra_display_args = []

    if vnc_display is not None:
        vnc_target = vnc_display if vnc_display.startswith(":") else f":{vnc_display}"
        extra_display_args = ["-display", f"vnc=127.0.0.1{vnc_target}"]
        vnc_port = 5900 + int(vnc_target.replace(":", ""))
        print(f"{CLR_GREEN}[+] VNC Server enabled: 127.0.0.1:{vnc_port} (Display {vnc_target}){CLR_RESET}")
        print(f"    Connect using: open vnc://localhost:{vnc_port}")
    elif headless:
        extra_display_args = ["-display", "none"]
    elif not resolved_display:
        if sys.platform == "darwin":
            extra_display_args = ["-display", "cocoa"]
        else:
            extra_display_args = []
    else:
        extra_display_args = ["-display", resolved_display]

    # Kernel Command Line
    if mode == "android":
        cmdline = "console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait init=/init android=1 opendtv.ohci=1 enforcing=0 androidboot.selinux=permissive consoleblank=0 fbcon=map:9"
    else:
        cmdline = "console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait init=/init enforcing=0 androidboot.selinux=permissive consoleblank=0 fbcon=map:9"

    # Assemble full QEMU invocation
    qemu_cmd = [
        qemu_bin,
        "-M", "rtd2841-tv030",
        # Older RT41 builds use the Cortex-A53 default of 62.5 MHz.
        "-global", "cortex-a53-arm-cpu.cntfrq=27000000",
        "-m", mem,
        "-smp", "4",
        "-dtb", dtb_path,
        "-kernel", kernel_bin,
        "-drive", f"file={disk_overlay},format=qcow2,if=sd",
        "-append", cmdline,
        "-nic", f"user,model=rtd2841-eth,hostfwd=tcp:127.0.0.1:{adb_port}-:5555",
        "-qmp", f"unix:{qmp_sock},server=on,wait=off",
        "-monitor", f"unix:{mon_sock},server=on,wait=off",
    ]

    if serial_stdio:
        qemu_cmd.extend(["-serial", "stdio"])

    qemu_cmd.extend(extra_display_args)

    print(f"\n{CLR_BOLD}================================================================================{CLR_RESET}")
    print(f"{CLR_CYAN}{CLR_BOLD} Launching Realtek RTD2841 Emulator | TCL RT41 (tv030){CLR_RESET}")
    print(f"{CLR_BOLD}================================================================================{CLR_RESET}")
    print(f"  Mode:            {CLR_BOLD}{mode.upper()}{CLR_RESET}")
    print(f"  Guest RAM:       {CLR_BOLD}{mem}{CLR_RESET}")
    print(f"  Base Disk:       {base_disk}")
    print(f"  Overlay (COW):   {CLR_GREEN}{disk_overlay}{CLR_RESET}")
    print(f"  Kernel:          {kernel_bin}")
    print(f"  Device Tree:     {dtb_path}")
    print(f"  Network/ADB:     Host 127.0.0.1:{adb_port} -> Guest :5555")
    print(f"  Sockets:         QMP: {qmp_sock} | Monitor: {mon_sock}")
    print(f"{CLR_BOLD}================================================================================\n{CLR_RESET}")

    # Launch process directly with isolated process group session (Doctrine 4)
    proc = subprocess.Popen(qemu_cmd, start_new_session=True)
    ACTIVE_PROCESS = proc
    try:
        proc.wait()
    except KeyboardInterrupt:
        print(f"\n{CLR_YELLOW}[*] Shutting down QEMU emulator...{CLR_RESET}")
        try:
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=3.0)
        except (subprocess.TimeoutExpired, ProcessLookupError):
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    finally:
        ACTIVE_PROCESS = None

    sys.exit(proc.returncode if proc.returncode is not None else 0)


# ==============================================================================
# 8. Interactive Terminal Operations Menu
# ==============================================================================

def run_interactive_menu():
    """Renders the ANSI-decorated interactive operations terminal menu."""
    while True:
        status = check_environment()
        base_disk_path = status["base_disk"]["path"]
        is_debloated = status["base_disk"].get("debloated", False)

        disk_label = "None (Run Build/Debloat first)"
        if base_disk_path:
            disk_type = "Debloated Custom ROM" if is_debloated else "Stock Factory Baseline"
            disk_label = f"{disk_type} ({os.path.basename(base_disk_path)})"

        print(f"\n{CLR_CYAN}{CLR_BOLD}")
        print("  ####   #####   ###   #      #####  #####  #   #")
        print("  #   #  #      #   #  #        #    #      #  #")
        print("  ####   ####   #####  #        #    ####   ###")
        print("  #  #   #      #   #  #        #    #      #  #")
        print("  #   #  #####  #   #  #####    #    #####  #   #")
        print(f"{CLR_RESET}{CLR_BOLD} Realtek RTD2841 Emulator | TCL RT41 (tv030){CLR_RESET}")
        print(f"{CLR_BOLD}================================================================================{CLR_RESET}")
        print(f" {CLR_BOLD}Active Storage:{CLR_RESET}   {CLR_GREEN if is_debloated else CLR_YELLOW}{disk_label}{CLR_RESET}")
        print(f" {CLR_BOLD}Guest RAM:{CLR_RESET}        {CLR_GREEN}2048 MiB (Dual-Channel){CLR_RESET}")
        print(f" {CLR_BOLD}Kernel Engine:{CLR_RESET}    {CLR_GREEN if status['kernel']['ok'] else CLR_RED}{os.path.basename(status['kernel']['path'] or 'Missing')}{CLR_RESET}")
        print(f" {CLR_BOLD}Device Tree:{CLR_RESET}      {CLR_GREEN if status['dtb']['ok'] else CLR_RED}{os.path.basename(status['dtb']['path'] or 'Missing')}{CLR_RESET}")
        print(f" {CLR_BOLD}ADB Host Port:{CLR_RESET}    {CLR_GREEN}{status['adb_port']['port']}{CLR_RESET}")
        print(f"{CLR_BOLD}--------------------------------------------------------------------------------{CLR_RESET}")
        print(f" {CLR_BOLD}[Interactive Operations Menu]{CLR_RESET}")
        print(f"  {CLR_GREEN}[1]{CLR_RESET} Boot Debloated Android TV  {CLR_GRAY}(Default: ProjectivyLauncher, COW overlay){CLR_RESET}")
        print(f"  {CLR_BLUE}[2]{CLR_RESET} Boot Stock Android TV      {CLR_GRAY}(Stock factory baseline firmware){CLR_RESET}")
        print(f"  {CLR_CYAN}[3]{CLR_RESET} Boot Diagnostic Shell      {CLR_GRAY}(Bare-metal kernel & serial console){CLR_RESET}")
        print(f"  {CLR_YELLOW}[4]{CLR_RESET} Build Debloated ROM        {CLR_GRAY}(Purge Category A, inject launcher & provision){CLR_RESET}")
        print(f"  {CLR_BLUE}[5]{CLR_RESET} Export Flashable Update.img {CLR_GRAY}(Copy Update.img + SHA256 to USB drive){CLR_RESET}")
        print(f"  {CLR_GREEN}[6]{CLR_RESET} Connect ADB Shell          {CLR_GRAY}(Interactive root shell on localhost:5555){CLR_RESET}")
        print(f"  {CLR_RED}[7]{CLR_RESET} Reset COW Overlay          {CLR_GRAY}(Wipe transient changes and start clean){CLR_RESET}")
        print(f"  {CLR_CYAN}[8]{CLR_RESET} Mount Custom Firmware      {CLR_GRAY}(Select a prepared disk or system and vendor partitions){CLR_RESET}")
        print(f"  {CLR_GRAY}[9] Run Diagnostics Check       (Verify compilers, QEMU and dependencies){CLR_RESET}")
        print(f"  {CLR_RED}[0] Exit{CLR_RESET}")
        print(f"{CLR_BOLD}================================================================================{CLR_RESET}")

        try:
            choice = input(f"{CLR_BOLD}Select operation [1-9, 0 to exit] (Default: 1): {CLR_RESET}").strip()
        except (KeyboardInterrupt, EOFError):
            print("\nExiting.")
            sys.exit(0)

        if choice == "" or choice == "1":
            # Boot Debloated
            base_disk, _ = find_base_disk("debloated")
            if not base_disk:
                print(f"{CLR_YELLOW}[!] Debloated disk not found. Would you like to build it now? [Y/n]: {CLR_RESET}", end="")
                if input().strip().lower() in ("n", "no"):
                    base_disk, _ = find_base_disk("stock")
                else:
                    execute_debloat_pipeline()
                    base_disk, _ = find_base_disk("debloated")

            if base_disk:
                boot_qemu(
                    base_disk=base_disk,
                    kernel_bin=find_kernel("android"),
                    dtb_path=find_dtb(),
                    mode="android",
                    adb_port=status["adb_port"]["port"]
                )
            else:
                print(f"{CLR_RED}[-] No disk image found to boot!{CLR_RESET}")

        elif choice == "2":
            # Boot Stock
            base_disk, _ = find_base_disk("stock")
            if base_disk:
                boot_qemu(
                    base_disk=base_disk,
                    kernel_bin=find_kernel("android"),
                    dtb_path=find_dtb(),
                    mode="android",
                    adb_port=status["adb_port"]["port"]
                )
            else:
                print(f"{CLR_RED}[-] Stock staging/emmc.raw not found!{CLR_RESET}")

        elif choice == "3":
            # Boot Diag
            base_disk, _ = find_base_disk("debloated")
            boot_qemu(
                base_disk=base_disk or str(REPO_ROOT / "staging/emmc.raw"),
                kernel_bin=find_kernel("diag"),
                dtb_path=find_dtb(),
                mode="diag",
                adb_port=status["adb_port"]["port"]
            )

        elif choice == "4":
            # Build Debloated ROM
            execute_debloat_pipeline()

        elif choice == "5":
            # Export USB
            export_usb()

        elif choice == "6":
            # Connect ADB
            connect_adb(port=5555)

        elif choice == "7":
            # Reset Overlay
            base_disk, _ = find_base_disk("debloated")
            if base_disk:
                ensure_cow_overlay(base_disk, force_reset=True)
                print(f"{CLR_GREEN}[✓] Copy-on-Write overlay reset to clean state.{CLR_RESET}")
            else:
                print(f"{CLR_RED}[-] No base disk available to reset overlay for.{CLR_RESET}")

        elif choice == "8":
            # Mount Custom Firmware
            print(f"\n{CLR_CYAN}[Custom Firmware Ingestion]{CLR_RESET}")
            custom_path = input("Enter path to a prepared GPT disk or a system partition: ").strip()
            if custom_path and os.path.exists(custom_path):
                with open(custom_path, "rb") as source:
                    source.seek(512)
                    whole_disk = source.read(8) == b"EFI PART"
                vendor = None if whole_disk else input("Vendor partition path (Enter for staging/vendor.img): ").strip()
                mounted_disk = mount_custom_firmware(custom_system=custom_path, custom_vendor=vendor)
                boot_qemu(
                    base_disk=mounted_disk,
                    kernel_bin=find_kernel("android"),
                    dtb_path=find_dtb(),
                    mode="android",
                    adb_port=status["adb_port"]["port"]
                )
            else:
                print(f"{CLR_RED}[-] Path does not exist.{CLR_RESET}")

        elif choice == "9":
            print_diagnostic_report()
            input("Press Enter to return to menu...")

        elif choice == "0":
            print("Exiting.")
            sys.exit(0)


# ==============================================================================
# 9. Main CLI Entry Point & Flag Parser
# ==============================================================================

def parse_cli_args():
    parser = argparse.ArgumentParser(
        description="OpenDTV RTD2841 / TCL RT41 Unified Turnkey Runner CLI & Firmware Toolkit",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  ./run.sh                              # Auto-detects dependencies and launches interactive menu
  ./run.sh --debloat                    # Run automated debloating pipeline
  ./run.sh --export-usb /Volumes/MY_USB # Export Update.img to USB drive
  ./run.sh --adb                        # Connect ADB shell to running emulator
  ./run.sh --reset                      # Wipe COW overlay and restore clean baseline
  ./run.sh --vnc :0                     # Run with VNC display on port 5900
  ./run.sh --system custom.img          # Mount and boot custom system image
  ./run.sh --check                      # Run system readiness & dependency diagnostics
"""
    )

    parser.add_argument("--system", help="Path to custom system.img, system.tar, or composite raw disk")
    parser.add_argument("--vendor", help="Path to custom vendor.img or vendor.tar")
    parser.add_argument("--update-img", help="Show preparation requirements for a packaged OTA")
    parser.add_argument("--debloat", action="store_true", help="Execute the automated ROM debloating pipeline")
    parser.add_argument("--export-usb", nargs="?", const="", default=None,
                        help="Export Update.img with SHA256 checksum to USB drive or directory")
    parser.add_argument("--adb", action="store_true", help="Connect interactive ADB shell to running emulator")
    parser.add_argument("--vnc", nargs="?", const="0", default=None,
                        help="Enable VNC display backend (e.g. :0 -> port 5900)")
    parser.add_argument("--serial", action="store_true", help="Run interactive serial console on stdio")
    parser.add_argument("--reset", action="store_true", help="Wipe and recreate copy-on-write overlay")
    parser.add_argument("--overlay", help="Explicit path to QCOW2 overlay disk image")
    parser.add_argument("--diag", action="store_true", help="Boot bare-metal diagnostic kernel shell")
    parser.add_argument("--headless", "--no-gui", action="store_true", dest="headless",
                        help="Run headless without graphical window (-display none)")
    parser.add_argument("--display", help="Explicitly specify QEMU display backend (cocoa, gtk, vnc, none)")
    parser.add_argument("--mem", default="2048M", help="Set QEMU guest memory size (default: 2048M)")
    parser.add_argument("--adb-port", type=int, default=None, help="Override ADB host forwarding port (default: 5555)")
    parser.add_argument("--kernel", help="Override path to kernel zImage")
    parser.add_argument("--dtb", help="Override path to device tree blob")
    parser.add_argument("--check", action="store_true", help="Run dependency & environment health check and exit")
    parser.add_argument("--menu", action="store_true", help="Force interactive terminal menu")

    return parser.parse_args()


def main():
    args = parse_cli_args()

    # 1. Diagnostics check flag
    if args.check:
        print_diagnostic_report()
        sys.exit(0)

    # 2. Debloat flag
    if args.debloat:
        success = execute_debloat_pipeline()
        sys.exit(0 if success else 1)

    # 3. Export USB flag
    if args.export_usb is not None:
        target_dest = args.export_usb if args.export_usb else None
        success = export_usb(target_dest)
        sys.exit(0 if success else 1)

    # 4. ADB connection flag
    if args.adb:
        port = args.adb_port or 5555
        connect_adb(port=port)
        sys.exit(0)

    # 5. Standalone Reset flag (clears overlay and exits unless boot/display flags are provided)
    has_boot_flags = any([
        args.system, args.vendor, args.update_img,
        args.diag, args.headless, args.vnc is not None,
        args.display, args.kernel, args.dtb, args.serial
    ])
    if args.reset and not has_boot_flags:
        base_disk, _ = find_base_disk("debloated")
        if not base_disk:
            base_disk, _ = find_base_disk("stock")
        if base_disk:
            overlay_file = ensure_cow_overlay(base_disk, overlay_path=args.overlay, force_reset=True)
            print(f"{CLR_GREEN}[✓] Copy-on-Write overlay successfully reset: {overlay_file}{CLR_RESET}")
            sys.exit(0)
        else:
            print(f"{CLR_RED}[-] No base disk found to reset overlay for.{CLR_RESET}")
            sys.exit(1)

    # 6. Interactive menu if requested or if zero arguments on a TTY
    if args.menu or (not has_boot_flags and sys.stdin.isatty()):
        run_interactive_menu()
        sys.exit(0)

    # 7. Direct execution mode via CLI flags
    status = check_environment()

    # Resolve base disk
    base_disk = ""
    if args.system or args.vendor or args.update_img:
        base_disk = mount_custom_firmware(
            custom_system=args.system,
            custom_vendor=args.vendor,
            custom_update_img=args.update_img
        )
    else:
        preferred = "diag" if args.diag else "debloated"
        base_disk, _ = find_base_disk(preferred)

    if not base_disk:
        print(f"{CLR_RED}[-] Error: No prepared eMMC disk found. Supply --system /path/to/emmc.raw or place it in output/emmc.raw. For the firmware-free demo, run ./scripts/setup-qemu.sh --demo.{CLR_RESET}")
        sys.exit(1)

    # Resolve Kernel & DTB
    kernel_bin = args.kernel or find_kernel("diag" if args.diag else "android")
    dtb_path = args.dtb or find_dtb()
    adb_port = args.adb_port or status["adb_port"]["port"]
    mode = "diag" if args.diag else "android"

    boot_qemu(
        base_disk=base_disk,
        kernel_bin=kernel_bin,
        dtb_path=dtb_path,
        overlay_path=args.overlay,
        mode=mode,
        display_backend=args.display,
        vnc_display=args.vnc,
        adb_port=adb_port,
        mem=args.mem,
        force_reset=args.reset,
        headless=args.headless,
        serial_stdio=True
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        sys.exit(f"OpenDTV: {error}")
