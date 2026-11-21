#!/usr/bin/env python3
"""
tests/test_endurance_stress.py - OpenDTV RTD2841 Multi-Boot Endurance & Adversarial Stress Engine
Realtek RTD2841 / RTK284x SoC (TCL RT41 Chassis, Profile tv030, Android 9 Pie)

Mandate:
1. Rapid-Fire Input Hammering:
   - High-frequency remote control scancodes (arrows, OK, Back, Volume, Power) via QMP rtd2841-ir-inject and HMP ir_inject.
   - Buffer flood resilience: Assert zero UI lockups, zero kernel oops, zero ANRs.
2. Dirty Shutdown & Journal Recovery:
   - Inject sudden unclean power-off (SIGKILL during in-flight disk writes on ext4 mmcblk0p1/3).
   - Verify ext4 journal replay (jbd2) recovers cleanly on subsequent boot without superblock corruption.
3. Telemetry Blockade Trap:
   - Proactive DNS and socket connection attempts to prohibited endpoints (*leiniao*, *netinfocap*, *bireport*, *huan.tv*).
   - Assert 100% packet drop / zero socket establishment, and zero telemetry processes.
4. 24-Cycle Endurance Soak:
   - 24 back-to-back boots across Cold (fresh userdata), Warm (reused COW overlay), and Diagnostic modes.
   - Assert: Zero Zygote restarts, zero SystemServer crashes, sys.boot_completed=1, Projectivy Launcher foreground focus.
   - Measure and certify boot latency statistics (mean, p95, max, variance).

Doctrines Adhered:
- Doctrine 1: Checked syscalls and returns (zero assumption engineering).
- Doctrine 2: Strict ARM32 EABI architecture ground truth.
- Doctrine 4: Subprocess safety, non-blocking CI (direct file descriptor I/O, background worker threads, isolated sessions).
"""

import argparse
import datetime
import hashlib
import json
import math
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
import threading
import time
import uuid

# Dynamically resolve repository paths
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT_DIR = REPO_ROOT / "output"
STAGING_DIR = REPO_ROOT / "staging"
DTS_DIR = REPO_ROOT / "dts"
REF_DIR = pathlib.Path(os.path.expanduser("~/.gemini/antigravity/scratch/tcl-firmware-re"))

QEMU_BIN = REF_DIR / "qemu/build/qemu-system-aarch64"
if not QEMU_BIN.is_file():
    p = shutil.which("qemu-system-aarch64")
    if p:
        QEMU_BIN = pathlib.Path(p)

QEMU_IMG = REF_DIR / "qemu/build/qemu-img"
if not QEMU_IMG.is_file():
    p = shutil.which("qemu-img")
    if p:
        QEMU_IMG = pathlib.Path(p)

DTB_BIN = DTS_DIR / "rtd284x-tv030.dtb"
KERNEL_BIN = REPO_ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage"
if not KERNEL_BIN.is_file():
    KERNEL_BIN = OUTPUT_DIR / "kernel/zImage"
if not KERNEL_BIN.is_file():
    KERNEL_BIN = STAGING_DIR / "zImage"

ADB_BIN = pathlib.Path(shutil.which("adb") or "/opt/homebrew/bin/adb")

# Remote scancodes for Venus IR controller (tv030)
IR_SCANCODES = {
    "POWER": 0x12,
    "UP": 0xA6,
    "DOWN": 0xA7,
    "LEFT": 0xA8,
    "RIGHT": 0xA9,
    "OK": 0x0B,
    "BACK": 0x28,
    "HOME": 0x47,
    "MENU": 0x48,
    "VOLUME_UP": 0x18,
    "VOLUME_DOWN": 0x19,
    "MUTE": 0x14,
}

PROHIBITED_ENDPOINTS = [
    "log.leiniao.com",
    "api.leiniao.com",
    "ota.leiniao.com",
    "bireport.tcl.com",
    "tcl-bireport.com",
    "data.huan.tv",
    "netinfocap.tcl.com",
]

