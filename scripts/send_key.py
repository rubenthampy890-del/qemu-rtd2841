#!/usr/bin/env python3
"""
scripts/send_key.py - TCL Smart TV Remote Control & QEMU Input Injector
Sends keycodes and UI navigation commands directly to the running RTD2841 QEMU instance
via the QEMU Monitor / QMP socket (UNIX domain socket or TCP port 4444).
Supports automatic detection and negotiation of both QMP (QEMU Machine Protocol)
and HMP (Human Monitor Protocol).
"""

import sys
import os
import socket
import tty
import termios
import json
import time
import select

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
DEFAULT_QMP_SOCK = "/tmp/qemu-qmp.sock"

class SocketLineReader:
    """Helper to read lines and peek available data with timeouts."""
    def __init__(self, sock):
        self.sock = sock
        self.buf = ""

    def read_available(self, timeout=0.3):
        r, _, _ = select.select([self.sock], [], [], timeout)
        if r:
            try:
                data = self.sock.recv(4096)
                if data:
                    self.buf += data.decode("utf-8", errors="replace")
            except (socket.error, OSError):
                pass
        return self.buf

    def readline(self, timeout=2.0):
        start = time.time()
        while "\n" not in self.buf:
            remaining = timeout - (time.time() - start)
            if remaining <= 0:
                break
            r, _, _ = select.select([self.sock], [], [], remaining)
            if not r:
                break
            try:
                data = self.sock.recv(4096)
                if not data:
                    break
                self.buf += data.decode("utf-8", errors="replace")
            except (socket.error, OSError):
                break
        if "\n" in self.buf:
            line, self.buf = self.buf.split("\n", 1)
            return line.strip()
        line = self.buf.strip()
        self.buf = ""
        return line

def _read_qmp_json(reader, timeout=2.0):
    start = time.time()
    while time.time() - start < timeout:
        line = reader.readline(timeout=timeout)
        if not line:
            continue
        try:
            data = json.loads(line)
            # Filter out asynchronous event notifications
            if "event" in data and "return" not in data and "error" not in data:
                continue
            return data
        except json.JSONDecodeError:
            continue
    return None

def _try_socket(s, qemu_key):
    """
    Communicates with QEMU peer, auto-detecting QMP vs HMP.
    Returns (True, mode_str) on success, or (False, error_str) on failure.
    """
    reader = SocketLineReader(s)
    initial_data = reader.read_available(timeout=0.3)

    if '"QMP"' in initial_data:
        # Peer speaks QMP
        if "\n" not in initial_data:
            reader.readline(timeout=1.0)

        # Step 1: Capabilities negotiation
        cap_cmd = json.dumps({"execute": "qmp_capabilities"}) + "\n"
        s.sendall(cap_cmd.encode("utf-8"))
        resp = _read_qmp_json(reader, timeout=2.0)
        if not resp or "error" in resp:
            err = resp.get("error", "no response") if resp else "timeout"
            return False, f"QMP capabilities failed: {err}"

        # Step 2: Inject key via human-monitor-command
        hmp_cmd = json.dumps({
            "execute": "human-monitor-command",
            "arguments": {"command-line": f"sendkey {qemu_key}"}
        }) + "\n"
        s.sendall(hmp_cmd.encode("utf-8"))
        resp = _read_qmp_json(reader, timeout=2.0)
        if not resp:
            return False, "QMP command timeout"
        if "error" in resp:
            return False, f"QMP command error: {resp['error']}"
        if "return" in resp:
            ret_val = resp["return"]
            if isinstance(ret_val, str) and ("unknown key" in ret_val.lower() or "error" in ret_val.lower()):
                return False, f"QMP sendkey failed: {ret_val.strip()}"
        return True, "QMP"
    else:
        # Peer speaks plaintext HMP
        cmd = f"sendkey {qemu_key}\n"
        s.sendall(cmd.encode("utf-8"))
        resp = reader.read_available(timeout=0.2)
        if "unknown key" in resp.lower() or "error:" in resp.lower():
            return False, f"HMP error: {resp.strip()}"
        return True, "HMP"

