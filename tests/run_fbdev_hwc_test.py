#!/usr/bin/env python3
"""
tests/run_fbdev_hwc_test.py - Automated Framebuffer & HWC Surface Verification Runner
Target: Realtek RTD2841 / TCL RT41 Chassis (tv030)
Adheres strictly to the 7 OpenDTV Project Inviolable Doctrines.
"""

import hashlib
import http.server
import json
import os
import re
import selectors
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

REPO_ROOT = Path("/Users/basilthampy/Documents/Qemu rtd")
REF_TREE = Path("/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re")

DISK = (REPO_ROOT / "staging/emmc.raw").resolve(strict=True)
KERNEL = REPO_ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage"
DTB = REPO_ROOT / "dts/rtd284x-tv030.dtb"
QEMU = REF_TREE / "qemu/build/qemu-system-aarch64"
QEMU_IMG = REF_TREE / "qemu/build/qemu-img"
TEST_SRC = REPO_ROOT / "tests/test_fbdev_hwc.c"
TEST_BIN = REPO_ROOT / "build/phase3/test_fbdev_hwc"

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def compile_test_binary() -> str:
    """Compile test_fbdev_hwc directly from C source using ARM32 EABI cross-compiler."""
    gcc_path = shutil.which("arm-none-eabi-gcc")
    if not gcc_path:
        cand = Path("/opt/homebrew/bin/arm-none-eabi-gcc")
        if cand.is_file():
            gcc_path = str(cand)
        else:
            print("[-] FATAL: arm-none-eabi-gcc not found on system PATH")
            sys.exit(1)

    # Log toolchain version
    v_proc = subprocess.run([gcc_path, "--version"], capture_output=True, text=True, check=True)
    first_line = v_proc.stdout.splitlines()[0] if v_proc.stdout else "unknown"
    print(f"[*] Toolchain:       {gcc_path} ({first_line})")

    src_hash = sha256_file(TEST_SRC)
    print(f"[*] Test Source:     {TEST_SRC} (SHA256: {src_hash})")

    TEST_BIN.parent.mkdir(parents=True, exist_ok=True)
    compile_cmd = [
        gcc_path,
        "-nostdlib",
        "-static",
        "-fno-builtin",
        "-Wall",
        "-Wextra",
        "-O2",
        "-mcpu=cortex-a53",
        str(TEST_SRC),
        "-lgcc",
        "-o",
        str(TEST_BIN)
    ]
    print(f"[*] Build Command:   {' '.join(compile_cmd)}")
    comp_res = subprocess.run(compile_cmd, capture_output=True, text=True)
    if comp_res.returncode != 0:
        print(f"[-] FATAL: Compilation of test_fbdev_hwc failed (rc={comp_res.returncode}):")
        print(comp_res.stderr)
        sys.exit(1)

    bin_hash = sha256_file(TEST_BIN)
    print(f"[+] Compiled Binary: {TEST_BIN} (SHA256: {bin_hash})")
    return bin_hash

