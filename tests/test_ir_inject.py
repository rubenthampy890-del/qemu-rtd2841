#!/usr/bin/env python3
"""
tests/test_ir_inject.py - Full-Stack IR Remote Injection Verification Test
Target: Realtek RTD2841 / TCL RT41 Chassis (tv030)

Verifies:
1. Linux input subsystem registers IR device: 'input: mtkinp as /devices/virtual/input/input0'
2. QMP command 'rtd2841-ir-inject' injects scancode 0x0B (KEY_ENTER)
3. HMP command 'ir_inject' injects scancode 0xA6 (KEY_UP)
4. Kernel driver decodes scancodes ('Venus IR: Data 0xb', 'Venus IR: Data 0xa6')
5. Linux evdev subsystem (/dev/input/event0) delivers userspace input events:
   - EV_KEY KEY_ENTER (DOWN / UP)
   - EV_KEY KEY_UP (DOWN / UP)
"""

import os
import sys
import time
import socket
import json
import signal
import shutil
import tempfile
import selectors
import subprocess

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def find_qemu():
    if "QEMU_BIN" in os.environ and os.path.isfile(os.environ["QEMU_BIN"]) and os.access(os.environ["QEMU_BIN"], os.X_OK):
        return os.environ["QEMU_BIN"]
    cands = [
        "/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64",
        os.path.expanduser("~/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64"),
        os.path.join(REPO_ROOT, "qemu-upstream/build/qemu-system-aarch64"),
        os.path.join(REPO_ROOT, "qemu/build/qemu-system-aarch64"),
    ]
    for c in cands:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    p = shutil.which("qemu-system-aarch64")
    if p:
        return p
    return None

def find_qemu_img():
    candidates = [
        os.environ.get("QEMU_IMG"),
        os.path.join(os.path.dirname(find_qemu() or ""), "qemu-img"),
        shutil.which("qemu-img"),
    ]
    for c in candidates:
        if c and os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    return None

