#!/usr/bin/env bash
# ==============================================================================
# build_kernel.sh - Automated Kernel Cross-Compilation Pipeline for RTK2841
# Target SoC: Realtek RTD2841 / RTK284x (TCL RT41 Chassis, TV030 Board)
# Target Architecture: ARMv7-A / ARMv8-A in AArch32 Mode (Cortex-A53 32-bit armhf)
# Target Kernel: Linux 4.4.3+ (TCLOpenSource/rtk2841)
# Supported Hosts: macOS (Apple Silicon arm64 / x86_64) & Linux (Native / Docker)
# ==============================================================================

set -euo pipefail

# ANSI Color formatting
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

log_info()  { echo -e "${CYAN}[*]${NC} $*"; }
log_ok()    { echo -e "${GREEN}[+]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[!]${NC} $*"; }
log_error() { echo -e "${RED}[-]${NC} $*" >&2; }

# Determine CPU core count
if command -v nproc >/dev/null 2>&1; then
    NUM_JOBS=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    NUM_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
else
    NUM_JOBS=4
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Resolve Kernel Directory and Staging Root
if [ -f "${SCRIPT_DIR}/Makefile" ] && [ -d "${SCRIPT_DIR}/arch/arm" ]; then
    KERNEL_DIR="${SCRIPT_DIR}"
    WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
elif [ -d "${SCRIPT_DIR}/kernel/rtk2841" ]; then
    KERNEL_DIR="${SCRIPT_DIR}/kernel/rtk2841"
    WORKSPACE_ROOT="${SCRIPT_DIR}"
elif [ -d "${SCRIPT_DIR}/rtk2841" ]; then
    KERNEL_DIR="${SCRIPT_DIR}/rtk2841"
    WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
else
    WORKSPACE_ROOT="/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re"
    KERNEL_DIR="${WORKSPACE_ROOT}/kernel/rtk2841"
fi

# Resolve Output Directory: Honor environment variable, container standard, or workspace default
if [ -n "${OUTPUT_DIR:-}" ]; then
    OUTPUT_DIR="${OUTPUT_DIR}"
elif [ -d "/build/output" ]; then
    OUTPUT_DIR="/build/output"
elif [ -d "${WORKSPACE_ROOT}/output/kernel" ] || [ -d "${WORKSPACE_ROOT}/output" ]; then
    OUTPUT_DIR="${WORKSPACE_ROOT}/output/kernel"
else
    OUTPUT_DIR="${WORKSPACE_ROOT}/output/kernel"
fi

ARCH="arm"
DEFCONFIG="tv030_defconfig"
DTB_TARGET="rtd284x-tv030.dtb"
DTS_SOURCE="rtd284x-tv030.dts"

# Default toolchain prefix (can be overridden via CROSS_COMPILE env var)
DEFAULT_CROSS_COMPILE="arm-linux-gnueabihf-"
CROSS_COMPILE="${CROSS_COMPILE:-$DEFAULT_CROSS_COMPILE}"

HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

# Actions
ACTION_SPECIFIED=false
DO_DEFCONFIG=false
DO_DTB=false
DO_ZIMAGE=false
DO_CLEAN=false
USE_CONTAINER=false
SETUP_CS_VOLUME=false

usage() {
    local exit_code="${1:-0}"
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Automated Build Pipeline for Realtek RTD2841 / RTK284x Linux 4.4 Kernel

Options:
  --all                Execute full pipeline: defconfig, zImage, and dtb compilation
  --defconfig          Apply TV030 defconfig (tv030_defconfig)
  --dtb                Compile device tree blobs (${DTB_TARGET} and rtd284x.dtb)
  --zimage             Compile Linux kernel zImage, vmlinux.bin, and modules
  --container          Execute build inside a Docker/Podman container
  --clean              Clean kernel build artifacts (make mrproper)
  --setup-cs-volume    Create & mount a Case-sensitive APFS volume on macOS
  --jobs <N>           Number of parallel make jobs (default: ${NUM_JOBS})
  --output <DIR>       Destination directory for kernel artifacts (default: ${OUTPUT_DIR})
  --src <DIR>          Path to kernel source tree (default: ${KERNEL_DIR})
  -h, --help           Show this help message

Examples:
  $(basename "$0") --dtb                     # Build TV030 DTB natively on macOS
  $(basename "$0") --defconfig --dtb         # Configure and build DTB
  $(basename "$0") --all                     # Full build (autodetects container/native)
  $(basename "$0") --all --container         # Force Docker containerized full build
  $(basename "$0") --setup-cs-volume         # Mount case-sensitive APFS volume on macOS
EOF
    exit "${exit_code}"
}