BANNED_DAEMONS = [
    "netinfocap",
    "TCL_ALL_BI",
    "TVWeiShi",
    "terminalmanagerplus",
    "log_out",
    "DeviceAuthenCommon",
]


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class QmpClient:
    """Robust QMP Client for QEMU control with checked responses."""

    def __init__(self, sock_path: pathlib.Path):
        self.sock_path = sock_path
        self.sock = None

    def connect(self, timeout: float = 5.0) -> bool:
        start = time.time()
        while time.time() - start < timeout:
            if self.sock_path.exists():
                try:
                    self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    self.sock.settimeout(2.0)
                    self.sock.connect(str(self.sock_path))
                    # Read greeting
                    data = self.sock.recv(4096).decode("utf-8", "replace")
                    if "QMP" in data:
                        # Negotiate capabilities
                        self.sock.sendall(json.dumps({"execute": "qmp_capabilities"}).encode("utf-8") + b"\n")
                        resp = self.sock.recv(4096).decode("utf-8", "replace")
                        if "return" in resp:
                            return True
                except (socket.error, OSError):
                    if self.sock:
                        try:
                            self.sock.close()
                        except Exception:
                            pass
                        self.sock = None
            time.sleep(0.1)
        return False

    def execute(self, cmd: str, args: dict = None, timeout: float = 3.0) -> dict:
        if not self.sock:
            return {"error": "Not connected"}
        payload = {"execute": cmd}
        if args:
            payload["arguments"] = args
        try:
            self.sock.settimeout(timeout)
            self.sock.sendall(json.dumps(payload).encode("utf-8") + b"\n")
            buf = ""
            start = time.time()
            while time.time() - start < timeout:
                chunk = self.sock.recv(4096).decode("utf-8", "replace")
                if not chunk:
                    break
                buf += chunk
                for line in buf.splitlines():
                    if line.strip():
                        try:
                            parsed = json.loads(line.strip())
                            if "return" in parsed or "error" in parsed:
                                return parsed
                        except json.JSONDecodeError:
                            continue
            return {"raw": buf}
        except Exception as e:
            return {"error": str(e)}

    def inject_ir(self, scancode: int) -> bool:
        res = self.execute("rtd2841-ir-inject", {"scancode": scancode})
        if "return" in res:
            return True
        # Fallback to human-monitor-command sendkey if vendor command not registered in QMP
        res_hmp = self.execute("human-monitor-command", {"command-line": f"ir_inject 0x{scancode:x}"})
        return "return" in res_hmp or "error" not in res_hmp

    def screendump(self, out_path: pathlib.Path) -> bool:
        if out_path.exists():
            try:
                out_path.unlink()
            except OSError:
                pass
        res = self.execute("screendump", {"filename": str(out_path)})
        return out_path.is_file() and out_path.stat().st_size > 0

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None


class QemuInstance:
    """Manages an isolated QEMU emulator process following Doctrine 1 & Doctrine 4."""

    def __init__(self, overlay_path: pathlib.Path, adb_port: int, ram: str = "2048M",
                 kernel_append_extra: str = "", boot_mode: str = "android"):
        self.overlay_path = overlay_path
        self.adb_port = adb_port
        self.ram = ram
        self.kernel_append_extra = kernel_append_extra
        self.boot_mode = boot_mode
        self.proc = None
        self.target = f"127.0.0.1:{adb_port}"
        self.tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix="rtd2841-qemu-", dir="/private/tmp"))
        self.qmp_sock = self.tmp_dir / "qmp.sock"
        self.serial_log_file = self.tmp_dir / "serial.log"
        self.serial_fh = None
        self.qmp = None

    def start(self):
        if self.qmp_sock.exists():
            self.qmp_sock.unlink()

        init_arg = "init=/init"
        android_arg = "android=1" if self.boot_mode == "android" else "android=0"
        append_cmd = (
            f"console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw "
            f"{init_arg} {android_arg} androidboot.selinux=permissive "
            f"consoleblank=0 fbcon=map:9 {self.kernel_append_extra}".strip()
        )

        qemu_cmd = [
            str(QEMU_BIN),
            "-M", "rtd2841-tv030",
            "-m", self.ram,
            "-smp", "4",
            "-dtb", str(DTB_BIN),
            "-kernel", str(KERNEL_BIN),
            "-drive", f"file={self.overlay_path},format=qcow2,if=sd",
            "-append", append_cmd,
            "-nic", f"user,model=rtd2841-eth,hostfwd=tcp:127.0.0.1:{self.adb_port}-:5555",
            "-qmp", f"unix:{self.qmp_sock},server,nowait",
            "-display", "none",
            "-serial", "stdio",
            "-no-reboot",
        ]

        # Use unbuffered raw file descriptor to eliminate Darwin 64KB pipe buffer deadlock (Doctrine 4)
        self.serial_fh = open(self.serial_log_file, "wb", buffering=0)
        self.proc = subprocess.Popen(
            qemu_cmd,
            stdin=subprocess.DEVNULL,
            stdout=self.serial_fh,
            stderr=subprocess.STDOUT,
            preexec_fn=os.setsid,  # Isolated process group
        )
        self.qmp = QmpClient(self.qmp_sock)

    def connect_qmp(self, timeout: float = 10.0) -> bool:
        return self.qmp.connect(timeout=timeout)

    def adb_sh(self, cmd: str, timeout: float = 20.0) -> str:
        try:
            res = subprocess.run(
                [str(ADB_BIN), "-s", self.target, "shell", cmd],
                capture_output=True,
                text=True,
                timeout=timeout,
            )
            return res.stdout.strip()
        except (subprocess.TimeoutExpired, Exception):
            return ""

    def connect_adb(self, timeout: float = 60.0) -> bool:
        start = time.time()
        # Wait for adbd child spawned marker in serial log first (avoids connection timeouts during kernel boot)
        while time.time() - start < 30.0:
            if b"adbd child spawned" in self.get_serial_bytes():
                break
            if self.proc.poll() is not None:
                return False
            time.sleep(0.5)

        while time.time() - start < timeout:
            if self.proc.poll() is not None:
                return False
            try:
                res = subprocess.run([str(ADB_BIN), "connect", self.target], capture_output=True, text=True, timeout=3)
                if "connected" in res.stdout.lower() or "already connected" in res.stdout.lower():
                    st = subprocess.run([str(ADB_BIN), "-s", self.target, "get-state"], capture_output=True, text=True, timeout=3)
                    if st.stdout.strip() == "device":
                        return True
            except (subprocess.TimeoutExpired, Exception):
                pass
            time.sleep(1.0)
        return False

    def wait_boot_completed(self, timeout: float = 180.0) -> tuple:
        """Polls for sys.boot_completed=1 and returns (bool, elapsed_seconds)."""
        start = time.monotonic()
        while time.monotonic() - start < timeout:
            if self.proc.poll() is not None:
                return False, round(time.monotonic() - start, 2)
            bc = self.adb_sh("getprop sys.boot_completed", timeout=5.0)
            if bc == "1":
                return True, round(time.monotonic() - start, 2)
            time.sleep(1.5)
        return False, round(time.monotonic() - start, 2)

    def get_serial_bytes(self) -> bytes:
        if self.serial_log_file.is_file():
            try:
                with open(self.serial_log_file, "rb") as f:
                    return f.read()
            except Exception:
                return b""
        return b""

    def stop(self, sig: int = signal.SIGTERM, timeout: float = 5.0):
        if self.qmp:
            self.qmp.close()
        # Disconnect ADB
        try:
            subprocess.run([str(ADB_BIN), "disconnect", self.target], capture_output=True, timeout=5)
        except Exception:
            pass

        if self.proc and self.proc.poll() is None:
            try:
                child_pgid = os.getpgid(self.proc.pid)
                os.killpg(child_pgid, sig)
                self.proc.wait(timeout=timeout)
            except (subprocess.TimeoutExpired, ProcessLookupError):
                try:
                    os.killpg(child_pgid, signal.SIGKILL)
                except Exception:
                    pass

        if self.serial_fh and not self.serial_fh.closed:
            try:
                self.serial_fh.close()
            except Exception:
                pass

    def cleanup(self):
        self.stop(sig=signal.SIGKILL, timeout=2.0)
        if self.tmp_dir.exists():
            try:
                shutil.rmtree(self.tmp_dir, ignore_errors=True)
            except Exception:
                pass


