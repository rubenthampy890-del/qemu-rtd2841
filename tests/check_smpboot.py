#!/usr/bin/env python3
"""Reassemble the mailbox monitor and compare the embedded QEMU words."""
from pathlib import Path
import os
import re
import struct
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    prefix = os.environ.get("CROSS_COMPILE", "arm-none-eabi-")
    source = (root / "qemu-model/rtd2841.c").read_text()
    block = source.split("rtd2841_smpboot[] = {", 1)[1].split("};", 1)[0]
    words = [int(word, 16) for word in re.findall(r"\{ (0x[0-9a-f]+) \}", block)]
    if not words or len(words) * 4 >= 0x100:
        raise RuntimeError("empty or oversized embedded mailbox monitor")
    with tempfile.TemporaryDirectory(prefix="rtd2841-smpboot-") as temp:
        obj, raw = Path(temp) / "monitor.o", Path(temp) / "monitor.bin"
        subprocess.run([prefix + "as", "-mcpu=cortex-a53",
                        str(root / "qemu-model/rtd2841-smpboot.S"),
                        "-o", str(obj)], check=True, timeout=30)
        subprocess.run([prefix + "objcopy", "-O", "binary", "-j", ".text",
                        str(obj), str(raw)], check=True, timeout=30)
        if raw.read_bytes() != struct.pack("<" + "I" * len(words), *words):
            raise RuntimeError("assembled monitor differs from QEMU words")
    print(f"Mailbox monitor: {len(words)} ARM32 instructions match source")


if __name__ == "__main__":
    main()
