#!/bin/bash
set -e

REPO_ROOT="/Users/basilthampy/Documents/Qemu rtd"
QEMU="/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64"
DTB="$REPO_ROOT/dts/rtd284x-tv030.dtb"
KERNEL="$REPO_ROOT/build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage"

# Select disk image: prioritize persistent Android overlay, fallback to base raw disk
MODE="android"
if [ "$1" == "--diag" ] || [ "$1" == "-diag" ]; then
    MODE="diag"
fi

if [ -n "$OVERLAY" ] && [ -f "$OVERLAY" ]; then
    DISK="$OVERLAY"
    DISK_FMT="qcow2"
    SNAPSHOT_OPT=""
elif [ "$MODE" == "android" ] && [ -f "$REPO_ROOT/build/phase3/emmc-android-overlay.qcow2" ]; then
    DISK="$REPO_ROOT/build/phase3/emmc-android-overlay.qcow2"
    DISK_FMT="qcow2"
    SNAPSHOT_OPT=""
elif [ "$MODE" == "android" ] && [ -f "/private/tmp/opendtv-milestone6-2gv94ia1/emmc-persistent-overlay.qcow2" ]; then
    DISK="/private/tmp/opendtv-milestone6-2gv94ia1/emmc-persistent-overlay.qcow2"
    DISK_FMT="qcow2"
    SNAPSHOT_OPT=""
else
    DISK="$REPO_ROOT/staging/emmc.raw"
    DISK_FMT="raw"
    SNAPSHOT_OPT=",snapshot=on"
    MODE="diag"
fi

if [ "$MODE" == "android" ]; then
    CMDLINE="console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init android=1 androidboot.selinux=permissive consoleblank=0 fbcon=map:9"
else
    CMDLINE="console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init androidboot.selinux=permissive consoleblank=0 fbcon=map:9"
fi

echo "========================================================"
echo " Starting OpenDTV RTD2841 Smart TV Emulator (Cocoa + Shell)"
echo "========================================================"
echo "Mode:    $MODE"
echo "Disk:    $DISK (format=$DISK_FMT)"
echo "Display: 1920x1080@32bpp Simple Framebuffer (Cocoa Window)"
echo "Console: Interactive Serial Root Shell (/bin/sh in this window)"
echo "Network: RTL8168 10.0.2.15 (ADB port forwarded to 5555)"
echo "Monitor: QMP /tmp/qemu-qmp.sock, HMP /tmp/qemu-monitor.sock"
echo "========================================================"

exec "$QEMU" \
  -M rtd2841-tv030 \
  -m 1024M \
  -smp 4 \
  -dtb "$DTB" \
  -kernel "$KERNEL" \
  -drive file="$DISK",format="$DISK_FMT",if=sd$SNAPSHOT_OPT \
  -append "$CMDLINE" \
  -serial stdio \
  -nic user,model=rtd2841-eth,hostfwd=tcp::5555-:5555 \
  -qmp unix:/tmp/qemu-qmp.sock,server,nowait \
  -monitor unix:/tmp/qemu-monitor.sock,server,nowait \
  -display cocoa
