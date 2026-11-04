#!/usr/bin/env python3
"""
Check Binder service registration for SurfaceFlinger.
"""

import os
import sys
import time
import subprocess
import tempfile
import selectors
import signal
from pathlib import Path

ROOT = Path("/Users/basilthampy/Documents/Qemu rtd")
REF = Path("/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re")

DISK = (ROOT / "staging/emmc.raw").resolve(strict=True)
KERNEL = (ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage").resolve(strict=True)
DTB = (ROOT / "dts/rtd284x-tv030.dtb").resolve(strict=True)
QEMU = (REF / "qemu/build/qemu-system-aarch64").resolve(strict=True)
QEMU_IMG = (REF / "qemu/build/qemu-img").resolve(strict=True)

COMMANDS = [
    "/vendor/bin/hw/android.hardware.configstore@1.1-service &",
    "/vendor/bin/hw/android.hardware.graphics.allocator@2.0-service &",
    "/vendor/bin/hw/android.hardware.graphics.composer@2.1-service &",
    "sleep 2",
    "/system/bin/surfaceflinger &",
    "sleep 4",
    "/system/bin/service list",
    "/system/bin/service check SurfaceFlinger",
    "dumpsys SurfaceFlinger | head -n 30"
]

def main():
    print("[*] Checking SurfaceFlinger Binder registration...")
    with tempfile.TemporaryDirectory() as td:
        overlay = Path(td) / "overlay.qcow2"
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
        t0 = time.time()

        def read_until_prompt(target_prompt=":/ # ", wait_timeout=15.0):
            nonlocal buf
            start_t = time.time()
            collected = ""
            while time.time() - start_t < wait_timeout:
                events = sel.select(timeout=0.1)
                for key, _ in events:
                    try:
                        chunk = os.read(key.fileobj.fileno(), 4096).decode("utf-8", "replace")
                        if chunk:
                            buf += chunk
                            collected += chunk
                            sys.stdout.write(chunk)
                            sys.stdout.flush()
                    except Exception:
                        pass
                if target_prompt in collected:
                    return collected
                if proc.poll() is not None:
                    break
            return collected

        try:
            while time.time() - t0 < 45.0:
                events = sel.select(timeout=0.1)
                for key, _ in events:
                    try:
                        chunk = os.read(key.fileobj.fileno(), 4096).decode("utf-8", "replace")
                        if chunk:
                            buf += chunk
                    except Exception:
                        pass
                if not sent_q and "Interactive TV Remote Control Active" in buf:
                    proc.stdin.write(b"q\n")
                    proc.stdin.flush()
                    sent_q = True
                    break

            print("[+] Waiting for initial root prompt...")
            read_until_prompt(":/ # ", wait_timeout=10.0)

            for idx, c in enumerate(COMMANDS):
                print(f"\n[>>> EXEC {idx+1}/{len(COMMANDS)}: {c} <<<]")
                proc.stdin.write(f"{c}\n".encode())
                proc.stdin.flush()
                res = read_until_prompt(":/ # ", wait_timeout=10.0)
                time.sleep(0.2)

            print("\n[+] Service check completed.")
        finally:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception:
                pass

if __name__ == "__main__":
    main()
