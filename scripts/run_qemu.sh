#!/usr/bin/env bash
# ==============================================================================
# run_qemu.sh - QEMU Launcher for Realtek RTD2841 / TCL RT41 Chassis Emulation
# ==============================================================================
set -euo pipefail

WORKSPACE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_BIN="${WORKSPACE_ROOT}/qemu/build/qemu-system-aarch64"
DTB="${WORKSPACE_ROOT}/output/kernel/rtd284x-tv030.dtb"
IMAGE="${1:-${WORKSPACE_ROOT}/staging/firmware_test.bin}"

if [ ! -f "${QEMU_BIN}" ]; then
    echo "[-] Error: QEMU binary not found at ${QEMU_BIN}" >&2
    exit 1
fi

if [ ! -f "${DTB}" ]; then
    echo "[-] Error: Device tree blob not found at ${DTB}" >&2
    exit 1
fi

QEMU_ARGS=(
    -M rtd2841-tv030
    -m 1024M
    -smp 4
    -dtb "${DTB}"
    -serial stdio
    -display "${DISPLAY_BACKEND:-cocoa}"
    -qmp unix:/tmp/qemu-monitor.sock,server,nowait
)

# Attach composite eMMC image to SDHCI host controller if present
EMMC_IMAGE="${WORKSPACE_ROOT}/staging/emmc.raw"
if [ -f "${EMMC_IMAGE}" ]; then
    echo "[+] Attaching composite eMMC disk image via SDHCI: ${EMMC_IMAGE}"
    QEMU_ARGS+=(-drive "file=${EMMC_IMAGE},format=raw,if=sd")
fi

if [ -n "${IMAGE}" ] && [ -f "${IMAGE}" ]; then
    filename="$(basename "${IMAGE}")"
    if [[ "${filename}" =~ vmlinux || "${filename}" =~ zImage || "${filename}" =~ kernel ]]; then
        echo "[+] Booting kernel image via -kernel: ${IMAGE}"
        QEMU_ARGS+=(
            -kernel "${IMAGE}"
            -append "console=tty0 console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init androidboot.selinux=permissive"
        )
    elif [[ "${filename}" == *.bin || "${filename}" == *.elf ]]; then
        echo "[+] Loading raw firmware/bare-metal binary via -bios: ${IMAGE}"
        QEMU_ARGS+=(-bios "${IMAGE}")
    else
        echo "[+] Booting kernel image via -kernel: ${IMAGE}"
        QEMU_ARGS+=(
            -kernel "${IMAGE}"
            -append "console=tty0 console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init androidboot.selinux=permissive"
        )
    fi
else
    echo "[+] Running bare-metal SoC simulation loop with DTB: ${DTB}"
fi

echo "[*] Launching QEMU RTD2841..."
exec "${QEMU_BIN}" "${QEMU_ARGS[@]}"