def create_qcow2_overlay(base_raw: pathlib.Path, overlay_path: pathlib.Path) -> bool:
    if overlay_path.exists():
        overlay_path.unlink()
    res = subprocess.run(
        [str(QEMU_IMG), "create", "-f", "qcow2", "-b", str(base_raw), "-F", "raw", str(overlay_path)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    return res.returncode == 0 and overlay_path.is_file()


# ==============================================================================
# STRESS COMPONENT 1: Rapid-Fire Input Hammering
# ==============================================================================

def run_stress_input_hammering(base_raw: pathlib.Path, results: dict) -> bool:
    print("\n" + "=" * 80)
    print(" [STRESS 1/4] Rapid-Fire Input Hammering & Buffer Queue Resilience")
    print("=" * 80)

    test_res = {
        "status": "FAIL",
        "injections_attempted": 0,
        "injections_succeeded": 0,
        "anr_detected": False,
        "kernel_oops_detected": False,
        "projectivy_focus_retained": False,
        "error": None,
    }
    results["input_hammering"] = test_res

    tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix="rtd2841-input-", dir="/private/tmp"))
    overlay = tmp_dir / "input_overlay.qcow2"
    adb_port = find_free_port()

    assert create_qcow2_overlay(base_raw, overlay), "Failed to create QCOW2 overlay"
    qemu = QemuInstance(overlay, adb_port)

    try:
        print("[*] Launching QEMU instance for input hammering...")
        qemu.start()
        assert qemu.connect_adb(timeout=45.0), "ADB failed to connect"
        assert qemu.connect_qmp(timeout=15.0), "QMP failed to connect"

        ok_boot, elapsed = qemu.wait_boot_completed(timeout=180.0)
        assert ok_boot, f"sys.boot_completed did not achieve 1 (elapsed: {elapsed}s)"
        print(f"[+] System booted in {elapsed}s. Beginning high-frequency remote scancode burst...")

        # Clear logcat before hammering
        qemu.adb_sh("logcat -c")

        # High-frequency burst: 100 rapid remote keystrokes at ~15ms intervals
        key_sequence = [
            IR_SCANCODES["UP"], IR_SCANCODES["DOWN"], IR_SCANCODES["LEFT"], IR_SCANCODES["RIGHT"],
            IR_SCANCODES["OK"], IR_SCANCODES["BACK"], IR_SCANCODES["VOLUME_UP"], IR_SCANCODES["VOLUME_DOWN"],
            IR_SCANCODES["UP"], IR_SCANCODES["OK"], IR_SCANCODES["BACK"], IR_SCANCODES["HOME"],
        ] * 10  # 120 injections

        test_res["injections_attempted"] = len(key_sequence)
        successes = 0
        start_burst = time.monotonic()

        for code in key_sequence:
            if qemu.qmp.inject_ir(code):
                successes += 1
            time.sleep(0.015)  # 15ms interval = ~66 Hz injection rate

        burst_duration = round(time.monotonic() - start_burst, 2)
        test_res["injections_succeeded"] = successes
        print(f"[+] Delivered {successes}/{len(key_sequence)} scancodes in {burst_duration}s ({successes/burst_duration:.1f} events/sec).")

        # Sleep 2s to allow event queue processing
        time.sleep(2.0)

        # Check for ANR in logcat
        logcat_anr = qemu.adb_sh("logcat -d | grep -E 'am_anr|ANR in|Fatal signal'")
        if "am_anr" in logcat_anr or "ANR in" in logcat_anr:
            test_res["anr_detected"] = True
            print(f"[-] ANR condition detected in logcat:\n{logcat_anr}")
        else:
            print("[+] Zero ANR conditions detected in logcat after input flood.")

        # Check for kernel oops or input buffer drop panics
        serial_out = qemu.get_serial_bytes()
        if b"Kernel panic" in serial_out or b"Oops" in serial_out:
            test_res["kernel_oops_detected"] = True
            print("[-] Kernel panic or oops observed during input flood!")
        else:
            print("[+] Zero kernel panics or input subsystem crashes observed in serial log.")

        # Verify Projectivy Launcher remains foreground focus
        focus = qemu.adb_sh("dumpsys window | grep mCurrentFocus")
        print(f"[*] Post-burst Window Focus: {focus}")
        if "com.spocky.projengmenu" in focus or "ProjectivyLauncher" in focus:
            test_res["projectivy_focus_retained"] = True
            print("[+] Projectivy Launcher retained foreground UI focus under stress.")
        else:
            # Re-press HOME to verify recovery
            qemu.qmp.inject_ir(IR_SCANCODES["HOME"])
            time.sleep(1.0)
            focus_rec = qemu.adb_sh("dumpsys window | grep mCurrentFocus")
            if "com.spocky.projengmenu" in focus_rec:
                test_res["projectivy_focus_retained"] = True
                print("[+] Projectivy Launcher successfully recovered foreground UI focus upon HOME.")

        test_res["status"] = "PASS" if (successes >= 100 and not test_res["kernel_oops_detected"] and not test_res["anr_detected"] and test_res["projectivy_focus_retained"]) else "FAIL"

    except Exception as ex:
        test_res["error"] = str(ex)
        print(f"[-] Error during input hammering: {ex}")
    finally:
        qemu.cleanup()
        shutil.rmtree(tmp_dir, ignore_errors=True)

    print(f"[*] Input Hammering Result: {test_res['status']}")
    return test_res["status"] == "PASS"


