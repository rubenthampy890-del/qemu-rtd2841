#!/usr/bin/env bash
# ==============================================================================
# run_qemu.sh - QEMU Launcher for Realtek RTD2841 / TCL RT41 Chassis Emulation
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Resolve QEMU binary
if [ -n "${QEMU_BIN:-}" ] && [ -x "${QEMU_BIN}" ]; then
    QEMU="${QEMU_BIN}"
elif [ -f "${REPO_ROOT}/../tcl-firmware-re/qemu/build/qemu-system-aarch64" ]; then
    QEMU="${REPO_ROOT}/../tcl-firmware-re/qemu/build/qemu-system-aarch64"
elif [ -f "${REPO_ROOT}/qemu/build/qemu-system-aarch64" ]; then
    QEMU="${REPO_ROOT}/qemu/build/qemu-system-aarch64"
elif command -v qemu-system-aarch64 >/dev/null 2>&1; then
    QEMU="$(command -v qemu-system-aarch64)"
else
    echo "[-] Error: qemu-system-aarch64 not found. Set QEMU_BIN to the compiled QEMU binary." >&2
    exit 1
fi

# Resolve Device Tree Blob
if [ -f "${REPO_ROOT}/dts/rtd284x-tv030.dtb" ]; then
    DTB="${REPO_ROOT}/dts/rtd284x-tv030.dtb"
elif [ -f "${REPO_ROOT}/output/kernel/rtd284x-tv030.dtb" ]; then
    DTB="${REPO_ROOT}/output/kernel/rtd284x-tv030.dtb"
else
    echo "[-] Error: Device tree blob not found." >&2
    exit 1
fi

IMAGE="${1:-}"

QEMU_ARGS=(
    -M rtd2841-tv030
    -m 1024M
    -smp 4
    -dtb "${DTB}"
    -serial stdio
    -display "${DISPLAY_BACKEND:-cocoa}"
)

# Attach composite eMMC image to SDHCI host controller if present
EMMC_IMAGE=""
if [ -n "${SD_IMAGE:-}" ] && [ -f "${SD_IMAGE}" ]; then
    EMMC_IMAGE="${SD_IMAGE}"
elif [ -f "${REPO_ROOT}/staging/emmc.raw" ]; then
    EMMC_IMAGE="${REPO_ROOT}/staging/emmc.raw"
elif [ -f "${REPO_ROOT}/../tcl-firmware-re/staging/emmc.raw" ]; then
    EMMC_IMAGE="${REPO_ROOT}/../tcl-firmware-re/staging/emmc.raw"
fi

if [ -n "${EMMC_IMAGE}" ]; then
    echo "[+] Attaching composite eMMC disk image via SDHCI: ${EMMC_IMAGE}"
    QEMU_ARGS+=(-drive "file=${EMMC_IMAGE},format=raw,if=sd")
fi

if [ -n "${IMAGE}" ] && [ -f "${IMAGE}" ]; then
    filename="$(basename "${IMAGE}")"
    if [[ "${filename}" =~ vmlinux || "${filename}" =~ zImage || "${filename}" =~ kernel ]]; then
        echo "[+] Booting kernel image via -kernel: ${IMAGE}"
        QEMU_ARGS+=(
            -kernel "${IMAGE}"
            -append "console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init"
        )
    elif [[ "${filename}" == *.bin || "${filename}" == *.elf ]]; then
        echo "[+] Loading raw firmware/bare-metal binary via -bios: ${IMAGE}"
        QEMU_ARGS+=(-bios "${IMAGE}")
    else
        echo "[+] Booting kernel image via -kernel: ${IMAGE}"
        QEMU_ARGS+=(
            -kernel "${IMAGE}"
            -append "console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init"
        )
    fi
else
    echo "[+] Running bare-metal SoC simulation loop with DTB: ${DTB}"
fi

echo "[*] Launching QEMU RTD2841..."
exec "${QEMU}" "${QEMU_ARGS[@]}"
