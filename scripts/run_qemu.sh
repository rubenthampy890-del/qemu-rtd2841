#!/usr/bin/env bash
# ==============================================================================
# run_qemu.sh - QEMU Launcher for Realtek RTD2841 / TCL RT41 Chassis Emulation
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Resolve QEMU binary: environment variable, local build, or system PATH
if [ -n "${QEMU_BIN:-}" ] && [ -x "${QEMU_BIN}" ]; then
    QEMU="${QEMU_BIN}"
elif [ -f "${REPO_ROOT}/../tcl-firmware-re/qemu/build/qemu-system-aarch64" ]; then
    QEMU="${REPO_ROOT}/../tcl-firmware-re/qemu/build/qemu-system-aarch64"
elif [ -f "${REPO_ROOT}/qemu/build/qemu-system-aarch64" ]; then
    QEMU="${REPO_ROOT}/qemu/build/qemu-system-aarch64"
elif command -v qemu-system-aarch64 >/dev/null 2>&1; then
    QEMU="$(command -v qemu-system-aarch64)"
else
    echo "[-] Error: qemu-system-aarch64 not found. Build QEMU with the rtd2841 patch or set QEMU_BIN." >&2
    exit 1
fi

DTB="${REPO_ROOT}/dts/rtd284x-tv030.dtb"
IMAGE="${1:-${REPO_ROOT}/tests/firmware_test.bin}"

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
    -display "${DISPLAY_BACKEND:-none}"
)

if [ -n "${IMAGE}" ] && [ -f "${IMAGE}" ]; then
    case "${IMAGE}" in
        *.bin)
            echo "[+] Loading raw firmware/bare-metal binary via -bios: ${IMAGE}"
            QEMU_ARGS+=(-bios "${IMAGE}")
            ;;
        *)
            echo "[+] Booting kernel image via -kernel: ${IMAGE}"
            QEMU_ARGS+=(
                -kernel "${IMAGE}"
                -append "console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rw init=/init"
            )
            ;;
    esac
else
    echo "[+] Running bare-metal SoC simulation loop with DTB: ${DTB}"
fi

echo "[*] Launching: ${QEMU} ${QEMU_ARGS[*]}"
exec "${QEMU}" "${QEMU_ARGS[@]}"
