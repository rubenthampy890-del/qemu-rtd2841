# Realtek RTD2841 QEMU Model

[![CI Build & Emulation Test](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml/badge.svg)](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml)

This repository adds a Realtek RTD2841 / RTK284x TV board model to QEMU. The target is the
TCL RT41 chassis, board profile `tv030`: four Cortex-A53 CPU cores running
32-bit ARM (AArch32) software, a vendor Linux 4.4.3 kernel, and Android 9 Pie
("UnionTV"). QEMU runs the emulated hardware on your computer.

## Android TV on the emulated board

I booted TCL’s Android userspace on the emulated RT41 and navigated the
Projectivy Home app grid on a development Mac using a disk with onboarding
already completed.

[![Projectivy Home running on the emulated RT41](docs/media/projectivy-home.jpg)](https://cdn.jsdelivr.net/gh/rubenthampy890-del/qemu-rtd2841@7cc5737/docs/media/rt41-projectivy-home.mp4)

[Watch the 52-second saved-session boot and navigation demo](https://cdn.jsdelivr.net/gh/rubenthampy890-del/qemu-rtd2841@7cc5737/docs/media/rt41-projectivy-home.mp4).
I recorded this on my Mac in two parts. The edit skips portions of the boot
wait and shows navigation at its recorded speed.

## Known issue: the launcher can restart during first use

On the same Mac, Android booted from fresh user data and displayed Projectivy’s
Welcome screen. During onboarding the launcher stopped responding. Android
logged an application timeout and killed the launcher process. The display
returned to the blue splash and then to Welcome while Android kept running.

In one fresh-disk experiment, lowering the resolution and disabling the
launcher’s measurement service allowed onboarding to reach the Home grid.
The launcher later timed out again. I have yet to isolate why the saved
session stayed usable or verify repeatable navigation from a fresh disk on
macOS or Linux. I have no measured first-boot success rate. Public CI covers
hardware and firmware-independent checks. It cannot exercise this private
firmware boot.

## First run on a new computer

Allow about 20 GB of free disk space and 8 GB of RAM. Complete steps 1–3 for the hardware demo. Continue to [Boot Android TV](#boot-android-tv) if you have firmware.

### 1. Install the packages for your OS

**macOS**

Install Apple’s Command Line Tools (`xcode-select --install` if missing) and
[Homebrew](https://brew.sh/). Then run:

```bash
brew install git ninja pkgconf glib pixman libslirp dtc python@3.14 arm-none-eabi-binutils
export PYTHON="$(brew --prefix python@3.14)/bin/python3.14"
```

`pkgconf` supplies the `pkg-config` command. `arm-none-eabi-binutils` supplies
`arm-none-eabi-as`, `arm-none-eabi-ld`, and `arm-none-eabi-objcopy`, which build
the ARM diagnostic programs. The versioned Python path avoids selecting an
older system Python. Package names are listed in the
[Homebrew catalogue](https://formulae.brew.sh/formula/).

**Ubuntu 24.04 LTS**

Use a desktop session for the graphical window. A server without a desktop can
still run the automated tests.

```bash
sudo apt update
sudo apt install -y build-essential git ninja-build pkg-config \
    libglib2.0-dev libpixman-1-dev libslirp-dev libfdt-dev zlib1g-dev \
    libgtk-3-dev device-tree-compiler python3 python3-venv \
    binutils-arm-none-eabi ca-certificates
```

The package that provides `dtc` is **`device-tree-compiler`**. The ARM tools come
from **`binutils-arm-none-eabi`**. These names were checked against the
[Ubuntu 24.04 package catalogue](https://packages.ubuntu.com/noble/).

<details>
<summary>Other Linux distributions: Fedora and Arch package lists</summary>

These are alternative dependency lists, not additional steps for Ubuntu users.
Package names were checked against the distribution catalogues; builds on these
systems have not been executed as part of this guide's verification.

**Fedora 43/44** ([package catalogue](https://packages.fedoraproject.org/)):

```bash
sudo dnf install -y gcc gcc-c++ make git ninja-build pkgconf-pkg-config \
    glib2-devel pixman-devel libslirp-devel libfdt-devel zlib-ng-compat-devel \
    gtk3-devel dtc python3 python3-devel arm-none-eabi-binutils-cs ca-certificates
```

**Arch Linux** ([package catalogue](https://archlinux.org/packages/)):

```bash
sudo pacman -Syu --needed base-devel git ninja pkgconf \
    glib2 pixman libslirp dtc zlib gtk3 python arm-none-eabi-binutils ca-certificates
```

On Arch, `-Syu` also updates installed system packages. Finish that update before
building. Do not assume these lists cover RHEL, CentOS, Manjaro, or every Debian
or Kali release: their repositories and dependency versions can differ. For
other distributions, match the dependencies and use Python 3.12 or newer.

</details>

### 2. Clone the repository

Use a checkout path without spaces or colons. Run on the host:

```bash
mkdir -p "$HOME/qemu-work"
cd "$HOME/qemu-work"
git clone https://github.com/rubenthampy890-del/qemu-rtd2841.git
cd qemu-rtd2841
```

### 3. Build the board and check the display

```bash
./scripts/setup-qemu.sh --demo
```

Eight color bars confirm the display demo is running. Omit `--demo` for a terminal-only test run. Repeating the command reuses the
download and compiled objects. On a fresh clone, the two public hardware tiers
run and the firmware-dependent Linux test is skipped. Close the display window
when you have seen the bars.

## Boot Android TV

### 1. Choose the inputs you have

Start in the repository directory after building the emulator above. You need a
matching patched ARM32 kernel. Choose **one** disk preparation path:

- **Prepared eMMC disk** — go straight to step 2.
- **Emulator-ready ext4 partitions** — use the import command under step 2. The system partition needs a working `/init`.
- **Extracted RT41 firmware** — [build the disk from extracted files](#firmware-and-storage), then return to step 2.
- **TCL OTA only** — partition extraction and preparation are still a development workflow. I haven’t verified a fresh OTA-to-Home procedure.

### 2. Start one session

**Prepared disk.** Replace the two absolute paths with your own. Give the
session a new filename on first boot.

```bash
./start_qemu_android.sh --kernel "/absolute/path/to/zImage" --system "/absolute/path/to/emmc.raw" --overlay "./my-tv.qcow2"
```

If the kernel and disk are already in the default locations
(`output/kernel/zImage` and `output/emmc.raw`), you can run
`./start_qemu_android.sh` directly. Use `./run.sh --check` to inspect discovery.

**Separate partitions.** To import the system and vendor images and boot them, run:

```bash
./start_qemu_android.sh --kernel "/absolute/path/to/zImage" --system "/absolute/path/to/system.img" --vendor "/absolute/path/to/vendor.img"
```

Save the printed base disk and session paths. Guest Linux mounts the partitions.
Keep the host terminal open while QEMU is running.

### 3. Navigate, close, and resume

| In the QEMU window | Action |
|---|---|
| Arrow keys | Move the TV remote focus between buttons or apps |
| Return or Enter | Select the focused item |
| Escape or Backspace | Send the TV remote's Back button |
| Home | Send the TV remote's Home button |
| Click inside, move the mouse, then left-click | Capture the host mouse, move Android's pointer, and select a target |
| Control + Option + G on macOS | Release the mouse back to the Mac |

Mouse input requires the updated board build, the matching vendor kernel with
the [tv030 OHCI patch](patches/linux-4.4.3-tv030-ohci.patch), and input device
permissions in the guest. The launcher enables the controller at boot.
Some TV apps only support remote focus. Use the arrow keys and Return in those
apps. The screen uses software rendering. The emulated TV currently has no
registered ALSA sound card for speaker output.

Close the QEMU window to stop. To resume, use the same kernel, base disk and
session. For imported partitions, use the prepared-disk command with the
printed paths. Keep the base disk at its original location.

## Report a test run

1. Record your host OS, CPU, command, and commit (`git rev-parse --short HEAD`).
2. For the hardware demo, say whether the tests passed and eight bars appeared. For Android, include the firmware and launcher versions, fresh or reused disk, and the last screen you saw.
3. If Home opened, try moving focus with an arrow key. Restart with the same session and check it again. If the splash repeated, describe the sequence and how long it took.
4. [Open a GitHub issue](https://github.com/rubenthampy890-del/qemu-rtd2841/issues/new) with those results. Successful runs help me too. Include a short relevant log excerpt with personal details removed. Keep firmware, APKs and disk images private.

## What you can run

| Goal | Required files | What it establishes |
|---|---|---|
| Hardware tests and color bars | This repository and the pinned upstream QEMU source | CPU execution, timer interrupts, Ethernet packet exchange, eMMC detection, and framebuffer output |
| Linux boot and network test | The above, plus a compatible patched `zImage` and prepared eMMC image | Four-core Linux boot, filesystem mounts, DHCP, ping, DNS, and HTTP checks |
| Android userspace development | The above, plus extracted system/vendor files and the project's userspace setup | Development of Bionic, properties, Binder services, ADB, graphics, and framework startup |

The code includes a custom PID 1 (`mini_init`), software EGL/GLES, Android service
startup, and IR input support. Their presence does not make the public hardware
quickstart a complete Android installation. See [HARDWARE_FIDELITY.md](HARDWARE_FIDELITY.md)
for hardware coverage and [Android and kernel development](#android-and-kernel-development)
for the additional preparation.

Shader builds require the pinned compiler frontend and, for ARM32 guest builds,
the Android NDK. See [shader compiler setup](tools/SHADER_COMPILER.md).
Shader compilation tests do not establish complete shader execution or Android
framework readiness.

For build internals and troubleshooting, see the [manual build reference](BUILD.md).

## Android and kernel development

### Kernel source

TCL publishes the vendor kernel at
[TCLOpenSource/rtk2841](https://github.com/TCLOpenSource/rtk2841), with other releases
listed in [TCL_Kernel_OpenSource](https://github.com/TCLOpenSource/TCL_Kernel_OpenSource).
This kernel source is separate from the proprietary Android system and vendor
binaries.

**1. Get the source.** For source inspection, clone it separately from the public QEMU build:

```bash
cd "$HOME/qemu-work"
git clone https://github.com/TCLOpenSource/rtk2841.git
```

**2. Check the kernel requirements.** The repository includes [the emulator kernel patch](patches/linux-4.4.3-rtd2841-qemu.patch)
and [a kernel build helper](scripts/build_kernel.sh). They are development inputs,
not a verified one-command fresh-host kernel recipe. In particular:

- `build_kernel.sh` does **not** apply that patch. Its default source discovery
  also contains the path from my development machine; a new machine needs explicit
  `--src` and `--output` paths.
- The correct vendor source revision, configuration, cross-compiler, emulator
  patch, and embedded device tree must agree. A stock vendor build is not
  equivalent to the tested emulator kernel.
- The helper's container path needs Docker or Podman and a suitable build image.
  These are not installed by the hardware quickstart. Linux kernel source also
  requires care with case-sensitive filenames on macOS.

**3. Inspect the helper’s options** without starting a build:

```bash
cd "$HOME/qemu-work/qemu-rtd2841"
bash scripts/build_kernel.sh --help
```

### Firmware and storage

No proprietary TV firmware or prepared Android disk image is distributed here.
Use firmware you are authorized to obtain from your device or the manufacturer.
The TCL kernel links above supply **source code**, not an Android firmware image.

**1. Prepare the inputs.** If you already have the extracted RT41 firmware and ARM32 renderer libraries,
you can build an emulator disk in a separate checkout. Use that checkout's
`staging/` and `output/` for this build: the builder writes intermediate images
to `staging/` and replaces the disk at the selected output path. Keep saved
sessions with their original backing disk.

This needs the ARM32 compiler (`arm-none-eabi-gcc`) and ext4 tools
(`mke2fs`, `debugfs`, `e2fsck`) in addition to the QEMU build tools.
The renderer directory must contain `libEGL.so`, `libGLES_CM.so`, and
`libGLESv2.so` for this Android guest. On macOS the builder uses a temporary
case-sensitive disk image for vendor filenames. The system extraction is copied
before removing apps. Allow space for that copy as well as the partition images.

**2. Build the disk.** From that checkout on the host, replace the four example input paths:

```bash
python3 tools/build_debloated_rom.py --skip-update-img \
    --system-dir "/path/to/system_extracted" \
    --vendor-tar "/path/to/vendor.tar" \
    --launcher-apk "/path/to/ProjectivyLauncher.apk" \
    --emulator-swiftshader-dir "/path/to/arm32-swiftshader" \
    --output-dir "./output/new-tv"
```

**3. Boot the result.** Use `output/new-tv/emmc.raw` as the prepared disk in [Boot Android TV](#boot-android-tv).

### Linux verification with already-prepared assets

**1. Set the inputs.** If you already have a compatible emulator kernel and prepared **raw** eMMC image,
set their absolute paths below. Replace the two example paths before running:

```bash
export KERNEL_BIN="/absolute/path/to/zImage"
export STORAGE_IMAGE="/absolute/path/to/emmc.raw"
```

**2. Run the diagnostic boot.**

```bash
(
    set -eu
    test -f "$KERNEL_BIN"
    test -f "$STORAGE_IMAGE"
    cd "$HOME/qemu-work/qemu-rtd2841"
    QEMU_BIN="$PWD/build/qemu-src/build/qemu-system-aarch64" QEMU_IMG="$PWD/build/qemu-src/build/qemu-img" \
        KERNEL_BIN="$KERNEL_BIN" STORAGE_IMAGE="$STORAGE_IMAGE" \
        STRICT_CI=1 "${PYTHON:-python3}" tests/run_ci_test.py
)
```

`STRICT_CI=1` makes missing Tier 3 inputs a failure. The runner creates a temporary
qcow2 overlay: guest disk writes go into that disposable file, preserving the
base image. Tier 3 expects the project's diagnostic `/init` and its network
checks; it is not a test for an arbitrary Android installation. It does not
establish that an Android app launches or displays correctly.

## Repository structure

```text
qemu-rtd2841/
├── README.md                 Setup, tests, display, and current preparation limits
├── HARDWARE_FIDELITY.md      Hardware addresses, implemented behavior, and evidence
├── qemu-model/              Board, timer, IR, and CPU startup source
├── patches/                 QEMU integration patch and Linux kernel patch
├── dts/                     Device tree source and compiled DTB
├── tests/                   ARM diagnostics and host-side test runners
├── tools/                   PID 1, software graphics, storage and development tools
├── scripts/                 Kernel build helper and emulator utilities
└── build/                   Local generated files; ignored by Git
    └── qemu-src/            Downloaded QEMU source
        └── build/           qemu-system-aarch64 and qemu-img
```

The `staging/`, `output/`, and `artifacts/` directories are also ignored by Git
and may exist in a developer's workspace. They are not supplied by a fresh clone.
See [tests/README.md](tests/README.md) for the diagnostic payloads.

## License

See [LICENSE](LICENSE) and individual source-file notices for licensing terms.
QEMU and the vendor Linux kernel retain their upstream licenses. Proprietary
firmware is not included in this repository's source license.