# Parse command line options
while [ $# -gt 0 ]; do
    case "$1" in
        --all)
            ACTION_SPECIFIED=true
            DO_DEFCONFIG=true
            DO_DTB=true
            DO_ZIMAGE=true
            shift
            ;;
        --defconfig)
            ACTION_SPECIFIED=true
            DO_DEFCONFIG=true
            shift
            ;;
        --dtb)
            ACTION_SPECIFIED=true
            DO_DTB=true
            shift
            ;;
        --zimage)
            ACTION_SPECIFIED=true
            DO_ZIMAGE=true
            shift
            ;;
        --container)
            USE_CONTAINER=true
            shift
            ;;
        --clean)
            ACTION_SPECIFIED=true
            DO_CLEAN=true
            shift
            ;;
        --setup-cs-volume)
            ACTION_SPECIFIED=true
            SETUP_CS_VOLUME=true
            shift
            ;;
        --jobs)
            NUM_JOBS="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --src)
            KERNEL_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage 1
            ;;
    esac
done

# If no target action was explicitly set, default to building all targets
if [ "${ACTION_SPECIFIED}" = false ]; then
    DO_DEFCONFIG=true
    DO_DTB=true
    DO_ZIMAGE=true
fi

log_info "=========================================================="
log_info "RTK2841 Linux 4.4.3+ Kernel Cross-Compilation Automation"
log_info "=========================================================="
log_info "Host OS:         ${HOST_OS} (${HOST_ARCH})"
log_info "Kernel Tree:     ${KERNEL_DIR}"
log_info "Target Board:    TV030 (Realtek RTD2841 / RTK284x)"
log_info "Target CPU:      ARM Cortex-A53 (AArch32 / armv7l / armhf)"
log_info "Target DTB:      ${DTB_TARGET}"
log_info "Parallel Jobs:   ${NUM_JOBS}"
log_info "Output Dir:      ${OUTPUT_DIR}"

# ------------------------------------------------------------------------------
# 0. Case-Sensitive APFS Volume Setup (macOS host)
# ------------------------------------------------------------------------------
setup_cs_volume() {
    log_info "Setting up case-sensitive APFS volume on macOS..."
    if [ "${HOST_OS}" != "Darwin" ]; then
        log_error "--setup-cs-volume is only applicable on macOS (Darwin)."
        return 1
    fi

    local vol_name="tcl_rtk2841_cs"
    local mount_point="/Volumes/${vol_name}"
    local img_path="${WORKSPACE_ROOT}/rtk2841_cs.sparseimage"

    if [ -d "${mount_point}" ]; then
        log_ok "Case-sensitive volume already mounted at ${mount_point}"
        return 0
    fi

    if [ ! -f "${img_path}" ]; then
        log_info "Creating 25GB sparse disk image with Case-sensitive APFS..."
        hdiutil create -size 25g -type SPARSE -fs "Case-sensitive APFS" -volname "${vol_name}" "${img_path}"
    fi

    log_info "Mounting case-sensitive volume at ${mount_point}..."
    hdiutil attach "${img_path}" -mountpoint "${mount_point}"
    log_ok "Mounted case-sensitive volume at ${mount_point}"
    log_info "To use for builds without APFS collisions:"
    log_info "  git archive HEAD | tar -x -C ${mount_point}"
    log_info "  ${SCRIPT_DIR}/build_kernel.sh --src ${mount_point} --all"
    return 0
}

