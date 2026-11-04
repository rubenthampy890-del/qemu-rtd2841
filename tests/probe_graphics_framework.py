#!/usr/bin/env python3
"""
Diagnostic probe for Graphics HALs, SurfaceFlinger, and Zygote (app_process32).
Boots RTD2841 QEMU with a temporary overlay and executes commands interactively at :/ #.
"""

import os
import sys
import time
import json
import signal
import tempfile
import selectors
import subprocess
from pathlib import Path

ROOT = Path("/Users/basilthampy/Documents/Qemu rtd")
REF = Path("/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re")

DISK = (ROOT / "staging/emmc.raw").resolve(strict=True)
KERNEL = (ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage").resolve(strict=True)
DTB = (ROOT / "dts/rtd284x-tv030.dtb").resolve(strict=True)
QEMU = (REF / "qemu/build/qemu-system-aarch64").resolve(strict=True)
QEMU_IMG = (REF / "qemu/build/qemu-img").resolve(strict=True)

COMMANDS = [
    "echo '=== 1. DIAGNOSTIC PROPERTIES ==='",
    "getprop ro.hardware.hwcomposer",
    "getprop ro.hardware.egl",
    "getprop ro.hardware.gralloc",
    "getprop hwservicemanager.ready",
    "echo '=== 2. RUNNING PROCESSES INITIAL ==='",
    "ps",
    "echo '=== 3. STARTING CONFIGSTORE ==='",
    "/vendor/bin/hw/android.hardware.configstore@1.1-service &",
    "sleep 1",
    "ps | grep configstore",
    "echo '=== 4. STARTING ALLOCATOR ==='",
    "/vendor/bin/hw/android.hardware.graphics.allocator@2.0-service &",
    "sleep 1",
    "ps | grep allocator",
    "echo '=== 5. STARTING COMPOSER ==='",
    "/vendor/bin/hw/android.hardware.graphics.composer@2.1-service &",
    "sleep 1",
    "ps | grep composer",
    "echo '=== 6. STARTING SURFACEFLINGER ==='",
    "/system/bin/surfaceflinger &",
    "sleep 3",
    "ps | grep surfaceflinger",
    "logcat -d -v time -t 50",
    "echo '=== 7. TEST APP_PROCESS DIRECT EXECUTION ==='",
    "/system/bin/app_process /system/bin --version",
    "/system/bin/app_process /system/bin -help",
    "echo '##__PROBE_FINISHED_EXECUTION_MARKER__##'"
]

def main():
    print("[*] OpenDTV Phase 3: Probing Graphics HALs, SurfaceFlinger, and ART/Zygote...")
    with tempfile.TemporaryDirectory(prefix="opendtv-probe-") as tmpdir:
        td = Path(tmpdir)
        overlay = td / "overlay.qcow2"
        subprocess.run([str(QEMU_IMG), "create", "-f", "qcow2", "-b", str(DISK), "-F", "raw", str(overlay)],
                       check=True, capture_output=True)

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

        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, preexec_fn=os.setsid, bufsize=0)

        sel = selectors.DefaultSelector()
        os.set_blocking(proc.stdout.fileno(), False)
        sel.register(proc.stdout, selectors.EVENT_READ)

        buf = ""
        sent_q = False
        cmd_idx = 0
        last_send_time = 0
        t0 = time.time()
        timeout = 120.0

        try:
            with open(td / "serial.log", "w") as f_log:
                while time.time() - t0 < timeout:
                    events = sel.select(timeout=0.1)
                    for key, _ in events:
                        try:
                            chunk = os.read(key.fileobj.fileno(), 4096).decode("utf-8", "replace")
                            if chunk:
                                buf += chunk
                                f_log.write(chunk)
                                f_log.flush()
                                if sent_q:
                                    sys.stdout.write(chunk)
                                    sys.stdout.flush()
                        except Exception:
                            pass

                    if not sent_q and "Interactive TV Remote Control Active" in buf:
                        proc.stdin.write(b"q\n")
                        proc.stdin.flush()
                        sent_q = True
                        last_send_time = time.time()

                    # Once shell is ready (prompt ':/ # '), feed commands sequentially
                    if sent_q and ":/ #" in buf and (time.time() - last_send_time > 0.3):
                        if cmd_idx < len(COMMANDS):
                            c = COMMANDS[cmd_idx]
                            cmd_idx += 1
                            # clear prompt from buffer by keeping only recent text
                            buf = buf[-256:]
                            proc.stdin.write(f"{c}\n".encode())
                            proc.stdin.flush()
                            last_send_time = time.time()

                    if "##__PROBE_FINISHED_EXECUTION_MARKER__##" in buf:
                        print("\n[+] All probe commands finished execution successfully.")
                        break

                    if proc.poll() is not None:
                        break
        finally:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception:
                pass

if __name__ == "__main__":
    main()