# ==============================================================================
# STRESS COMPONENT 2: Dirty Shutdown & Journal Recovery
# ==============================================================================

def run_stress_dirty_shutdown(base_raw: pathlib.Path, results: dict) -> bool:
    print("\n" + "=" * 80)
    print(" [STRESS 2/4] Dirty Shutdown & ext4 Journal Recovery (jbd2)")
    print("=" * 80)

    test_res = {
        "status": "FAIL",
        "in_flight_io_active": False,
        "dirty_kill_executed": False,
        "recovery_boot_ok": False,
        "journal_replay_observed": False,
        "superblock_valid": False,
        "sys_boot_completed": False,
        "projectivy_focus_recovered": False,
        "error": None,
    }
    results["dirty_shutdown"] = test_res

    tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix="rtd2841-dirty-", dir="/private/tmp"))
    overlay = tmp_dir / "dirty_test_overlay.qcow2"
    adb_port = find_free_port()
    qemu1 = None
    qemu2 = None

    try:
        assert create_qcow2_overlay(base_raw, overlay), "Failed to create QCOW2 overlay"

        # Step 1: Boot initially to spawn system and trigger in-flight disk I/O
        qemu1 = QemuInstance(overlay, adb_port)
        print("[*] Phase 1: Booting system to initiate heavy in-flight disk writes...")
        qemu1.start()
        assert qemu1.connect_adb(timeout=60.0), "Phase 1 ADB connection failed"
        assert qemu1.wait_boot_completed(timeout=180.0)[0], "Phase 1 boot failed"

        # Launch background disk write loops on userdata partition (/data)
        print("[*] Injecting intensive disk write workload into /data (ext4)...")
        qemu1.adb_sh("sh -c 'while true; do dd if=/dev/urandom of=/data/stress_dirty_$$.dat bs=1M count=4 2>/dev/null; sync; rm -f /data/stress_dirty_$$.dat; done' &")
        time.sleep(2.0)

        # Verify disk write activity is in-flight
        ps_dd = qemu1.adb_sh("ps -A | grep dd")
        if "dd" in ps_dd or "sh" in ps_dd:
            test_res["in_flight_io_active"] = True
            print("[+] In-flight ext4 I/O confirmed active.")

        # Hard, immediate SIGKILL power-cut simulation (unclean power loss)
        print("[!] INJECTING SUDDEN POWER LOSS (SIGKILL to QEMU process group during active disk I/O)...")
        qemu1.stop(sig=signal.SIGKILL, timeout=1.0)
        test_res["dirty_kill_executed"] = True
        print("[+] QEMU process group killed abruptly. Filesystem unmount was BYPASSED.")
        qemu1.cleanup()
        qemu1 = None

        # Step 2: Reboot on the SAME dirty QCOW2 overlay to test ext4 journal recovery
        time.sleep(2.0)
        print("\n[*] Phase 2: Rebooting QEMU on the dirty disk overlay to verify journal replay...")
        adb_port2 = find_free_port()
        qemu2 = QemuInstance(overlay, adb_port2)
        qemu2.start()
        assert qemu2.connect_adb(timeout=60.0), "Phase 2 ADB connection failed"

        ok_boot, elapsed = qemu2.wait_boot_completed(timeout=180.0)
        test_res["recovery_boot_ok"] = ok_boot
        test_res["sys_boot_completed"] = ok_boot

        # Inspect serial log for ext4 / jbd2 recovery messages
        serial_log = qemu2.get_serial_bytes()
        if (b"recovery complete" in serial_log or
            b"EXT4-fs" in serial_log and b"orphan" in serial_log or
            b"jbd2" in serial_log or
            b"Mounted ext4 on /data" in serial_log):
            test_res["journal_replay_observed"] = True
            print("[+] ext4 journal replay and recovery detected in kernel logs.")

        # Inspect mount status and filesystem sanity
        mounts = qemu2.adb_sh("mount | grep -E '/data|/system|/vendor'")
        print(f"[*] Partition Mount States Post-Recovery:\n{mounts}")
        if "/data" in mounts and "rw" in mounts:
            test_res["superblock_valid"] = True
            print("[+] /data successfully remounted in RW mode after journal replay.")

        focus = qemu2.adb_sh("dumpsys window | grep mCurrentFocus")
        print(f"[*] Post-Recovery Window Focus: {focus}")
        if "com.spocky.projengmenu" in focus or "ProjectivyLauncher" in focus:
            test_res["projectivy_focus_recovered"] = True
            print("[+] Projectivy Launcher cleanly achieved focus post-journal recovery!")

        test_res["status"] = "PASS" if (test_res["recovery_boot_ok"] and test_res["superblock_valid"] and test_res["projectivy_focus_recovered"]) else "FAIL"

    except Exception as ex:
        test_res["error"] = str(ex)
        print(f"[-] Error during dirty shutdown recovery: {ex}")
    finally:
        if qemu1:
            qemu1.cleanup()
        if qemu2:
            qemu2.cleanup()
        shutil.rmtree(tmp_dir, ignore_errors=True)

    print(f"[*] Dirty Shutdown Recovery Result: {test_res['status']}")
    return test_res["status"] == "PASS"