if [ "${SETUP_CS_VOLUME}" = true ]; then
    setup_cs_volume
    exit 0
fi

if [ ! -d "${KERNEL_DIR}" ]; then
    log_error "Kernel directory not found at: ${KERNEL_DIR}"
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"
cd "${KERNEL_DIR}"

# ------------------------------------------------------------------------------
# 1. Setup Source Symlinks, Config Tree & Portability Patches
# ------------------------------------------------------------------------------
setup_prerequisites() {
    log_info "Verifying TV030 configuration and device tree prerequisites..."

    # Ensure tv030_defconfig exists in arch/arm/configs/
    local target_defconfig="arch/arm/configs/tv030_defconfig"
    local source_defconfig="realtek/config.develop.android.pie.rtd284x.tv030.emmc.andtv.optee.2K-model.old_defconfig"
    if [ ! -e "${target_defconfig}" ] && [ ! -L "${target_defconfig}" ]; then
        log_info "Creating symlink: ${target_defconfig} -> ${source_defconfig}"
        ln -snf "${source_defconfig}" "${target_defconfig}"
    fi

    # Ensure rtd284x-tv030.dts exists in arch/arm/boot/dts/
    local target_dts="arch/arm/boot/dts/${DTS_SOURCE}"
    if [ ! -e "${target_dts}" ] && [ ! -L "${target_dts}" ]; then
        log_info "Creating symlink: ${target_dts} -> rtd284x.dts"
        ln -snf "rtd284x.dts" "${target_dts}"
    fi

    # Ensure arch/arm/boot/dts/Makefile contains rtd284x-tv030.dtb
    if ! grep -q "rtd284x-tv030.dtb" "arch/arm/boot/dts/Makefile"; then
        log_info "Patching arch/arm/boot/dts/Makefile to include ${DTB_TARGET}"
        sed -i '' 's/rtd284x\.dtb/rtd284x.dtb rtd284x-tv030.dtb/g' "arch/arm/boot/dts/Makefile" 2>/dev/null || \
        sed -i 's/rtd284x\.dtb/rtd284x.dtb rtd284x-tv030.dtb/g' "arch/arm/boot/dts/Makefile"
    fi

    # Ensure Darwin host compatibility for scripts/dtc and fix 64-bit pointer truncation
    if [ "${HOST_OS}" = "Darwin" ]; then
        if grep -q "//#include <string.h>" "scripts/dtc/libfdt/libfdt_env.h" 2>/dev/null; then
            log_info "Applying macOS Darwin host compatibility patch to scripts/dtc/libfdt/libfdt_env.h"
            cat << 'EOF' > /tmp/dtc_patch.sed
s|//#include <stdint.h>|#ifdef __APPLE__\
#include <stdint.h>\
#include <string.h>\
#else|
s|//#include <string.h>||
s|#include <linux/string.h>|#include <linux/string.h>\
#endif|
s|#define uintptr_t u_int32_t|#if defined(__APPLE__) \|\| defined(__x86_64__) \|\| defined(__aarch64__) \|\| defined(__arm64__)\
#define uintptr_t unsigned long\
#else\
#define uintptr_t u_int32_t\
#endif|
EOF
            sed -i '' -f /tmp/dtc_patch.sed "scripts/dtc/libfdt/libfdt_env.h" || true
            rm -f /tmp/dtc_patch.sed
        fi
    fi
}

