#!/usr/bin/env python3
"""
scripts/send_key.py - TCL Smart TV Remote Control & QEMU Input Injector
Sends keycodes and UI navigation commands directly to the running RTD2841 QEMU instance
via the QEMU Monitor socket (TCP port 4444 or UNIX domain socket).
"""

import sys
import os
import socket
import tty
import termios

# TV remote key to QEMU monitor key mapping
QEMU_KEY_MAP = {
    "POWER": "p",
    "UP": "up",
    "DOWN": "down",
    "LEFT": "left",
    "RIGHT": "right",
    "OK": "ret",
    "ENTER": "ret",
    "CENTER": "ret",
    "BACK": "esc",
    "HOME": "h",
    "MENU": "m",
    "VOLUME_UP": "volumeup",
    "VOLUME_DOWN": "volumedown",
    "MUTE": "mute",
    "1": "1",
    "2": "2",
    "3": "3",
    "4": "4",
    "5": "5",
    "6": "6",
    "B": "b",
    "T": "t"
}

# Android TV Linux keycode values for userspace reference
ANDROID_KEYCODES = {
    "POWER": 26, "UP": 19, "DOWN": 20, "LEFT": 21, "RIGHT": 22,
    "OK": 23, "BACK": 4, "HOME": 3, "MENU": 82,
    "VOLUME_UP": 24, "VOLUME_DOWN": 25, "MUTE": 164
}

DEFAULT_MONITOR_HOST = "127.0.0.1"
DEFAULT_MONITOR_PORT = 4444
DEFAULT_MONITOR_SOCK = "/tmp/qemu-monitor.sock"

def send_qemu_key(qemu_key):
    """Transmits a sendkey command to the running QEMU monitor socket."""
    sock_path = os.environ.get("QEMU_MONITOR_SOCK", DEFAULT_MONITOR_SOCK)
    host = os.environ.get("QEMU_MONITOR_HOST", DEFAULT_MONITOR_HOST)
    port = int(os.environ.get("QEMU_MONITOR_PORT", DEFAULT_MONITOR_PORT))

    # 1. Attempt UNIX domain socket
    if os.path.exists(sock_path):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(2.0)
            s.connect(sock_path)
            s.sendall(f"sendkey {qemu_key}\n".encode("utf-8"))
            s.close()
            return True, f"UNIX socket {sock_path}"
        except Exception as e:
            pass

    # 2. Attempt TCP socket
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((host, port))
        # Read greeting banner if present
        try:
            s.recv(1024)
        except socket.timeout:
            pass
        s.sendall(f"sendkey {qemu_key}\n".encode("utf-8"))
        s.close()
        return True, f"TCP {host}:{port}"
    except Exception as e:
        return False, str(e)

def print_remote_banner():
    print("""
============================================================
      TCL SMART TV INTERACTIVE REMOTE CONTROL (IR)
      Target: Realtek RTD2841 / TCL RT41 Chassis (tv030)
============================================================
  [1] TV / DTV        [2] HDMI 1          [3] HDMI 2
  [4] Android Apps    [5] Media Center    [6] Settings
  ----------------------------------------------------------
  Controls:
    [< / > or h / l] : Navigate Inputs
    [1 - 6]          : Direct Input Jump
    [B]              : SMPTE Calibration Test Bars
    [T]              : Restore TCL TV Dashboard
    [Q]              : Exit Remote Control
============================================================
""")

def get_char():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        if ch == '\x1b':
            ch2 = sys.stdin.read(1)
            if ch2 == '[':
                ch3 = sys.stdin.read(1)
                return '\x1b[' + ch3
            return ch + ch2
        return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

def main():
    if len(sys.argv) > 1 and sys.argv[1] not in ("--interactive", "-i"):
        cmd = sys.argv[1].upper()
        if cmd in ("--HELP", "-H"):
            print("Usage: ./scripts/send_key.py [KEY_NAME | --interactive]")
            print(f"Supported keys: {', '.join(sorted(QEMU_KEY_MAP.keys()))}")
            print("\nTo enable monitor socket in QEMU, run with:")
            print("  -monitor tcp:127.0.0.1:4444,server,nowait")
            print("  or -monitor unix:/tmp/qemu-monitor.sock,server,nowait")
            sys.exit(0)

        if cmd in QEMU_KEY_MAP:
            qemu_key = QEMU_KEY_MAP[cmd]
            ok, target = send_qemu_key(qemu_key)
            if ok:
                print(f"[+] Key '{cmd}' sent successfully to QEMU via {target} (sendkey {qemu_key})")
                sys.exit(0)
            else:
                print(f"[-] Could not connect to QEMU monitor ({target}).")
                print(f"[*] Note: Launch QEMU with -monitor tcp:127.0.0.1:4444,server,nowait to enable live injection.")
                if cmd in ANDROID_KEYCODES:
                    print(f"[*] Target Android keycode: {ANDROID_KEYCODES[cmd]}")
                sys.exit(1)
        else:
            print(f"[-] Unknown key: '{cmd}'. Run with --help for supported key names.")
            sys.exit(1)

    print_remote_banner()
    print("[*] Target: Live QEMU instance via monitor TCP:4444 or /tmp/qemu-monitor.sock")
    print("[*] Ready. Press remote keys (1-6, h/l, b, t, q to quit):")

    while True:
        try:
            ch = get_char()
            if ch.lower() == 'q':
                print("\n[+] Exiting remote control.")
                break
            elif ch in ('1', '2', '3', '4', '5', '6'):
                ok, target = send_qemu_key(ch)
                status = f"dispatched to {target}" if ok else "monitor offline"
                sys.stdout.write(f"\r[*] Dispatched Input [{ch}]: {status}        \n")
            elif ch in ('h', 'H', '\x1b[D'):
                ok, target = send_qemu_key("left")
                sys.stdout.write(f"\r[*] Dispatched: LEFT ({target})             \n")
            elif ch in ('l', 'L', '\x1b[C'):
                ok, target = send_qemu_key("right")
                sys.stdout.write(f"\r[*] Dispatched: RIGHT ({target})            \n")
            elif ch in ('\r', '\n'):
                ok, target = send_qemu_key("ret")
                sys.stdout.write(f"\r[*] Dispatched: OK / ENTER ({target})       \n")
            elif ch.lower() == 'b':
                ok, target = send_qemu_key("b")
                sys.stdout.write(f"\r[*] Dispatched: SMPTE BARS ({target})       \n")
            elif ch.lower() == 't':
                ok, target = send_qemu_key("t")
                sys.stdout.write(f"\r[*] Dispatched: TV DASHBOARD ({target})     \n")
            sys.stdout.flush()
        except KeyboardInterrupt:
            print("\n[+] Exited.")
            break

if __name__ == "__main__":
    main()