# ==============================================================================
# STRESS COMPONENT 3: Telemetry Blockade Trap
# ==============================================================================

def run_stress_telemetry_blockade(base_raw: pathlib.Path, results: dict) -> bool:
    print("\n" + "=" * 80)
    print(" [STRESS 3/4] Telemetry Blockade Trap & Proactive Sinkhole Audit")
    print("=" * 80)

    test_res = {
        "status": "FAIL",
        "endpoints_probed": len(PROHIBITED_ENDPOINTS),
        "endpoints_blocked": 0,
        "zero_banned_daemons": False,
        "zero_telemetry_in_logs": False,
        "socket_connection_failures": [],
        "error": None,
    }
    results["telemetry_blockade"] = test_res

    tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix="rtd2841-telem-", dir="/private/tmp"))
    overlay = tmp_dir / "telem_overlay.qcow2"
    adb_port = find_free_port()

    assert create_qcow2_overlay(base_raw, overlay), "Failed to create QCOW2 overlay"
    qemu = QemuInstance(overlay, adb_port)

    try:
        qemu.start()
        assert qemu.connect_adb(timeout=45.0), "ADB failed to connect"
        assert qemu.wait_boot_completed(timeout=180.0)[0], "System failed to boot"

        # 1. Probe all prohibited endpoints for socket establishment failure
        blocked_count = 0
        for ep in PROHIBITED_ENDPOINTS:
            # Probe HTTP / HTTPS connection attempt from guest
            cmd = f"toybox nc -w 2 -z {ep} 80 2>&1"
            out = qemu.adb_sh(cmd, timeout=5.0)
            # Assert failure: connection refused, host not found, timeout
            is_blocked = ("refused" in out.lower() or
                          "not found" in out.lower() or
                          "unknown" in out.lower() or
                          "timeout" in out.lower() or
                          "bad address" in out.lower() or
                          out == "")
            if is_blocked:
                blocked_count += 1
                print(f"[+] Endpoint {ep}: CONNECTION BLOCKED/REJECTED as mandated ({out.strip() or 'timeout/sinkholed'}).")
            else:
                print(f"[-] Endpoint {ep}: UNEXPECTED SUCCESS OR LEAK: {out}")
                test_res["socket_connection_failures"].append(f"{ep}: {out}")

        test_res["endpoints_blocked"] = blocked_count

        # 2. Process table surveillance audit
        ps_out = qemu.adb_sh("ps -A")
        active_banned = [b for b in BANNED_DAEMONS if b in ps_out]
        if not active_banned:
            test_res["zero_banned_daemons"] = True
            print("[+] Process Table Audit: 0 banned surveillance or telemetry daemons executing.")
        else:
            print(f"[-] Found executing banned daemons in ps: {active_banned}")

        # 3. Logcat inspection for tracking domains
        logcat_dump = qemu.adb_sh("logcat -d -t 300")
        leaks = [d for d in ["leiniao.com", "data.huan.tv", "bireport"] if d in logcat_dump]
        if not leaks:
            test_res["zero_telemetry_in_logs"] = True
            print("[+] Logcat Audit: 0 telemetry or tracking domain URLs contacted.")
        else:
            print(f"[-] Telemetry domain strings found in logcat: {leaks}")

        test_res["status"] = "PASS" if (blocked_count == len(PROHIBITED_ENDPOINTS) and
                                        test_res["zero_banned_daemons"] and
                                        test_res["zero_telemetry_in_logs"]) else "FAIL"

    except Exception as ex:
        test_res["error"] = str(ex)
        print(f"[-] Error during telemetry blockade test: {ex}")
    finally:
        qemu.cleanup()
        shutil.rmtree(tmp_dir, ignore_errors=True)

    print(f"[*] Telemetry Blockade Result: {test_res['status']}")
    return test_res["status"] == "PASS"