# ------------------------------------------------------------------------------
# 2. Containerized Build Runner
# ------------------------------------------------------------------------------
run_in_container() {
    log_info "Preparing containerized build environment..."
    
    local container_runtime=""
    if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
        container_runtime="docker"
    elif command -v podman >/dev/null 2>&1 && podman info >/dev/null 2>&1; then
        container_runtime="podman"
    fi

    if [ -z "${container_runtime}" ]; then
        log_error "No working Docker or Podman daemon found on this host!"
        log_warn "To run containerized kernel cross-compilation on macOS:"
        log_warn "  1. Start Docker Desktop, Colima ('colima start'), or OrbStack."
        log_warn "  2. Ensure '${container_runtime:-docker}' CLI is in your PATH and daemon is running."
        log_warn "  3. Re-run: ./build_kernel.sh --all --container"
        return 1
    fi

    local image_name="rtk2841-builder:latest"
    local dockerfile="${WORKSPACE_ROOT}/Dockerfile.kernel"
    if [ ! -f "${dockerfile}" ]; then
        dockerfile="${KERNEL_DIR}/Dockerfile"
    fi

    log_info "Building cross-compilation container image (${image_name}) using ${container_runtime}..."
    # Build container directly from Dockerfile (using stdin to avoid transmitting multi-gigabyte workspace context)
    "${container_runtime}" build -t "${image_name}" - < "${dockerfile}"

    log_info "Running build inside ${container_runtime} container..."
    local container_cmd="
        set -e
        echo '[*] Inside container: Initializing build environment...'
        mkdir -p /build/output
        cd /build/kernel

        # Verify filesystem case-sensitivity and fix any APFS collisions if needed
        touch .case_test_upper
        if [ -f .case_test_UPPER ]; then
            echo '[!] Warning: Mounted kernel directory is on a case-insensitive host filesystem!'
            echo '[*] Extracting clean tree to container internal ext4 storage to restore collided headers...'
            mkdir -p /tmp/kernel_build
            if [ -d .git ]; then
                git archive HEAD | tar -x -C /tmp/kernel_build
            else
                cp -a /build/kernel/. /tmp/kernel_build/
            fi
            cd /tmp/kernel_build
        fi
        rm -f .case_test_upper 2>/dev/null || true

        # Ensure TV030 symlinks exist
        mkdir -p arch/arm/configs arch/arm/boot/dts
        [ ! -e arch/arm/configs/tv030_defconfig ] && ln -snf realtek/config.develop.android.pie.rtd284x.tv030.emmc.andtv.optee.2K-model.old_defconfig arch/arm/configs/tv030_defconfig
        [ ! -e arch/arm/boot/dts/rtd284x-tv030.dts ] && ln -snf rtd284x.dts arch/arm/boot/dts/rtd284x-tv030.dts

        echo '[*] Inside container: Applying defconfig (tv030_defconfig)...'
        make ARCH=arm tv030_defconfig

        echo '[*] Inside container: Compiling device tree blobs and generating embedded_dtb...'
        make -j${NUM_JOBS} ARCH=arm dtbs
        [ -f arch/arm/boot/dts/rtd284x-tv030.dtb ] && cp -v arch/arm/boot/dts/rtd284x-tv030.dtb /build/output/
        [ -f arch/arm/boot/dts/rtd284x.dtb ] && cp -v arch/arm/boot/dts/rtd284x.dtb /build/output/
        [ -f embedded_dtb ] && cp -v embedded_dtb /build/output/

        echo '[*] Inside container: Compiling kernel zImage, vmlinux.bin, and modules...'
        make -j${NUM_JOBS} ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- all zImage modules

        echo '[*] Inside container: Staging outputs...'
        [ -f arch/arm/boot/zImage ] && cp -v arch/arm/boot/zImage /build/output/
        [ -f vmlinux.bin ] && cp -v vmlinux.bin /build/output/
        [ -f vmlinux ] && cp -v vmlinux /build/output/
        [ -f System.map ] && cp -v System.map /build/output/

        if [ ! -f /build/output/zImage ] && [ ! -f /build/output/vmlinux.bin ]; then
            echo '[-] ERROR: Neither zImage nor vmlinux.bin was generated!'
            exit 1
        fi
        echo '[+] Container build complete and verified!'
    "

    "${container_runtime}" run --rm \
        -v "${KERNEL_DIR}:/build/kernel" \
        -v "${OUTPUT_DIR}:/build/output" \
        -e OUTPUT_DIR=/build/output \
        -e NUM_JOBS="${NUM_JOBS}" \
        -w "/build/kernel" \
        "${image_name}" \
        /bin/bash -c "${container_cmd}"

    log_ok "Containerized build finished successfully."
    return 0
}

