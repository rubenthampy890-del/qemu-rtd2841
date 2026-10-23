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

def run_test(name, extra_args, pass_marker, kernel_bin=None, timeout_sec=10.0):
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
                    if pass_marker in full_line:
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
            if pass_marker in full_line:
                passed = True

        if line_buf:
            output_lines.append(line_buf + "\n")
            safe_print(line_buf + "\n")
            if pass_marker in line_buf:
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
        print("[-] Error: Linux zImage or emmc.raw not present; skipping full OS network verification test.", file=sys.stderr)
        if os.environ.get("CI") or os.environ.get("STRICT_CI"):
            success = False

    if not success:
        sys.exit(1)
    print("[>>> ALL CI HARDWARE AUTOMATION TESTS PASSED <<<]")

if __name__ == "__main__":
    main()