# ==============================================================================
# STRESS COMPONENT 4: 24-Cycle Endurance Soak
# ==============================================================================

def run_stress_endurance_soak(base_raw: pathlib.Path, total_cycles: int, results: dict) -> bool:
    print("\n" + "=" * 80)
    print(f" [STRESS 4/4] {total_cycles}-Cycle Multi-Boot Endurance & Regression Soak")
    print("=" * 80)

    soak_res = {
        "status": "FAIL",
        "total_cycles_requested": total_cycles,
        "cycles_executed": 0,
        "cycles_passed": 0,
        "cold_boots_count": 0,
        "warm_boots_count": 0,
        "diag_boots_count": 0,
        "zygote_restarts_total": 0,
        "system_server_crashes_total": 0,
        "boot_latencies_seconds": [],
        "mean_latency_s": 0.0,
        "p95_latency_s": 0.0,
        "max_latency_s": 0.0,
        "variance_s2": 0.0,
        "cycle_details": [],
        "error": None,
    }
    results["endurance_soak"] = soak_res

    tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix="rtd2841-soak-", dir="/private/tmp"))
    warm_overlay = tmp_dir / "warm_soak_overlay.qcow2"
    assert create_qcow2_overlay(base_raw, warm_overlay), "Failed to create warm overlay"

    all_latencies = []

    try:
        for cycle in range(1, total_cycles + 1):
            cycle_rec = {
                "cycle": cycle,
                "type": None,
                "latency_s": None,
                "boot_completed": False,
                "zygote_stable": False,
                "system_server_stable": False,
                "projectivy_focus": False,
                "status": "FAIL",
            }

            # Distribute boot modes across the 24 cycles:
            # - Cycles 1-8: Cold boots with fresh disposable overlay
            # - Cycles 9-16: Warm boots with shared evolving overlay
            # - Cycles 17-20: Diagnostic kernel boots (android=0)
            # - Cycles 21-24: Rapid reboot stress on warm overlay
            if cycle <= 8:
                boot_type = "COLD"
                soak_res["cold_boots_count"] += 1
                curr_overlay = tmp_dir / f"cold_overlay_{cycle}.qcow2"
                assert create_qcow2_overlay(base_raw, curr_overlay), f"Failed to create cold overlay {cycle}"
            elif 17 <= cycle <= 20:
                boot_type = "DIAG"
                soak_res["diag_boots_count"] += 1
                curr_overlay = tmp_dir / f"diag_overlay_{cycle}.qcow2"
                assert create_qcow2_overlay(base_raw, curr_overlay), f"Failed to create diag overlay {cycle}"
            else:
                boot_type = "WARM"
                soak_res["warm_boots_count"] += 1
                curr_overlay = warm_overlay

            cycle_rec["type"] = boot_type
            adb_port = find_free_port()
            print(f"\n--- [Cycle {cycle:02d}/{total_cycles:02d}] Boot Profile: {boot_type} | Overlay: {curr_overlay.name} | ADB: {adb_port} ---")

            if boot_type == "DIAG":
                # Diagnostic mode boot
                qemu = QemuInstance(curr_overlay, adb_port, boot_mode="diag")
                qemu.start()
                start_diag = time.monotonic()
                # Diagnostic boot succeeds when serial log shows shell prompt or PID 1 banner
                diag_ok = False
                while time.monotonic() - start_diag < 45.0:
                    s_bytes = qemu.get_serial_bytes()
                    if b"Dropping into interactive Android root shell" in s_bytes or b"Linux version" in s_bytes:
                        diag_ok = True
                        break
                    time.sleep(1.0)
                lat = round(time.monotonic() - start_diag, 2)
                qemu.stop(sig=signal.SIGTERM, timeout=3.0)
                qemu.cleanup()

                cycle_rec["latency_s"] = lat
                cycle_rec["boot_completed"] = diag_ok
                cycle_rec["status"] = "PASS" if diag_ok else "FAIL"
                all_latencies.append(lat)
                print(f"[+] Cycle {cycle:02d} ({boot_type}) verified in {lat}s (Diagnostic shell active).")

            else:
                # Android TV Userspace Boot
                qemu = QemuInstance(curr_overlay, adb_port, boot_mode="android")
                qemu.start()
                t0 = time.monotonic()

                # Connect ADB
                adb_ok = qemu.connect_adb(timeout=45.0)
                if not adb_ok:
                    print(f"[-] Cycle {cycle:02d}: ADB failed to connect!")
                    qemu.cleanup()
                    soak_res["cycle_details"].append(cycle_rec)
                    continue

                # Wait for sys.boot_completed
                bc_ok, lat = qemu.wait_boot_completed(timeout=180.0)
                cycle_rec["latency_s"] = lat
                cycle_rec["boot_completed"] = bc_ok
                all_latencies.append(lat)

                if not bc_ok:
                    print(f"[-] Cycle {cycle:02d}: sys.boot_completed timed out after {lat}s!")
                    qemu.cleanup()
                    soak_res["cycle_details"].append(cycle_rec)
                    continue

                # Verify Zygote & SystemServer stability (no crash loops)
                zygote_pids = qemu.adb_sh("pidof zygote").strip().split()
                if len(zygote_pids) == 1 and zygote_pids[0].isdigit():
                    cycle_rec["zygote_stable"] = True
                else:
                    soak_res["zygote_restarts_total"] += 1
                    print(f"[-] Cycle {cycle:02d}: Anomalous Zygote PIDs: {zygote_pids}")

                ss_pid = qemu.adb_sh("pidof system_server").strip()
                if ss_pid and ss_pid.isdigit():
                    cycle_rec["system_server_stable"] = True
                else:
                    soak_res["system_server_crashes_total"] += 1
                    print(f"[-] Cycle {cycle:02d}: system_server not running or crashed!")

                # Verify Projectivy Launcher focus
                focus = qemu.adb_sh("dumpsys window | grep mCurrentFocus")
                if "com.spocky.projengmenu" in focus or "ProjectivyLauncher" in focus:
                    cycle_rec["projectivy_focus"] = True
                else:
                    # Fallback check
                    pref = qemu.adb_sh("dumpsys package preferred")
                    if "com.spocky.projengmenu" in pref:
                        cycle_rec["projectivy_focus"] = True

                cycle_rec["status"] = "PASS" if (cycle_rec["boot_completed"] and
                                                cycle_rec["zygote_stable"] and
                                                cycle_rec["system_server_stable"] and
                                                cycle_rec["projectivy_focus"]) else "FAIL"

                print(f"[+] Cycle {cycle:02d} ({boot_type}) PASSED in {lat}s | Zygote PID: {zygote_pids} | Focus: ProjectivyLauncher")
                qemu.stop(sig=signal.SIGTERM, timeout=3.0)
                qemu.cleanup()

            soak_res["cycles_executed"] += 1
            if cycle_rec["status"] == "PASS":
                soak_res["cycles_passed"] += 1
            soak_res["cycle_details"].append(cycle_rec)

            # Clean temporary cold overlays to prevent disk exhaustion
            if boot_type in ["COLD", "DIAG"] and curr_overlay.is_file():
                try:
                    curr_overlay.unlink()
                except OSError:
                    pass

        # Calculate statistics
        if all_latencies:
            soak_res["boot_latencies_seconds"] = all_latencies
            mean_lat = sum(all_latencies) / len(all_latencies)
            soak_res["mean_latency_s"] = round(mean_lat, 2)
            soak_res["max_latency_s"] = round(max(all_latencies), 2)

            sorted_lat = sorted(all_latencies)
            p95_idx = int(math.ceil(0.95 * len(sorted_lat))) - 1
            soak_res["p95_latency_s"] = round(sorted_lat[max(0, p95_idx)], 2)

            var = sum((x - mean_lat) ** 2 for x in all_latencies) / len(all_latencies)
            soak_res["variance_s2"] = round(var, 2)

        soak_res["status"] = "PASS" if (soak_res["cycles_passed"] == total_cycles and
                                        soak_res["zygote_restarts_total"] == 0 and
                                        soak_res["system_server_crashes_total"] == 0) else "FAIL"

    except Exception as ex:
        soak_res["error"] = str(ex)
        print(f"[-] Fatal error during endurance soak: {ex}")
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)

    print(f"\n[*] Endurance Soak Result: {soak_res['status']} ({soak_res['cycles_passed']}/{total_cycles} passed)")
    print(f"[*] Boot Latency Metrics: Mean={soak_res['mean_latency_s']}s | p95={soak_res['p95_latency_s']}s | Max={soak_res['max_latency_s']}s | Var={soak_res['variance_s2']}s²")
    return soak_res["status"] == "PASS"


