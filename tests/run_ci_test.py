#!/usr/bin/env python3
"""
tests/run_ci_test.py - Automated Headless Hardware Verification Runner for CI
Target: Realtek RTD2841 / TCL RT41 Chassis (tv030)
"""
import subprocess
import sys
import os
import time

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
    if "QEMU_BIN" in os.environ and os.path.isfile(os.environ["QEMU_BIN"]):
        return os.environ["QEMU_BIN"]
    cands = [
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

def safe_print(s):
    try:
        sys.stdout.write(s)
        sys.stdout.flush()
    except (BlockingIOError, OSError):
        pass

def run_test(name, extra_args, pass_marker, kernel_bin=None, timeout_sec=10.0):
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
        text=True,
        bufsize=1
    )

    passed = False
    start_time = time.time()

    output_lines = []
    try:
        while True:
            if time.time() - start_time > timeout_sec:
                print(f"[-] Test timed out after {timeout_sec}s!", file=sys.stderr)
                break

            line = proc.stdout.readline()
            if not line and proc.poll() is not None:
                break
            if line:
                output_lines.append(line)
                safe_print(line)
                if pass_marker in line:
                    passed = True
                    break
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()

    if passed:
        print(f"[+] CI TEST PASSED: {name}\n")
        return True
    else:
        print(f"[-] CI TEST FAILED: {name} (marker '{pass_marker}' not found)\n", file=sys.stderr)
        return False

def main():
    success = True

    # Test 1: Core SoC Diagnostic Suite without storage attached
    t1 = run_test(
        "Core SoC Diagnostics (CPU, GIC, TC0, SDHCI controller MMIO)",
        [],
        "ALL HARDWARE SUBSYSTEM VERIFICATION TESTS PASSED"
    )
    if not t1:
        success = False

    # Test 2: Storage Media Detection with virtual drive attached
    dummy_img = os.path.join(REPO_ROOT, "tests/ci_storage_test.raw")
    try:
        with open(dummy_img, "wb") as f:
            f.write(b"\x00" * (1024 * 1024 * 16)) # 16 MB dummy raw disk
        t2 = run_test(
            "SDHCI eMMC Storage Media Insertion Test",
            ["-drive", f"file={dummy_img},format=raw,if=sd"],
            "media present"
        )
        if not t2:
            success = False
    finally:
        if os.path.isfile(dummy_img):
            os.remove(dummy_img)

    # Test 3: Linux Kernel Boot with RTL8168GU Network Subsystem & Live Outbound Connectivity
    kernel_img = find_kernel()
    storage_img = find_storage()
    if kernel_img and storage_img:
        t3 = run_test(
            "Linux Kernel Boot with RTL8168GU Network Subsystem & Live Outbound Connectivity",
            [
                "-drive", f"file={storage_img},format=raw,if=sd",
                "-append", "console=tty0 console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init androidboot.selinux=permissive",
                "-nic", "user,model=rtd2841-eth,hostfwd=tcp::5555-:5555",
            ],
            "[>>> NETWORK SUBSYSTEM VERIFICATION TESTS PASSED <<<]",
            kernel_bin=kernel_img,
            timeout_sec=45.0,
        )
        if not t3:
            success = False
    else:
        print("[!] Note: Linux zImage or emmc.raw not present; skipping full OS network verification test.")

    if not success:
        sys.exit(1)
    print("[>>> ALL CI HARDWARE AUTOMATION TESTS PASSED <<<]")

if __name__ == "__main__":
    main()