# ------------------------------------------------------------------------------
# 3. Clean Target
# ------------------------------------------------------------------------------
if [ "${DO_CLEAN}" = true ]; then
    log_info "Cleaning kernel tree..."
    make ARCH="${ARCH}" clean
    log_ok "Clean complete."
    exit 0
fi

setup_prerequisites

# ------------------------------------------------------------------------------
# 4. Defconfig Target
# ------------------------------------------------------------------------------
if [ "${DO_DEFCONFIG}" = true ]; then
    log_info "Applying defconfig: ${DEFCONFIG}..."
    make ARCH="${ARCH}" "${DEFCONFIG}"
    log_ok "Kernel configuration written to .config successfully."
fi

# ------------------------------------------------------------------------------
# 5. Device Tree Blob Compilation Target
# ------------------------------------------------------------------------------
compile_dtb() {
    log_info "Compiling Device Tree Blobs: ${DTB_TARGET} and rtd284x.dtb..."

    # Check if host dtc is available or build local dtc
    local local_dtc="${KERNEL_DIR}/scripts/dtc/dtc"
    if [ ! -x "${local_dtc}" ]; then
        log_info "Building local scripts/dtc/dtc host tool..."
        make ARCH="${ARCH}" scripts/dtc/dtc 2>/dev/null || \
        make -C scripts/dtc dtc 2>/dev/null || \
        {
            log_info "Compiling scripts/dtc directly using host compiler..."
            mkdir -p scripts/dtc
            ${CC:-cc} -O2 -Iscripts/dtc -Iscripts/dtc/libfdt \
                scripts/dtc/dtc.c scripts/dtc/flattree.c scripts/dtc/fstree.c \
                scripts/dtc/data.c scripts/dtc/livetree.c scripts/dtc/treesource.c \
                scripts/dtc/srcpos.c scripts/dtc/checks.c scripts/dtc/util.c \
                scripts/dtc/dtc-lexer.lex.c scripts/dtc/dtc-parser.tab.c \
                -o "${local_dtc}"
        }
    fi

    if [ -x "${local_dtc}" ]; then
        log_info "Compiling ${DTB_TARGET} using ${local_dtc}..."
        "${local_dtc}" -I dts -O dtb -o "arch/arm/boot/dts/${DTB_TARGET}" \
            -i arch/arm/boot/dts "arch/arm/boot/dts/${DTS_SOURCE}"
        
        "${local_dtc}" -I dts -O dtb -o "arch/arm/boot/dts/rtd284x.dtb" \
            -i arch/arm/boot/dts "arch/arm/boot/dts/rtd284x.dts"
    else
        log_info "Local dtc build skipped; attempting kernel kbuild target..."
        make ARCH="${ARCH}" "${DTB_TARGET}"
    fi

    # Verify compiled DTBs and stage to output directory AND update embedded_dtb
    if [ -f "arch/arm/boot/dts/${DTB_TARGET}" ]; then
        cp -v "arch/arm/boot/dts/${DTB_TARGET}" "${OUTPUT_DIR}/"
        cp -v "arch/arm/boot/dts/rtd284x.dtb" "${OUTPUT_DIR}/"
        cp -v "arch/arm/boot/dts/${DTB_TARGET}" "${KERNEL_DIR}/embedded_dtb"
        local dtb_size
        dtb_size=$(wc -c < "${OUTPUT_DIR}/${DTB_TARGET}" | tr -d ' ')
        log_ok "Successfully compiled ${DTB_TARGET} (${dtb_size} bytes)"
        log_ok "Artifact staged at: ${OUTPUT_DIR}/${DTB_TARGET}"
        log_ok "Embedded DTB updated at: ${KERNEL_DIR}/embedded_dtb"
    else
        log_error "Failed to produce ${DTB_TARGET}"
        return 1
    fi
}

