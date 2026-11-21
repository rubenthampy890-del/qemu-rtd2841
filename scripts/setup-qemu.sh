#!/usr/bin/env bash
# Build and test the public RTD2841 hardware model.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd -P)
src="$root/build/qemu-src"
pin=a19f59a72d96056d9a8f48cf6c77f5787491e414
board_patch="$root/patches/qemu-rtd2841-complete.patch"

if [[ $# -gt 1 || (${1:-} != "" && ${1:-} != "--demo") ]]; then
    echo "Usage: $0 [--demo]" >&2
    exit 2
fi
case "$src" in *" "*|*:*) echo "Clone under a path without spaces or colons, such as ~/qemu-work/qemu-rtd2841." >&2; exit 1;; esac
case "$(uname -s)" in
    Darwin) display=cocoa ;;
    Linux) display=gtk ;;
    *) echo "This setup supports macOS and Linux." >&2; exit 1 ;;
esac
python_bin=${PYTHON:-python3}
if [[ -z ${PYTHON:-} && "$display" == cocoa ]] && command -v python3.14 >/dev/null; then
    python_bin=python3.14
fi
for tool in "$python_bin" git make ninja pkg-config dtc arm-none-eabi-as arm-none-eabi-ld arm-none-eabi-objcopy; do
    command -v "$tool" >/dev/null || { echo "Missing $tool. Install the host packages listed in README.md." >&2; exit 1; }
done
"$python_bin" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 12) else "QEMU requires Python 3.12 or later. Set PYTHON to its executable.")'
if [[ ! -e "$src" ]]; then
    mkdir -p "$root/build"
    checkout=$(mktemp -d "$root/build/qemu-download.XXXXXX")
    git -C "$checkout" init -q
    git -C "$checkout" remote add origin https://gitlab.com/qemu-project/qemu.git
    git -C "$checkout" fetch --depth 1 origin "$pin"
    git -C "$checkout" checkout --detach FETCH_HEAD
    mv "$checkout" "$src"
fi
[[ -d "$src/.git" && $(git -C "$src" rev-parse HEAD) == "$pin" ]] || {
    echo "Existing $src is not the pinned QEMU checkout. Use a fresh project directory." >&2; exit 1;
}
if git -C "$src" apply --check --reverse "$board_patch" 2>/dev/null; then
    echo "Board patch already applied."
else
    git -C "$src" apply --check "$board_patch"
    git -C "$src" apply "$board_patch"
fi
mkdir -p "$src/build"
cd "$src/build"
if [[ ! -f build.ninja ]]; then
    ../configure --target-list=aarch64-softmmu --enable-slirp --enable-pixman \
        --enable-tools "--enable-$display" --disable-rust --disable-werror \
        --disable-docs --python="$(command -v "$python_bin")"
fi
ninja -j"${JOBS:-4}" qemu-system-aarch64 qemu-img
./qemu-system-aarch64 -display help | grep -w "$display"
cd "$root"
make -B -C tests
dtc -I dts -O dtb -o dts/rtd284x-tv030.dtb dts/rtd284x-tv030.dts
QEMU_BIN="$src/build/qemu-system-aarch64" QEMU_IMG="$src/build/qemu-img" \
    "$python_bin" tests/run_ci_test.py
if [[ ${1:-} == --demo ]]; then
    exec "$src/build/qemu-system-aarch64" -M rtd2841-tv030 -m 1024M -smp 4 \
        -dtb dts/rtd284x-tv030.dtb -kernel tests/firmware_fb_test.bin \
        -nic user,model=rtd2841-eth -monitor none -serial mon:stdio -display "$display"
fi