def send_qemu_key(qemu_key):
    """Transmits a sendkey command to the running QEMU monitor/QMP socket."""
    sock_path = os.environ.get("QEMU_MONITOR_SOCK")
    candidate_socks = []
    if sock_path:
        candidate_socks.append(sock_path)
    else:
        candidate_socks.extend([DEFAULT_MONITOR_SOCK, DEFAULT_QMP_SOCK])

    last_error = None

    # 1. Attempt UNIX domain sockets
    for path in candidate_socks:
        if os.path.exists(path):
            s = None
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.settimeout(2.0)
                s.connect(path)
                ok, detail = _try_socket(s, qemu_key)
                if ok:
                    return True, f"UNIX socket {path} ({detail})"
                last_error = f"{path}: {detail}"
            except Exception as e:
                last_error = f"{path}: {e}"
            finally:
                if s is not None:
                    try:
                        s.close()
                    except Exception:
                        pass

    # 2. Attempt TCP socket
    host = os.environ.get("QEMU_MONITOR_HOST", DEFAULT_MONITOR_HOST)
    port = int(os.environ.get("QEMU_MONITOR_PORT", DEFAULT_MONITOR_PORT))
    s = None
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((host, port))
        ok, detail = _try_socket(s, qemu_key)
        if ok:
            return True, f"TCP {host}:{port} ({detail})"
        return False, f"TCP {host}:{port}: {detail}"
    except Exception as e:
        if last_error:
            return False, f"{last_error} | TCP {host}:{port}: {e}"
        return False, f"TCP {host}:{port}: {e}"
    finally:
        if s is not None:
            try:
                s.close()
            except Exception:
                pass

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
        raw_cmd = sys.argv[1]
        cmd = raw_cmd.upper()
        if cmd in ("--HELP", "-H"):
            print("Usage: ./scripts/send_key.py [KEY_NAME | --interactive]")
            print(f"Supported keys: {', '.join(sorted(QEMU_KEY_MAP.keys()))}")
            print("\nTo enable monitor/QMP socket in QEMU, run with:")
            print("  -monitor unix:/tmp/qemu-monitor.sock,server,nowait")
            print("  or -qmp unix:/tmp/qemu-monitor.sock,server,nowait")
            print("  or -monitor tcp:127.0.0.1:4444,server,nowait")
            sys.exit(0)

        if cmd in QEMU_KEY_MAP:
            qemu_key = QEMU_KEY_MAP[cmd]
        elif raw_cmd.lower() in QEMU_KEY_MAP.values():
            qemu_key = raw_cmd.lower()
        else:
            print(f"[-] Unknown key: '{raw_cmd}'. Run with --help for supported key names.")
            sys.exit(1)

        ok, target = send_qemu_key(qemu_key)
        if ok:
            print(f"[+] Key '{raw_cmd}' sent successfully to QEMU via {target} (sendkey {qemu_key})")
            sys.exit(0)
        else:
            print(f"[-] Could not connect to QEMU monitor ({target}).")
            print(f"[*] Note: Launch QEMU with -monitor unix:/tmp/qemu-monitor.sock,server,nowait or -qmp unix:/tmp/qemu-monitor.sock,server,nowait")
            if cmd in ANDROID_KEYCODES:
                print(f"[*] Target Android keycode: {ANDROID_KEYCODES[cmd]}")
            sys.exit(1)

    print_remote_banner()
    print("[*] Target: Live QEMU instance via monitor/QMP (TCP:4444 or /tmp/qemu-monitor.sock)")
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
                status = f"dispatched to {target}" if ok else "monitor offline"
                sys.stdout.write(f"\r[*] Dispatched: LEFT ({status})             \n")
            elif ch in ('l', 'L', '\x1b[C'):
                ok, target = send_qemu_key("right")
                status = f"dispatched to {target}" if ok else "monitor offline"
                sys.stdout.write(f"\r[*] Dispatched: RIGHT ({status})            \n")
            elif ch in ('\r', '\n'):
                ok, target = send_qemu_key("ret")
                status = f"dispatched to {target}" if ok else "monitor offline"
                sys.stdout.write(f"\r[*] Dispatched: OK / ENTER ({status})       \n")
            elif ch.lower() == 'b':
                ok, target = send_qemu_key("b")
                status = f"dispatched to {target}" if ok else "monitor offline"
                sys.stdout.write(f"\r[*] Dispatched: SMPTE BARS ({status})       \n")
            elif ch.lower() == 't':
                ok, target = send_qemu_key("t")
                status = f"dispatched to {target}" if ok else "monitor offline"
                sys.stdout.write(f"\r[*] Dispatched: TV DASHBOARD ({status})     \n")
            sys.stdout.flush()
        except KeyboardInterrupt:
            print("\n[+] Exited.")
            break

if __name__ == "__main__":
    main()
