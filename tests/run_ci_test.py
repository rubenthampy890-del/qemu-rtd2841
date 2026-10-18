#!/usr/bin/env python3
"""
tests/run_ci_test.py - Automated Headless Hardware Verification Runner for CI
Target: Realtek RTD2841 / TCL RT41 Chassis (tv030)
"""
import subprocess
import sys
import os
import time

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

def run_test(name, extra_args, pass_marker):
    qemu_bin = find_qemu()
    if not qemu_bin:
        print("[-] Error: qemu-system-aarch64 binary not found.", file=sys.stderr)
        return False

    dtb = os.path.join(REPO_ROOT, "dts/rtd284x-tv030.dtb")
    kernel = os.path.join(REPO_ROOT, "tests/firmware_test.bin")

    cmd = [
        qemu_bin,
        "-M", "rtd2841-tv030",
        "-m", "1024M",
        "-smp", "4",
        "-dtb", dtb,
        "-display", "none",
        "-serial", "stdio",
        "-kernel", kernel,
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
    timeout_sec = 10.0

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
                sys.stdout.write(line)
                sys.stdout.flush()
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

    if not success:
        sys.exit(1)
    print("[>>> ALL CI HARDWARE AUTOMATION TESTS PASSED <<<]")

if __name__ == "__main__":
    main()