def run_test():
    qemu_bin = find_qemu()
    if not qemu_bin:
        print("[-] Error: qemu-system-aarch64 binary not found.", file=sys.stderr)
        return False

    qemu_img = find_qemu_img()
    if not qemu_img:
        print("[-] Error: qemu-img binary not found.", file=sys.stderr)
        return False

    dtb = os.path.join(REPO_ROOT, "dts/rtd284x-tv030.dtb")
    kernel = os.path.join(REPO_ROOT, "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage")
    storage = os.path.join(REPO_ROOT, "staging/emmc.raw")

    for f, desc in [(dtb, "Device tree blob"), (kernel, "Kernel image"), (storage, "Storage disk")]:
        if not os.path.isfile(f):
            print(f"[-] Error: {desc} not found at {f}", file=sys.stderr)
            return False

    with tempfile.TemporaryDirectory(prefix="opendtv-ir-test-") as tmpdir:
        overlay = os.path.join(tmpdir, "overlay.qcow2")
        qmp_sock = os.path.join(tmpdir, "qmp.sock")
        hmp_sock = os.path.join(tmpdir, "hmp.sock")

        # 1. Create isolated qcow2 overlay backed by raw eMMC
        print(f"[*] Creating isolated overlay in {tmpdir}...")
        res = subprocess.run([qemu_img, "create", "-f", "qcow2", "-b", os.path.realpath(storage), "-F", "raw", overlay],
                             capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[-] Failed to create qcow2 overlay: {res.stderr}", file=sys.stderr)
            return False

        cmd = [
            qemu_bin,
            "-M", "rtd2841-tv030",
            "-m", "1024M",
            "-smp", "4",
            "-dtb", dtb,
            "-display", "none",
            "-serial", "stdio",
            "-kernel", kernel,
            "-drive", f"file={overlay},format=qcow2,if=sd",
            "-append", "console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init androidboot.selinux=permissive consoleblank=0",
            "-nic", "user,model=rtd2841-eth",
            "-qmp", f"unix:{qmp_sock},server,nowait",
            "-monitor", f"unix:{hmp_sock},server,nowait"
        ]

        print("[*] Launching QEMU with Linux kernel...")
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            preexec_fn=os.setsid
        )

        os.set_blocking(proc.stdout.fileno(), False)
        sel = selectors.DefaultSelector()
        sel.register(proc.stdout, selectors.EVENT_READ)

        buf = ""
        start_time = time.time()
        timeout = 45.0
        sent_quit = False
        started_getevent = False
        success = False

        try:
            while time.time() - start_time < timeout:
                events = sel.select(timeout=0.2)
                for key, _ in events:
                    try:
                        chunk = os.read(proc.stdout.fileno(), 4096).decode("utf-8", "replace")
                        if chunk:
                            buf += chunk
                    except (BlockingIOError, InterruptedError):
                        continue
                    except OSError:
                        break

                # Step 1: Detect fb_test active, send 'q' to transition to interactive root shell
                if not sent_quit and ("Interactive TV Remote Control Active" in buf or "fb_test" in buf):
                    time.sleep(0.3)
                    proc.stdin.write(b"q\n")
                    proc.stdin.flush()
                    sent_quit = True
                    print("[+] Interactive test session detected, requested transition to root shell.")

                # Step 2: In root shell, launch getevent on /dev/input/event0
                if sent_quit and not started_getevent and (":/ #" in buf or "Dropping into interactive Android root shell" in buf):
                    time.sleep(0.3)
                    proc.stdin.write(b"/system/bin/getevent -l /dev/input/event0 &\n")
                    proc.stdin.flush()
                    started_getevent = True
                    print("[+] Root shell active. Launched '/system/bin/getevent -l /dev/input/event0 &' in guest.")
                    break

            if not started_getevent:
                print("[-] Timed out waiting for guest userspace shell!", file=sys.stderr)
                return False

            time.sleep(1.0)

            # Step 3: Test QMP injection of 0x0B (KEY_ENTER)
            print("[*] Connecting to QMP socket...")
            s_qmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s_qmp.settimeout(5.0)
            s_qmp.connect(qmp_sock)

            # Read greeting
            qmp_greeting = s_qmp.recv(4096).decode("utf-8")
            s_qmp.sendall(json.dumps({"execute": "qmp_capabilities"}).encode("utf-8") + b"\n")
            qmp_cap_resp = s_qmp.recv(4096).decode("utf-8")

            print("[*] Injecting scancode 0x0B (KEY_ENTER) via QMP rtd2841-ir-inject...")
            s_qmp.sendall(json.dumps({"execute": "rtd2841-ir-inject", "arguments": {"scancode": 0x0B}}).encode("utf-8") + b"\n")
            qmp_inj_resp = s_qmp.recv(4096).decode("utf-8")
            print(f"[QMP Response] {qmp_inj_resp.strip()}")
            s_qmp.close()

            time.sleep(1.0)

            # Step 4: Test HMP injection of 0xA6 (KEY_UP)
            print("[*] Connecting to HMP socket...")
            s_hmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s_hmp.settimeout(5.0)
            s_hmp.connect(hmp_sock)
            s_hmp.recv(4096) # prompt

            print("[*] Injecting scancode 0xA6 (KEY_UP) via HMP ir_inject...")
            s_hmp.sendall(b"ir_inject 0xa6\n")
            time.sleep(1.0)
            s_hmp.close()

            # Step 5: Drain output for up to 4 seconds to observe getevent and dmesg
            drain_deadline = time.time() + 4.0
            while time.time() < drain_deadline:
                events = sel.select(timeout=0.2)
                for key, _ in events:
                    try:
                        chunk = os.read(proc.stdout.fileno(), 4096).decode("utf-8", "replace")
                        if chunk:
                            buf += chunk
                    except (BlockingIOError, InterruptedError):
                        continue
                    except OSError:
                        break

            # Step 6: Verify evidence across layers
            has_input_reg = "input: mtkinp as /devices/virtual/input/input0" in buf
            has_dmesg_enter = "Venus IR: Data 0xb" in buf
            has_dmesg_up = "Venus IR: Data 0xa6" in buf
            has_ev_enter = "KEY_ENTER" in buf
            has_ev_up = "KEY_UP" in buf

            print("\n================ Verification Summary ================")
            print(f"[*] [1/5] Kernel input dev registration ('mtkinp' -> event0): {has_input_reg}")
            print(f"[*] [2/5] Kernel driver decoded scancode 0x0B (ENTER):        {has_dmesg_enter}")
            print(f"[*] [3/5] Userspace evdev received KEY_ENTER:                  {has_ev_enter}")
            print(f"[*] [4/5] Kernel driver decoded scancode 0xA6 (UP):           {has_dmesg_up}")
            print(f"[*] [5/5] Userspace evdev received KEY_UP:                     {has_ev_up}")
            print("======================================================\n")

            if has_input_reg and has_dmesg_enter and has_ev_enter and has_dmesg_up and has_ev_up:
                print("[+] SUCCESS: IR hardware peripheral correctly delivered both injected scancodes to Linux guest and userspace evdev!")
                success = True
            else:
                print("[-] FAILED: Missing one or more required verification markers in guest log.", file=sys.stderr)
                success = False

        finally:
            if proc.poll() is None:
                try:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                except Exception:
                    pass
                proc.wait()

    return success

if __name__ == "__main__":
    ok = run_test()
    sys.exit(0 if ok else 1)
