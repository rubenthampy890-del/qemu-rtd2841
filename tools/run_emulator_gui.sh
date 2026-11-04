#!/bin/bash
set -e

REPO_ROOT="/Users/basilthampy/Documents/Qemu rtd"
QEMU="/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64"
DTB="$REPO_ROOT/dts/rtd284x-tv030.dtb"
KERNEL="$REPO_ROOT/build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage"
DISK="$REPO_ROOT/staging/emmc.raw"

echo "========================================================"
echo " Starting OpenDTV RTD2841 Smart TV Emulator (Cocoa + Shell)"
echo "========================================================"
echo "Display: 1920x1080@32bpp Simple Framebuffer (Cocoa Window)"
echo "Console: Interactive Serial Root Shell (/bin/sh in this window)"
echo "Network: RTL8168 10.0.2.15 (ADB port forwarded to 5555)"
echo "========================================================"

exec "$QEMU" \
  -M rtd2841-tv030 \
  -m 1024M \
  -smp 4 \
  -dtb "$DTB" \
  -kernel "$KERNEL" \
  -drive file="$DISK",format=raw,if=sd,snapshot=on \
  -append "console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init androidboot.selinux=permissive consoleblank=0" \
  -serial stdio \
  -nic user,model=rtd2841-eth,hostfwd=tcp::5555-:5555 \
  -display cocoa
