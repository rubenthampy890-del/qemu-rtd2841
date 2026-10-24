#!/usr/bin/env python3
"""
tests/run_ci_test.py - Automated Headless Hardware Verification Runner for CI
Target: Realtek RTD2841 / TCL RT41 Chassis (tv030)
"""
import subprocess
import sys
import os
import time
import selectors
import signal

try:
    import fcntl
    for fd in (sys.stdout.fileno(), sys.stderr.fileno()):
        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        if flags & os.O_NONBLOCK:
            fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)
except Exception:
    pass

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def find_qemu():
    if "QEMU_BIN" in os.environ and os.path.isfile(os.environ["QEMU_BIN"]) and os.access(os.environ["QEMU_BIN"], os.X_OK):
        return os.environ["QEMU_BIN"]
    cands = [
        "/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64",
        os.path.expanduser("~/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64"),
        os.path.join(REPO_ROOT, "qemu-upstream/build/qemu-system-aarch64"),
        os.path.join(REPO_ROOT, "qemu/build/qemu-system-aarch64"),
        os.path.join(REPO_ROOT, "../tcl-firmware-re/qemu/build/qemu-system-aarch64"),
    ]
    for c in cands:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    # Fallback to system PATH
    import shutil
    p = shutil.which("qemu-system-aarch64")
    if p:
        return p
    return None

def find_kernel():
    if "KERNEL_BIN" in os.environ and os.path.isfile(os.environ["KERNEL_BIN"]):
        return os.environ["KERNEL_BIN"]
    cands = [
        os.path.join(REPO_ROOT, "output/kernel/zImage"),
        os.path.join(REPO_ROOT, "../tcl-firmware-re/output/kernel/zImage"),
    ]
    for c in cands:
        if os.path.isfile(c):
            return c
    return None

def find_storage():
    if "STORAGE_IMAGE" in os.environ and os.path.isfile(os.environ["STORAGE_IMAGE"]):
        return os.environ["STORAGE_IMAGE"]
    cands = [
        os.path.join(REPO_ROOT, "staging/emmc.raw"),
        os.path.join(REPO_ROOT, "storage.img"),
        os.path.join(REPO_ROOT, "../tcl-firmware-re/staging/emmc.raw"),
    ]
    for c in cands:
        if os.path.isfile(c):
            return c
    return None

def find_available_port(preferred=5555):
    import socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(("127.0.0.1", preferred))
            return preferred
        except OSError:
            pass
    for p in range(5556, 5576):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                s.bind(("127.0.0.1", p))
                return p
            except OSError:
                continue
    return None

current_proc = None

def sig_handler(signum, frame):
    global current_proc
    if current_proc and current_proc.poll() is None:
        try:
            os.killpg(os.getpgid(current_proc.pid), signal.SIGKILL)
        except Exception:
            pass
    sys.exit(128 + signum)

try:
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)
except Exception:
    pass

def safe_print(s):
    try:
        sys.stdout.write(s)
        sys.stdout.flush()
    except (BlockingIOError, OSError):
        pass

def run_test(name, extra_args, pass_markers, kernel_bin=None, timeout_sec=10.0):
    global current_proc
    qemu_bin = find_qemu()
    if not qemu_bin:
        print("[-] Error: qemu-system-aarch64 binary not found.", file=sys.stderr)
        return False

    dtb = os.path.join(REPO_ROOT, "dts/rtd284x-tv030.dtb")
    if not kernel_bin:
        kernel_bin = os.path.join(REPO_ROOT, "tests/firmware_test.bin")

    cmd = [
        qemu_bin,
        "-M", "rtd2841-tv030",
        "-m", "1024M",
        "-smp", "4",
        "-dtb", dtb,
        "-display", "none",
        "-serial", "stdio",
        "-kernel", kernel_bin,
    ] + extra_args

    print(f"\n========================================================")
    print(f"[*] Running CI Test: {name}")
    print(f"[*] Command: {' '.join(cmd)}")
    print(f"========================================================")

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        preexec_fn=os.setsid,
    )
    current_proc = proc

    # Set stdout pipe to non-blocking mode
    os.set_blocking(proc.stdout.fileno(), False)

    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ)

    if isinstance(pass_markers, (list, tuple)):
        required_markers = list(pass_markers)
    else:
        required_markers = [pass_markers]
    remaining_markers = list(required_markers)

    passed = False
    start_time = time.time()
    line_buf = ""

    output_lines = []
    try:
        while True:
            now = time.time()
            elapsed = now - start_time
            if elapsed > timeout_sec:
                print(f"\n[-] Test timed out after {timeout_sec:.1f}s!", file=sys.stderr)
                break

            remaining = timeout_sec - elapsed
            events = sel.select(timeout=max(0.01, min(remaining, 0.2)))

            if events:
                try:
                    chunk = os.read(proc.stdout.fileno(), 4096)
                except (BlockingIOError, InterruptedError):
                    continue
                except OSError:
                    break

                if not chunk:
                    # Pipe closed / EOF: break out to drain remaining line_buf
                    break

                text = chunk.decode("utf-8", errors="replace")
                line_buf += text
                while "\n" in line_buf:
                    line, line_buf = line_buf.split("\n", 1)
                    full_line = line + "\n"
                    output_lines.append(full_line)
                    safe_print(full_line)
                    to_remove = [m for m in remaining_markers if m in full_line]
                    for m in to_remove:
                        remaining_markers.remove(m)
                    if not remaining_markers:
                        passed = True
                        break
                if passed:
                    break
            else:
                # Timeout on selector; check if process terminated
                if proc.poll() is not None:
                    break

        # Drain any remaining bytes from pipe buffer and process line_buf
        try:
            while True:
                chunk = os.read(proc.stdout.fileno(), 4096)
                if not chunk:
                    break
                line_buf += chunk.decode("utf-8", errors="replace")
        except (BlockingIOError, OSError):
            pass

        while "\n" in line_buf:
            line, line_buf = line_buf.split("\n", 1)
            full_line = line + "\n"
            output_lines.append(full_line)
            safe_print(full_line)
            to_remove = [m for m in remaining_markers if m in full_line]
            for m in to_remove:
                remaining_markers.remove(m)

        if line_buf:
            output_lines.append(line_buf + "\n")
            safe_print(line_buf + "\n")
            to_remove = [m for m in remaining_markers if m in line_buf]
            for m in to_remove:
                remaining_markers.remove(m)

        if not remaining_markers:
            passed = True

    finally:
        try:
            sel.unregister(proc.stdout)
        except Exception:
            pass
        sel.close()
        if proc.poll() is None:
            try:
                pgid = os.getpgid(proc.pid)
                os.killpg(pgid, signal.SIGTERM)
                proc.wait(timeout=2)
            except (ProcessLookupError, OSError):
                pass
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(pgid, signal.SIGKILL)
                    proc.wait(timeout=1)
                except Exception:
                    pass
        current_proc = None

    if passed:
        print(f"[+] CI TEST PASSED: {name}\n")
        return True
    else:
        missing = [m for m in required_markers if m in remaining_markers]
        print(f"[-] CI TEST FAILED: {name} (missing markers: {missing})\n", file=sys.stderr)
        return False