def boot_with_overlay(overlay, output_dir, commands, terminal_marker, timeout_sec=120):
    cmd = [
        str(QEMU), "-M", "rtd2841-tv030", "-m", "1024M", "-smp", "4",
        "-dtb", str(DTB), "-display", "none", "-serial", "stdio",
        "-monitor", "none", "-kernel", str(KERNEL), "-no-reboot",
        "-drive", f"file={overlay},format=qcow2,if=sd",
        "-append", "console=tty0 console=ttyS0,115200 earlyprintk "
                   "root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait "
                   "init=/init consoleblank=0",
        "-nic", "user,model=rtd2841-eth"
    ]
    (output_dir / "command.json").write_text(json.dumps(cmd, indent=2) + "\n")
    proc = subprocess.Popen(
        cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, preexec_fn=os.setsid, bufsize=0
    )

    selector = selectors.DefaultSelector()
    os.set_blocking(proc.stdout.fileno(), False)
    os.set_blocking(proc.stdin.fileno(), False)
    selector.register(proc.stdout, selectors.EVENT_READ)

    data = bytearray()
    quit_sent = False
    commands_sent = False
    done = False
    start = time.monotonic()
    stdin_queue = bytearray()

    def send_stdin_chunk():
        nonlocal stdin_queue
        if not stdin_queue:
            return
        try:
            to_send = bytes(stdin_queue[:64])
            written = os.write(proc.stdin.fileno(), to_send)
            if written > 0:
                del stdin_queue[:written]
        except (BlockingIOError, InterruptedError):
            pass

    try:
        with (output_dir / "serial.log").open("wb", buffering=0) as log:
            while time.monotonic() - start < timeout_sec:
                events = selector.select(timeout=0.05)
                for key, _ in events:
                    try:
                        chunk = os.read(key.fileobj.fileno(), 65536)
                    except BlockingIOError:
                        continue
                    if not chunk:
                        selector.unregister(key.fileobj)
                        break
                    log.write(chunk)
                    data.extend(chunk)
                    if len(data) > 4 * 1024 * 1024:
                        del data[:2 * 1024 * 1024]

                if stdin_queue:
                    send_stdin_chunk()

                # Step 1: Detect TV remote control prompt and send 'q'
                if not quit_sent and b"Interactive TV Remote Control Active" in data:
                    stdin_queue.extend(b"q\n")
                    send_stdin_chunk()
                    quit_sent = True

                # Step 2: Detect interactive shell prompt and queue test commands
                if quit_sent and not commands_sent and (b"Dropping into interactive" in data or b":/ # " in data):
                    payload = ("\n".join(commands) + "\n").encode()
                    stdin_queue.extend(payload)
                    send_stdin_chunk()
                    commands_sent = True

                # Step 3: Check for terminal marker
                if commands_sent and not stdin_queue and re.search(terminal_marker, data):
                    done = True
                    break

                if proc.poll() is not None and not selector.get_map():
                    break
    finally:
        selector.close()
        terminated_by_test = False
        if proc.poll() is None:
            if done:
                terminated_by_test = True
            os.killpg(proc.pid, signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait(timeout=5)
        else:
            proc.wait()

    return {
        "quit_sent": quit_sent,
        "commands_sent": commands_sent,
        "done": done,
        "terminated_by_test": terminated_by_test,
        "qemu_exit_code": proc.returncode,
        "elapsed_seconds": round(time.monotonic() - start, 1)
    }

def main():
    print("[*] OpenDTV Milestone 3: Framebuffer & HWC Hardware Verification")
    for p in (DISK, KERNEL, DTB, QEMU, QEMU_IMG, TEST_SRC):
        if not p.is_file():
            print(f"[-] FAILED: Missing prerequisite file: {p}")
            sys.exit(1)

    work_dir = Path(tempfile.mkdtemp(prefix="opendtv-fb-test-", dir="/private/tmp"))
    print(f"[*] Artifacts directory: {work_dir}")

    disk_hash_before = sha256_file(DISK)
    test_hash = compile_test_binary()
    test_bytes = TEST_BIN.read_bytes()

    # Host HTTP Server to deliver test_fbdev_hwc into guest
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/test_fbdev_hwc":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(test_bytes)))
                self.end_headers()
                self.wfile.write(test_bytes)
            else:
                self.send_error(404)

        def log_message(self, *_args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    port = server.server_address[1]

    overlay = work_dir / "emmc-overlay.qcow2"
    # Create disposable qcow2 overlay - guarantees base emmc.raw is never touched
    qimg_rc = subprocess.run(
        [str(QEMU_IMG), "create", "-f", "qcow2", "-b", str(DISK), "-F", "raw", str(overlay)],
        capture_output=True, text=True, timeout=30
    )
    if qimg_rc.returncode != 0:
        print(f"[-] FAILED: qemu-img failed: {qimg_rc.stderr}")
        sys.exit(1)

    guest_commands = [
        "echo __STAGE_START__",
        "/system/bin/mkdir -p /dev/graphics && /system/bin/ln -sf /dev/fb0 /dev/graphics/fb0",
        "LD_LIBRARY_PATH=/vendor/am_apps/tplayer/lib:/vendor/lib:/system/lib "
        f"/vendor/am_apps/tplayer/bin/curl --fail --silent --show-error --max-time 10 --output /tmp/test_fbdev_hwc http://10.0.2.2:{port}/test_fbdev_hwc",
        "echo __FETCH_RC_${?}__",
        f"printf '%s  %s\\n' {test_hash} /tmp/test_fbdev_hwc | /system/bin/sha256sum -c -",
        "echo __VERIFY_RC_${?}__",
        "/system/bin/chmod 755 /tmp/test_fbdev_hwc",
        "/tmp/test_fbdev_hwc",
        "echo __PROBE_RC_${?}__",
        "echo __STAGE_END__"
    ]

    result = boot_with_overlay(
        overlay, work_dir, guest_commands,
        rb"(?m)^__STAGE_END__\r?$", timeout_sec=120
    )

    log_bytes = (work_dir / "serial.log").read_bytes()
    log_text = log_bytes.decode("latin1", errors="replace")

    print("[*] QEMU Execution Summary:")
    print(f"    Elapsed:         {result['elapsed_seconds']}s")
    print(f"    Quit sent:       {result['quit_sent']}")
    print(f"    Commands sent:   {result['commands_sent']}")
    print(f"    Done:            {result['done']}")
    print(f"    Clean SIGTERM:   {result['terminated_by_test']}")
    print(f"    QEMU Exit code:  {result['qemu_exit_code']}")

    # Validation Checks
    done_ok = (result["done"] is True)
    quit_ok = (result["quit_sent"] is True)
    cmds_ok = (result["commands_sent"] is True)
    term_ok = (result["terminated_by_test"] is True)
    fetch_ok = "__FETCH_RC_0__" in log_text
    verify_ok = "__VERIFY_RC_0__" in log_text
    probe_ok = "__PROBE_RC_0__" in log_text
    readback_ok = "Readback verified: all 2,073,600 pixels match bitwise ground truth." in log_text
    passed_marker = "SURFACE PROBE VERIFICATION PASSED" in log_text
    stage_end_ok = "__STAGE_END__" in log_text

    print(f"    Done marker:     {'OK' if done_ok else 'FAIL'}")
    print(f"    Fetch binary:    {'OK' if fetch_ok else 'FAIL'}")
    print(f"    Verify SHA256:   {'OK' if verify_ok else 'FAIL'}")
    print(f"    Probe exit 0:    {'OK' if probe_ok else 'FAIL'}")
    print(f"    Bitwise readback:{'OK' if readback_ok else 'FAIL'}")
    print(f"    Pass banner:     {'OK' if passed_marker else 'FAIL'}")
    print(f"    Stage end:       {'OK' if stage_end_ok else 'FAIL'}")
    print(f"    Clean teardown:  {'OK' if term_ok else 'FAIL'}")

    disk_hash_after = sha256_file(DISK)
    if disk_hash_before != disk_hash_after:
        print("[-] FATAL: Base disk SHA256 was altered!")
        sys.exit(1)
    print(f"[+] Base disk SHA256 verified invariant: {disk_hash_before}")

    if (done_ok and quit_ok and cmds_ok and fetch_ok and verify_ok and
            probe_ok and readback_ok and passed_marker and stage_end_ok and term_ok):
        print("[+] MILESTONE 3 HARDWARE FRAMEBUFFER & COMPOSER SURFACE PROBE PASSED!")
        sys.exit(0)
    else:
        print("[-] MILESTONE 3 PROBE FAILED. Inspect log: " + str(work_dir / "serial.log"))
        sys.exit(2)

if __name__ == "__main__":
    main()