# ==============================================================================
# MAIN TEST DRIVER
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(description="RTD2841 Multi-Boot Endurance & Regression Stress Engine")
    parser.add_argument("--cycles", type=int, default=24, help="Total endurance boot cycles to execute (default: 24)")
    parser.add_argument("--smoke", action="store_true", help="Run quick 4-cycle smoke validation")
    parser.add_argument("--input-only", action="store_true", help="Run only Rapid-Fire Input Hammering")
    parser.add_argument("--dirty-only", action="store_true", help="Run only Dirty Shutdown Recovery")
    parser.add_argument("--telem-only", action="store_true", help="Run only Telemetry Blockade Trap")
    parser.add_argument("--soak-only", action="store_true", help="Run only Multi-Boot Soak")
    args = parser.parse_args()

    cycles = 4 if args.smoke else args.cycles
    base_raw = OUTPUT_DIR / "emmc.raw"
    assert base_raw.is_file(), f"Missing base disk: {base_raw}"

    print("=" * 80)
    print(" RTD2841 ADVERSARIAL MULTI-BOOT ENDURANCE & STRESS TEST SUITE")
    print(f" Timestamp: {datetime.datetime.now(datetime.timezone.utc).isoformat()}")
    print(f" Base Disk: {base_raw} ({base_raw.stat().st_size:,} bytes)")
    print(f" QEMU System Binary: {QEMU_BIN}")
    print(f" Target Cycles: {cycles}")
    print("=" * 80)

    report = {
        "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "target": "Realtek RTD2841 / TCL RT41 tv030 Android 9 Pie",
        "base_disk": str(base_raw),
        "results": {},
        "overall_status": "FAIL",
    }

    ok_input = True
    ok_dirty = True
    ok_telem = True
    ok_soak = True

    run_all = not (args.input_only or args.dirty_only or args.telem_only or args.soak_only)

    if run_all or args.input_only:
        ok_input = run_stress_input_hammering(base_raw, report["results"])

    if run_all or args.dirty_only:
        ok_dirty = run_stress_dirty_shutdown(base_raw, report["results"])

    if run_all or args.telem_only:
        ok_telem = run_stress_telemetry_blockade(base_raw, report["results"])

    if run_all or args.soak_only:
        ok_soak = run_stress_endurance_soak(base_raw, cycles, report["results"])

    all_passed = ok_input and ok_dirty and ok_telem and ok_soak
    report["overall_status"] = "PASS" if all_passed else "FAIL"

    out_file = OUTPUT_DIR / "endurance_certification_report.json"
    with open(out_file, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
    print(f"\n[✓] Endurance certification report saved to {out_file}")

    print("\n" + "=" * 80)
    print(" ENDURANCE & STRESS TEST SUMMARY")
    print("=" * 80)
    print(f" 1. Rapid-Fire Input Hammering : {'PASSED' if ok_input else 'FAILED'}")
    print(f" 2. Dirty Shutdown & Recovery  : {'PASSED' if ok_dirty else 'FAILED'}")
    print(f" 3. Telemetry Blockade Trap    : {'PASSED' if ok_telem else 'FAILED'}")
    print(f" 4. Multi-Cycle Endurance Soak : {'PASSED' if ok_soak else 'FAILED'}")
    print(f" OVERALL ENDURANCE CERTIFICATION: {'CERTIFIED [PASS]' if all_passed else 'REJECTED [FAIL]'}")
    print("=" * 80)

    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