# ------------------------------------------------------------------------------
# 6. zImage & Kernel Binary Compilation Target
# ------------------------------------------------------------------------------
compile_zimage() {
    log_info "Initiating Linux kernel zImage compilation..."

    local has_cross_gcc=false
    if command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1; then
        has_cross_gcc=true
    fi

    if [ "${USE_CONTAINER}" = true ] || ([ "${HOST_OS}" = "Darwin" ] && [ "${has_cross_gcc}" = false ]); then
        log_info "Target host is macOS (${HOST_ARCH}) without native '${CROSS_COMPILE}gcc'."
        log_info "Switching to Docker/Podman containerized pipeline..."
        if run_in_container; then
            log_ok "zImage compilation completed via container."
            return 0
        else
            log_error "Containerized kernel compilation failed!"
            log_warn "If running locally without Docker, you can still compile DTB natively:"
            log_warn "  ./build_kernel.sh --dtb"
            log_warn "To enable full zImage compilation:"
            log_warn "  1. Start Docker Desktop, Colima ('colima start'), or OrbStack."
            log_warn "  2. Re-run: ./build_kernel.sh --all --container"
            return 1
        fi
    else
        log_info "Executing native make with CROSS_COMPILE=${CROSS_COMPILE} (-j${NUM_JOBS})..."
        # Ensure DTBs and embedded_dtb are built first to satisfy dtb.o dependency
        make -j"${NUM_JOBS}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" dtbs
        make -j"${NUM_JOBS}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" all zImage modules

        if [ -f "arch/arm/boot/zImage" ] || [ -f "vmlinux.bin" ]; then
            [ -f "arch/arm/boot/zImage" ] && cp -v "arch/arm/boot/zImage" "${OUTPUT_DIR}/"
            [ -f "vmlinux.bin" ] && cp -v "vmlinux.bin" "${OUTPUT_DIR}/"
            [ -f "vmlinux" ] && cp -v "vmlinux" "${OUTPUT_DIR}/"
            [ -f "System.map" ] && cp -v "System.map" "${OUTPUT_DIR}/"
            log_ok "Kernel binaries successfully compiled and staged at: ${OUTPUT_DIR}/"
            return 0
        else
            log_error "Failed to produce kernel binary (zImage or vmlinux.bin)!"
            return 1
        fi
    fi
}

if [ "${DO_DTB}" = true ]; then
    if ! compile_dtb; then
        log_error "DTB compilation failed."
        exit 1
    fi
fi

if [ "${DO_ZIMAGE}" = true ]; then
    if ! compile_zimage; then
        log_error "zImage compilation target failed."
        exit 1
    fi
fi

# ------------------------------------------------------------------------------
# 7. Summary & Checksums
# ------------------------------------------------------------------------------
log_info "=========================================================="
log_info "Build Pipeline Execution Summary"
log_info "=========================================================="
if [ -d "${OUTPUT_DIR}" ] && [ "$(ls -A "${OUTPUT_DIR}" 2>/dev/null)" ]; then
    log_info "Staged Artifacts in ${OUTPUT_DIR}:"
    ls -lh "${OUTPUT_DIR}"
    (
        cd "${OUTPUT_DIR}"
        rm -f SHA256SUMS
        # Hash regular files only, explicitly omitting SHA256SUMS to avoid self-hashing
        local_files=$(find . -maxdepth 1 -type f ! -name "SHA256SUMS" -print | sed 's|^\./||' | sort)
        if [ -n "${local_files}" ]; then
            if command -v shasum >/dev/null 2>&1; then
                shasum -a 256 ${local_files} > SHA256SUMS
            elif command -v sha256sum >/dev/null 2>&1; then
                sha256sum ${local_files} > SHA256SUMS
            fi
            log_ok "Checksums written to ${OUTPUT_DIR}/SHA256SUMS"
            # Verify checksum file integrity
            if command -v shasum >/dev/null 2>&1; then
                shasum -c SHA256SUMS
            elif command -v sha256sum >/dev/null 2>&1; then
                sha256sum -c SHA256SUMS
            fi
        fi
    )
    log_ok "All checksums verified successfully!"
fi

log_ok "Pipeline task completed."
