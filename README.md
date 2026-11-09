# OpenDTV — Realtek RTD2841 Smart TV Emulator

[![CI Build & Emulation Test](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml/badge.svg)](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml)
[![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)](LICENSE)

OpenDTV is an open-source QEMU system emulator for the **Realtek RTD2841 / RTK284x SoC** (TCL RT41 chassis, board profile `tv030`). The RTD2841 powers commercial Android Smart TVs with four Cortex-A53 cores running **32-bit ARM (AArch32)** firmware under Linux 4.4.3 and Android 9 Pie (UnionTV).

---

## Emulated Capabilities

- **Hardware Silicon**: Quad-core Cortex-A53 in ARM32 (AArch32) mode, GICv2 interrupt distributor (256 INTIDs), Realtek TC0 27 MHz monotonic timer, SDHCI eMMC controller, and RTL8168 Ethernet via QEMU SLIRP.
- **Linux 4.4 Kernel**: Full 4-core SMP boot, ext4 rootfs and vendor partition mounting, and DHCP networking.
- **Android 9 Pie Userspace**: Custom static PID 1 (`mini_init`), Bionic property trie with futex notifications, all three Project Treble Binder domains (`servicemanager`, `hwservicemanager`, `vndservicemanager`), live ART/Zygote runtime, `logd` logging with `logcat`, SurfaceFlinger composition, and persistent `/data` storage.

---

## Beginner Quickstart: From Zero to Running in 5 Steps

You can build and test the emulator on your Mac or Linux computer in under 10 minutes without needing any proprietary TV firmware.

### 1. Install Prerequisites

Choose your operating system:

