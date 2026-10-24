#!/usr/bin/env bash
# ==============================================================================
# run_qemu.sh - QEMU Launcher for Realtek RTD2841 / TCL RT41 Chassis Emulation
# ==============================================================================
set -euo pipefail

# Resolve repository root directory (parent of scripts/)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Resolve QEMU binary: environment variable -> local build trees -> system PATH
QEMU_BIN="${QEMU_BIN:-}"
if [ -z "${QEMU_BIN}" ]; then
    for cand in \
        "${REPO_ROOT}/qemu-upstream/build/qemu-system-aarch64" \
        "${REPO_ROOT}/qemu/build/qemu-system-aarch64" \
        "${REPO_ROOT}/../tcl-firmware-re/qemu/build/qemu-system-aarch64" \
        "${HOME}/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64" \
        "/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re/qemu/build/qemu-system-aarch64" \
        "$(command -v qemu-system-aarch64 2>/dev/null || true)"; do
        if [ -n "${cand}" ] && [ -x "${cand}" ]; then
            QEMU_BIN="${cand}"
            break
        fi
    done
fi

if [ -z "${QEMU_BIN}" ] || [ ! -x "${QEMU_BIN}" ]; then
    echo "[-] Error: qemu-system-aarch64 binary not found." >&2
    echo "    Please build QEMU following BUILD.md, or specify QEMU_BIN=/path/to/qemu-system-aarch64" >&2
    exit 1
fi

# Resolve Device Tree Blob
DTB="${DTB:-}"
if [ -z "${DTB}" ]; then
    for cand in \
        "${REPO_ROOT}/dts/rtd284x-tv030.dtb" \
        "${REPO_ROOT}/output/kernel/rtd284x-tv030.dtb"; do
        if [ -f "${cand}" ]; then
            DTB="${cand}"
            break
        fi
    done
fi

if [ -z "${DTB}" ] || [ ! -f "${DTB}" ]; then
    echo "[-] Error: Device tree blob (rtd284x-tv030.dtb) not found." >&2
    echo "    Compile with: dtc -I dts -O dtb -o ${REPO_ROOT}/dts/rtd284x-tv030.dtb ${REPO_ROOT}/dts/rtd284x-tv030.dts" >&2
    exit 1
fi

# Target execution payload (defaults to live kernel if available, else firmware_test.bin)
IMAGE="${1:-}"
if [ -z "${IMAGE}" ]; then
    if [ -f "${REPO_ROOT}/output/kernel/zImage" ]; then
        IMAGE="${REPO_ROOT}/output/kernel/zImage"
    elif [ -f "${REPO_ROOT}/../tcl-firmware-re/output/kernel/zImage" ]; then
        IMAGE="${REPO_ROOT}/../tcl-firmware-re/output/kernel/zImage"
    else
        IMAGE="${REPO_ROOT}/tests/firmware_test.bin"
    fi
fi

# Auto-detect display backend if unspecified
if [ -z "${DISPLAY_BACKEND:-}" ]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        DISPLAY_BACKEND="cocoa"
    else
        DISPLAY_BACKEND="default"
    fi
fi

# Monitor and Control socket configuration
# Supports -monitor unix:/tmp/qemu-monitor.sock,server,nowait or both QMP and HMP cleanly.
QEMU_MONITOR_SOCK="${QEMU_MONITOR_SOCK:-/tmp/qemu-monitor.sock}"
QEMU_QMP_SOCK="${QEMU_QMP_SOCK:-/tmp/qemu-qmp.sock}"
QEMU_MONITOR_MODE="${QEMU_MONITOR_MODE:-both}"

QEMU_ARGS=(
    -M rtd2841-tv030
    -m 1024M
    -smp 4
    -dtb "${DTB}"
    -serial stdio
    -display "${DISPLAY_BACKEND}"
    -nic "user,model=rtd2841-eth,hostfwd=tcp::5555-:5555"
)

case "${QEMU_MONITOR_MODE}" in
    qmp)
        QEMU_ARGS+=(-qmp "unix:${QEMU_MONITOR_SOCK},server,nowait")
        ;;
    hmp)
        QEMU_ARGS+=(-monitor "unix:${QEMU_MONITOR_SOCK},server,nowait")
        ;;
    both|*)
        QEMU_ARGS+=(
            -monitor "unix:${QEMU_MONITOR_SOCK},server,nowait"
            -qmp "unix:${QEMU_QMP_SOCK},server,nowait"
        )
        ;;
esac

# Attach raw eMMC storage image to SDHCI host controller if present
STORAGE_IMAGE="${STORAGE_IMAGE:-}"
if [ -z "${STORAGE_IMAGE}" ]; then
    for cand in \
        "${REPO_ROOT}/storage.img" \
        "${REPO_ROOT}/staging/emmc.raw" \
        "${REPO_ROOT}/../tcl-firmware-re/staging/emmc.raw"; do
        if [ -f "${cand}" ]; then
            STORAGE_IMAGE="${cand}"
            break
        fi
    done
fi

if [ -n "${STORAGE_IMAGE}" ] && [ -f "${STORAGE_IMAGE}" ]; then
    echo "[+] Attaching composite eMMC disk image via SDHCI: ${STORAGE_IMAGE}"
    QEMU_ARGS+=(-drive "file=${STORAGE_IMAGE},format=raw,if=sd")
fi

if [ -n "${IMAGE}" ] && [ -f "${IMAGE}" ]; then
    filename="$(basename "${IMAGE}")"
    if [[ "${filename}" =~ vmlinux || "${filename}" =~ zImage || "${filename}" =~ kernel ]]; then
        echo "[+] Booting kernel image via -kernel: ${IMAGE}"
        QEMU_ARGS+=(
            -kernel "${IMAGE}"
            -append "console=tty0 console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait init=/init androidboot.selinux=permissive consoleblank=0"
        )
    elif [[ "${filename}" == *.bin || "${filename}" == *.elf ]]; then
        echo "[+] Loading bare-metal test binary via -kernel: ${IMAGE}"
        QEMU_ARGS+=(-kernel "${IMAGE}")
    else
        echo "[+] Booting kernel image via -kernel: ${IMAGE}"
        QEMU_ARGS+=(
            -kernel "${IMAGE}"
            -append "console=tty0 console=ttyS0,115200 earlyprintk root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait init=/init androidboot.selinux=permissive consoleblank=0"
        )
    fi
else
    echo "[+] Running bare-metal SoC simulation loop with DTB: ${DTB}"
fi

echo "[*] Launching RTD2841 on ${QEMU_BIN}..."
exec "${QEMU_BIN}" "${QEMU_ARGS[@]}"