def main():
    success = True

    # Test 1: Tier 1 - Core SoC Diagnostics (Bare-metal silicon verification with SLIRP)
    t1_markers = [
        "Active up-count verified",
        "Hardware IRQ trap (INTID 34) delivered and acknowledged",
        "Packet TX/RX verified via SLIRP backend",
        "ALL HARDWARE SUBSYSTEM VERIFICATION TESTS PASSED",
    ]
    t1 = run_test(
        "Tier 1: Core SoC Diagnostics (CPU, GIC, TC0 IRQ, SDHCI, RTL8168 SLIRP)",
        ["-nic", "user,model=rtd2841-eth"],
        t1_markers,
    )
    if not t1:
        success = False

    # Test 2: Tier 2 - SDHCI eMMC Storage Media Insertion Test with virtual drive attached
    dummy_img = os.path.join(REPO_ROOT, "tests/ci_storage_test.raw")
    try:
        with open(dummy_img, "wb") as f:
            f.write(b"\x00" * (1024 * 1024 * 16)) # 16 MB dummy raw disk
        t2_markers = [
            "media present",
            "PRSNT_ST=0x01FF0000",
            "ALL HARDWARE SUBSYSTEM VERIFICATION TESTS PASSED",
        ]
        t2 = run_test(
            "Tier 2: SDHCI eMMC Storage Media Insertion Test",
            ["-drive", f"file={dummy_img},format=raw,if=sd", "-nic", "user,model=rtd2841-eth"],
            t2_markers,
        )
        if not t2:
            success = False
    finally:
        if os.path.isfile(dummy_img):
            os.remove(dummy_img)

    # Test 3: Tier 3 - Linux Kernel Boot with RTL8168GU Network Subsystem & Live Outbound Connectivity
    kernel_img = find_kernel()
    storage_img = find_storage()
    if kernel_img and storage_img:
        ci_port = find_available_port(5555)
        if ci_port:
            nic_arg = f"user,model=rtd2841-eth,hostfwd=tcp::{ci_port}-:5555"
        else:
            nic_arg = "user,model=rtd2841-eth"
        t3 = run_test(
            "Tier 3: Full-System Linux 4.4 Kernel Boot & Live Outbound Network Connectivity",
            [
                "-drive", f"file={storage_img},format=raw,if=sd",
                "-append", "console=tty0 console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init androidboot.selinux=permissive consoleblank=0",
                "-nic", nic_arg,
            ],
            "[>>> NETWORK SUBSYSTEM VERIFICATION TESTS PASSED <<<]",
            kernel_bin=kernel_img,
            timeout_sec=45.0,
        )
        if not t3:
            success = False
    else:
        print("[*] Notice: Linux zImage or emmc.raw not present in runner environment; skipping full OS network verification test.", file=sys.stderr)
        if os.environ.get("STRICT_CI") == "1":
            success = False

    if not success:
        sys.exit(1)
    print("[>>> ALL CI HARDWARE AUTOMATION TESTS PASSED <<<]")

if __name__ == "__main__":
    main()