#### macOS (Apple Silicon or Intel)
Make sure Apple Command Line Tools and [Homebrew](https://brew.sh) are installed, then run:
```bash
brew install git ninja pkgconf glib pixman libslirp dtc python@3.14 arm-none-eabi-binutils
export PYTHON="$(brew --prefix python@3.14)/bin/python3.14"
export OPENDTV_DISPLAY=cocoa
```

#### Ubuntu 24.04 LTS
```bash
sudo apt update
sudo apt install -y build-essential git ninja-build pkg-config \
    libglib2.0-dev libpixman-1-dev libslirp-dev libfdt-dev zlib1g-dev \
    libgtk-3-dev device-tree-compiler python3 python3-venv \
    binutils-arm-none-eabi ca-certificates
export PYTHON=python3
export OPENDTV_DISPLAY=gtk
```

---

### 2. Clone the OpenDTV Repository
```bash
git clone https://github.com/rubenthampy890-del/qemu-rtd2841.git
cd qemu-rtd2841
export OPENDTV_ROOT="$PWD"
```

---

### 3. Build Patched QEMU
Download the pinned upstream QEMU source, apply our RTD2841 machine model patch, and compile:

```bash
# 1. Fetch pinned QEMU
export QEMU_SRC="$OPENDTV_ROOT/build/qemu-src"
mkdir -p "$QEMU_SRC"
git -C "$QEMU_SRC" init
git -C "$QEMU_SRC" remote add origin https://gitlab.com/qemu-project/qemu.git
git -C "$QEMU_SRC" fetch --depth 1 origin a19f59a72d96056d9a8f48cf6c77f5787491e414
git -C "$QEMU_SRC" checkout --detach FETCH_HEAD

# 2. Apply RTD2841 hardware patch
git -C "$QEMU_SRC" apply "$OPENDTV_ROOT/patches/qemu-rtd2841-complete.patch"

# 3. Configure and compile
mkdir -p "$QEMU_SRC/build"
cd "$QEMU_SRC/build"
../configure --target-list=aarch64-softmmu --enable-slirp --enable-pixman \
    --enable-tools --enable-$OPENDTV_DISPLAY --disable-rust --disable-werror \
    --disable-docs --python="$PYTHON"
ninja -j4 qemu-system-aarch64 qemu-img

cd "$OPENDTV_ROOT"
export QEMU_BIN="$QEMU_SRC/build/qemu-system-aarch64"
export QEMU_IMG="$QEMU_SRC/build/qemu-img"
```

---

### 4. Build Test Diagnostics & Device Tree
```bash
make -C tests clean && make -C tests
dtc -I dts -O dtb -o dts/rtd284x-tv030.dtb dts/rtd284x-tv030.dts
```

---

### 5. Run the Automated Hardware Verification
Run the automated test runner to verify CPU execution, timers, GICv2 interrupts, and network packets:
```bash
QEMU_BIN="$QEMU_BIN" "$PYTHON" tests/run_ci_test.py
```
*Expected output: `[>>> ALL CI HARDWARE AUTOMATION TESTS PASSED <<<]`.*

---

## Launch the Display (Color Bar Test)

To open the emulator window and verify 1080p display output:
```bash
"$QEMU_BIN" -M rtd2841-tv030 -m 1024M -smp 4 \
    -dtb "$OPENDTV_ROOT/dts/rtd284x-tv030.dtb" \
    -kernel "$OPENDTV_ROOT/tests/firmware_fb_test.bin" \
    -nic user,model=rtd2841-eth \
    -monitor none -serial mon:stdio -display "$OPENDTV_DISPLAY"
```
*You will see the 1920×1080 SMPTE color bar pattern. To quit, hold `Ctrl`, press `A`, release both, then press `X`.*

---

## Running the Android 9 Pie Userspace

If you have compatible firmware assets (`staging/emmc.raw` and a patched `zImage`), you can launch the complete Android userspace with a single command:

```bash
./tools/run_emulator_gui.sh -m android
```

### What Happens During Android Boot:
1. **Linux 4.4 Kernel**: Boots with 4 SMP cores online, mounts eMMC partitions, and configures `eth0` networking via DHCP.
2. **Custom PID 1 (`mini_init`)**: Mounts `/system` and `/vendor`, initializes the shared-memory Bionic property trie (`/dev/__properties__`), and sets `ro.build.type=eng`.
3. **Treble IPC Triad**: Starts `servicemanager` (Binder), `hwservicemanager` (HwBinder), and `vndservicemanager` (VndBinder).
4. **Graphics & Daemons**: Starts `SurfaceFlinger`, `android.hardware.graphics.allocator@2.0`, `android.hardware.graphics.composer@2.1`, `logd`, `tombstoned`, and `installd`.
5. **Zygote & ART**: Spawns `app_process32` (Zygote), executes `patchoat` to verify `/system/framework/boot.art`, and listens on `/dev/socket/zygote`.

### Interacting via ADB:
Once booted, connect to the guest ADB server from another host terminal:
```bash
adb connect 127.0.0.1:5555
adb shell
```
From the root shell, you can inspect running daemons, test storage persistence, and view real-time Android logs:
```bash
# Check running Android daemons
ps -A

# Stream live system logs
logcat -d -t 50

# Test persistent storage
echo "hello" > /data/test.txt && sync
```

---

## Repository Structure

```text
qemu-rtd2841/
├── qemu-model/               RTD2841 board, timer, and IR model source
├── patches/                  QEMU machine patch and Linux 4.4 kernel patch
├── dts/                      Device tree source (rtd284x-tv030.dts) and compiled DTB
├── tests/                    ARM32 assembly diagnostics and automated CI runner
├── tools/                    mini_init, su, software_egl_gles2, and GUI launcher
├── scripts/                  Kernel build and QEMU helper scripts
└── build/                    Local build artifacts (ignored by Git)
    └── qemu-src/             Pinned upstream QEMU source and compiled binaries
```

---

## Troubleshooting Guide

| Issue | Cause | Solution |
|---|---|---|
| `brew: command not found` | Homebrew is not in your shell PATH | Add `eval "$(/opt/homebrew/bin/brew shellenv)"` to `~/.zprofile` and reopen terminal. |
| `Unable to locate package` on Ubuntu | Ubuntu Universe repository disabled | Run `sudo add-apt-repository universe && sudo apt update`. |
| `main directory cannot contain spaces nor colons` | QEMU build path has spaces | Move the repository to a simple path like `~/qemu-rtd2841`. |
| `patch does not apply` | QEMU source is not clean | Ensure you checked out exact commit `a19f59a72d96056d9a8f48cf6c77f5787491e414`. |
| `ninja: build stopped: subcommand failed` | Host ran out of RAM | Run `ninja -j2 qemu-system-aarch64 qemu-img` to limit concurrent compiler jobs. |
| `Display 'cocoa'/'gtk' not available` | Mismatched display backend | On macOS use `--enable-cocoa`; on Ubuntu use `--enable-gtk`. |
| Tier 3 skipped in `run_ci_test.py` | Expected behavior on fresh clone | Tier 3 tests the proprietary Linux/firmware disk; Tier 1 and 2 verify the public silicon model. |

---

## License

OpenDTV machine model and patches are licensed under the **GNU General Public License v2 (GPL-2.0-or-later)**, matching QEMU.
