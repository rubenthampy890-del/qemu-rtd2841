#!/usr/bin/env python3
"""
scripts/send_key.py - TCL Smart TV Remote Control Helper
Sends keycodes and UI navigation commands to the RTD2841 QEMU emulation.
"""

import sys
import os
import tty
import termios

# Android TV KeyCode Definitions
KEYCODES = {
    "POWER": 26,
    "UP": 19,
    "DOWN": 20,
    "LEFT": 21,
    "RIGHT": 22,
    "OK": 23,
    "CENTER": 23,
    "BACK": 4,
    "HOME": 3,
    "MENU": 82,
    "VOLUME_UP": 24,
    "VOLUME_DOWN": 25,
    "MUTE": 164,
    "TV": 170,
    "SETTINGS": 176
}

CARD_MAP = {
    "1": ("0", "TV / DTV"),
    "2": ("1", "HDMI 1"),
    "3": ("2", "HDMI 2"),
    "4": ("3", "Android Apps"),
    "5": ("4", "Media Center"),
    "6": ("5", "System Settings")
}

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
            # Handle escape sequence for arrow keys
            ch2 = sys.stdin.read(1)
            if ch2 == '[':
                ch3 = sys.stdin.read(1)
                return '\x1b[' + ch3
            return ch + ch2
        return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

def main():
    if len(sys.argv) > 1:
        cmd = sys.argv[1].upper()
        if cmd in KEYCODES:
            print(f"[+] Dispatched Key: {cmd} (KeyCode: {KEYCODES[cmd]})")
            print(f"[*] Command to execute in QEMU guest shell: input keyevent {KEYCODES[cmd]}")
            sys.exit(0)
        elif cmd in ("--help", "-h"):
            print("Usage: ./scripts/send_key.py [KEY_NAME | --interactive]")
            print(f"Available keys: {', '.join(KEYCODES.keys())}")
            sys.exit(0)

    print_remote_banner()
    current_card = 0

    print("[*] Interactive Mode: Use keys inside QEMU terminal (/bin/fb_test --loop)")
    print("[*] Or run individual commands inside the target shell:\n")
    print("    fb_test --card 0   # TV / DTV")
    print("    fb_test --card 1   # HDMI 1")
    print("    fb_test --card 3   # Android Apps")
    print("    fb_test --bars     # SMPTE 1080p Color Bars")
    print("    fb_test --loop     # Start live keyboard control\n")

if __name__ == "__main__":
    main()
