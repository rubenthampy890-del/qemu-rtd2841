# Realtek RTD2841 QEMU Model

[![CI Build & Emulation Test](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml/badge.svg)](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml)

This repository adds a Realtek RTD2841 / RTK284x TV board model to QEMU. The target is the
TCL RT41 chassis, board profile `tv030`: four Cortex-A53 CPU cores running
32-bit ARM (AArch32) software, a vendor Linux 4.4.3 kernel, and Android 9 Pie
("UnionTV"). QEMU runs the emulated hardware on your computer.

## Phase 4 milestone — Android TV on the emulated board

We booted TCL’s Android userspace on the emulated RT41 and navigated the
Projectivy Home app grid on a development Mac with a prepared disk. First
boot with empty Android user data remains experimental. Fresh disks have
shown Welcome followed by a return to the blue splash. In one experiment
a fresh disk reached Home and the launcher later stopped responding.
Android recorded launcher timeouts. We have no measured first-boot success rate.

**Report your experience**, including successful runs, in a
[GitHub issue](https://github.com/rubenthampy890-del/qemu-rtd2841/issues/new).
Include your OS and CPU, the output of `git rev-parse --short HEAD`, the
command you ran, and whether you tried the hardware demo or Android.
For Android, include the firmware and launcher versions, whether your disk was
fresh or reused, and the last screen you reached. Could an arrow key move focus
between Home tiles? After restarting with the same session, could you navigate
again? If the screen looped, describe the sequence and approximate time.
Share a short relevant log excerpt with personal details removed. Keep APKs,
firmware and disk images private.

## First run on a new computer

1. Install the [host tools for your operating system](#1-install-the-host-tools).
2. Clone the project into a path without spaces or colons, then enter it.

   ```bash
   mkdir -p "$HOME/qemu-work"
   cd "$HOME/qemu-work"
   git clone https://github.com/rubenthampy890-del/qemu-rtd2841.git
   cd qemu-rtd2841
   ```

3. Build the board, run its hardware tests, and open the color-bar display.

   ```bash
   ./scripts/setup-qemu.sh --demo
   ```

Omit `--demo` for a terminal-only test run. Repeating the command reuses the
download and compiled objects. On a fresh clone, the two public hardware tiers
run and the firmware-dependent Linux test is skipped. Close the display window
when you have seen the bars.

For **Android TV**, bring a matching patched kernel and a prepared Android
eMMC disk. The procedure below starts at that point. If you only have a TCL
OTA, extracting and preparing an Android disk is still a development workflow.
Building the Projectivy edition also requires a locally supplied launcher APK
(`--launcher-apk` on the ROM builder) and the matching ARM32 SwiftShader libraries.

## Start Android on a prepared computer

From the repository directory, run this host command:

```bash
./start_qemu_android.sh
```

The launcher opens a QEMU display window and prints the kernel log in Terminal.
Keep Terminal open while using the TV. Give Android a few minutes to start on
the software renderer. On first use, Projectivy may show its Welcome screens. If
Android asks about access to TV listings, choose your preference.

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

If Projectivy returns to its blue splash after showing Home, keep your session
file. We have observed a launcher timeout with its rendering thread inside
SwiftShader. Resetting the disk erases your session and has no verified benefit
for this symptom.

**Where the files go.** The Android launcher needs the patched QEMU executable,
the tv030 DTB, a compatible ARM32 kernel (normally `output/kernel/zImage`),
and a prepared eMMC disk (normally `output/emmc.raw`). On the prepared
development machine it also finds the saved Projectivy setup. The launcher
writes changes to a separate qcow2 session file and prints its location.
Keep the session, its backing files, and their paths together between runs.
A new clone needs the public build below and privately supplied Android assets.

**Use your own prepared eMMC disk.** From the repository directory on the host:

```bash
./start_qemu_android.sh --kernel "/absolute/path/to/zImage" --system "/absolute/path/to/emmc.raw" --overlay "./my-tv.qcow2"
```

Choose a new filename for the overlay on first use. Repeat the same command to
resume. QEMU attaches the disk as eMMC and guest Linux mounts its partitions.
This requires no host mount or administrator access. To import separate raw ext4
partitions with an emulator-ready root and `/init`, use:

```bash
./start_qemu_android.sh --system "/absolute/path/to/system.img" --vendor "/absolute/path/to/vendor.img"
```

The import creates a 4 GiB GPT disk under a new `output/firmware-*` directory.
Save the printed base disk path and use it with the preceding eMMC command on
later boots. A TCL `Update.img` needs extraction and preparation first.
Use `./run.sh --check` to see which files and tools the launcher found.
Close the display window to stop QEMU.

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

## Before you start

Use macOS or Linux with an internet connection. For the hardware build, allow
about 20 GB of free space and 8 GB of RAM. Use a checkout path without spaces
or colons, such as `~/qemu-work/qemu-rtd2841`. Run the numbered steps in the
same host terminal so the build paths carry over.

## 1. Install the host tools

### macOS

Install Apple’s Command Line Tools (`xcode-select --install` if missing) and
[Homebrew](https://brew.sh/). Then run:

```bash
brew install git ninja pkgconf glib pixman libslirp dtc python@3.14 arm-none-eabi-binutils
export PYTHON="$(brew --prefix python@3.14)/bin/python3.14"
export QEMU_DISPLAY=cocoa
```

`pkgconf` supplies the `pkg-config` command. `arm-none-eabi-binutils` supplies
`arm-none-eabi-as`, `arm-none-eabi-ld`, and `arm-none-eabi-objcopy`, which build
our ARM diagnostic programs. The versioned Python path avoids selecting an
older system Python. Package names are listed in the
[Homebrew catalogue](https://formulae.brew.sh/formula/).

### Ubuntu 24.04 LTS

Use a desktop session for the graphical window. A server without a desktop can
still run the automated tests.

```bash
sudo apt update
sudo apt install -y build-essential git ninja-build pkg-config \
    libglib2.0-dev libpixman-1-dev libslirp-dev libfdt-dev zlib1g-dev \
    libgtk-3-dev device-tree-compiler python3 python3-venv \
    binutils-arm-none-eabi ca-certificates
export PYTHON=python3
export QEMU_DISPLAY=gtk
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
export PYTHON=python3
export QEMU_DISPLAY=gtk
```

**Arch Linux** ([package catalogue](https://archlinux.org/packages/)):

```bash
sudo pacman -Syu --needed base-devel git ninja pkgconf \
    glib2 pixman libslirp dtc zlib gtk3 python arm-none-eabi-binutils ca-certificates
export PYTHON=python3
export QEMU_DISPLAY=gtk
```

On Arch, `-Syu` also updates installed system packages. Finish that update before
building. Do not assume these lists cover RHEL, CentOS, Manjaro, or every Debian
or Kali release: their repositories and dependency versions can differ. For
other distributions, match the dependencies and check Python below.

</details>

### Check the tools before downloading QEMU

Run on either host:

```bash
"$PYTHON" -c 'import sys; print(sys.version); assert sys.version_info >= (3, 12), "This QEMU revision needs Python 3.12 or newer"'
```

```bash
for tool in git make ninja pkg-config dtc arm-none-eabi-as arm-none-eabi-ld arm-none-eabi-objcopy; do
    command -v "$tool" || break
done
```

The loop must print a path for **all eight tools**. If it stops early, return to
the package installation step. The pinned QEMU revision needs Python 3.12 or
newer; Ubuntu 22.04's default Python is too old for this guide.

## 2. Download the QEMU model

This creates a new checkout under your home directory:

```bash
mkdir -p "$HOME/qemu-work"
cd "$HOME/qemu-work"
git clone https://github.com/rubenthampy890-del/qemu-rtd2841.git
```

```bash
cd "$HOME/qemu-work/qemu-rtd2841"
export REPO_ROOT="$PWD"
export QEMU_SRC="$REPO_ROOT/build/qemu-src"
```

If Git says the destination already exists, do not delete it. For an existing
installation, follow [Returning to an existing build](#returning-to-an-existing-build).

## 3. Build QEMU with the RTD2841 board

A normal QEMU installation does not include this board. This step downloads the
upstream source version used by the project, applies the hardware patch, then
builds a local executable. It does not replace your system QEMU.

Fetch the exact upstream revision into a new directory:

```bash
(
    set -eu
    test ! -e "$QEMU_SRC"
    mkdir -p "$QEMU_SRC"
    git -C "$QEMU_SRC" init
    git -C "$QEMU_SRC" remote add origin https://gitlab.com/qemu-project/qemu.git
    git -C "$QEMU_SRC" fetch --depth 1 origin a19f59a72d96056d9a8f48cf6c77f5787491e414
    git -C "$QEMU_SRC" checkout --detach FETCH_HEAD
)
```

The parentheses keep this block's settings local; `set -eu` stops it if a command
fails or a required variable is missing. An existing `$QEMU_SRC` deliberately
stops the block. A Git message about a **detached HEAD** is expected: it means
this source is pinned to a specific revision.

Check and apply the patch once:

```bash
(
    set -eu
    git -C "$QEMU_SRC" apply --check "$REPO_ROOT/patches/qemu-rtd2841-complete.patch"
    git -C "$QEMU_SRC" apply "$REPO_ROOT/patches/qemu-rtd2841-complete.patch"
)
```

No output means the patch commands succeeded. Do not apply the patch a second
time to the same source tree.

Configure and compile. `$QEMU_DISPLAY` selects Cocoa on macOS or GTK on Linux:

```bash
(
    set -eu
    mkdir -p "$QEMU_SRC/build"
    cd "$QEMU_SRC/build"
    ../configure --target-list=aarch64-softmmu \
        --enable-slirp --enable-pixman --enable-tools \
        "--enable-$QEMU_DISPLAY" --disable-rust --disable-werror \
        --disable-docs --python="$PYTHON"
    ninja -j4 qemu-system-aarch64 qemu-img
)
```

QEMU sets up its own Python build environment and may download build
requirements. `-j4` allows four compilation jobs; use `-j2` if memory is tight.
Successful compilation ends without `FAILED` or `ninja: build stopped`.

Save the executable paths and check the result:

```bash
export QEMU_BIN="$QEMU_SRC/build/qemu-system-aarch64"
export QEMU_IMG="$QEMU_SRC/build/qemu-img"
"$QEMU_BIN" --version
"$QEMU_IMG" --version
"$QEMU_BIN" -machine help
"$QEMU_BIN" -display help
```

The machine list must include **`rtd2841-tv030`**; the display list must include
**`cocoa`** on macOS or **`gtk`** on Linux. The executable is named
`qemu-system-aarch64`, but this board runs its guest in **32-bit ARM mode**.

## 4. Build and run the hardware tests

Build the diagnostic programs from their assembly sources, then compile the
device tree. The device tree describes the board's memory and peripherals to
the software running inside it.

```bash
(
    set -eu
    cd "$REPO_ROOT"
    make -C tests clean
    make -C tests
    dtc -I dts -O dtb -o dts/rtd284x-tv030.dtb dts/rtd284x-tv030.dts
)
```

Run the hardware test runner:

```bash
cd "$REPO_ROOT"
QEMU_BIN="$QEMU_BIN" "$PYTHON" tests/run_ci_test.py
```

On a fresh checkout, expect:

- **Tier 1 passes:** CPU execution, timer counting and interrupts, and Ethernet
  packet exchange with QEMU's virtual gateway.
- **Tier 2 passes:** the emulated eMMC controller detects an attached test disk.
- **Tier 3 is skipped:** no compatible Linux kernel and firmware disk are present.
- The runner exits with status `0` and prints
  `[>>> ALL CI HARDWARE AUTOMATION TESTS PASSED <<<]`.

To inspect the exit status, run `echo $?` immediately after the runner, before
another command. A final pass with Tier 3 skipped verifies the executed hardware
tests; it does not establish Linux or Android boot. The runner is headless, so
no display window should open during this step.

## 5. Open the display window

Run this command in the same host terminal:

```bash
"$QEMU_BIN" -M rtd2841-tv030 -m 1024M -smp 4 \
    -dtb "$REPO_ROOT/dts/rtd284x-tv030.dtb" \
    -kernel "$REPO_ROOT/tests/firmware_fb_test.bin" \
    -nic user,model=rtd2841-eth \
    -monitor none -serial mon:stdio -display "$QEMU_DISPLAY"
```

You should see **eight vertical color bars at 1920 × 1080**. This small ARM
program writes directly to the emulated framebuffer. The bars are the expected
result of this test; there is no Android launcher in this payload.

**To quit:** click the terminal that started QEMU, hold **Ctrl**, press **A**,
release both, then press lowercase **x**. Use Control, not Command on a Mac.
Alternatively, **Ctrl+A**, then **c** switches to the QEMU monitor, where you can
type `quit` and press Enter. These keys belong in the terminal, not the graphics
window. The display test stays running until you quit it.

On a Linux server without a desktop, replace the final argument with
`-display none` to run without a window.

## Returning to an existing build

From a new host terminal:

```bash
cd "$HOME/qemu-work/qemu-rtd2841"
export REPO_ROOT="$PWD"
export QEMU_SRC="$REPO_ROOT/build/qemu-src"
export QEMU_BIN="$QEMU_SRC/build/qemu-system-aarch64"
export QEMU_IMG="$QEMU_SRC/build/qemu-img"
```

On **macOS**:

```bash
export PYTHON="$(brew --prefix python@3.14)/bin/python3.14"
export QEMU_DISPLAY=cocoa
```

On **Linux**:

```bash
export PYTHON=python3
export QEMU_DISPLAY=gtk
```

You can now rerun steps 4 and 5. To rebuild changes made inside the patched QEMU
source tree:

```bash
ninja -C "$QEMU_SRC/build" -j4 qemu-system-aarch64 qemu-img
```

Editing `qemu-model/` does **not** automatically update the separate
QEMU source tree. The first build uses `patches/qemu-rtd2841-complete.patch`.
If that patch changes after an update, use a new source directory, such as
`export QEMU_SRC="$REPO_ROOT/build/qemu-src-next"`, and repeat step 3. Keep
any local source changes before replacing a build.

## Android and kernel development

### Kernel source

TCL publishes the vendor kernel at
[TCLOpenSource/rtk2841](https://github.com/TCLOpenSource/rtk2841), with other releases
listed in [TCL_Kernel_OpenSource](https://github.com/TCLOpenSource/TCL_Kernel_OpenSource).
This kernel source is separate from the proprietary Android system and vendor
binaries.

For source inspection, clone it separately from the public QEMU build:

```bash
cd "$HOME/qemu-work"
git clone https://github.com/TCLOpenSource/rtk2841.git
```

The repository includes [the emulator kernel patch](patches/linux-4.4.3-rtd2841-qemu.patch)
and [a kernel build helper](scripts/build_kernel.sh). They are development inputs,
not a verified one-command fresh-host kernel recipe. In particular:

- `build_kernel.sh` does **not** apply that patch. Its default source discovery
  also contains the original author's local path; a new machine needs explicit
  `--src` and `--output` paths.
- The correct vendor source revision, configuration, cross-compiler, emulator
  patch, and embedded device tree must agree. A stock vendor build is not
  equivalent to the tested emulator kernel.
- The helper's container path needs Docker or Podman and a suitable build image.
  These are not installed by the hardware quickstart. Linux kernel source also
  requires care with case-sensitive filenames on macOS.

Inspect the helper's options without starting a build:

```bash
cd "$REPO_ROOT"
bash scripts/build_kernel.sh --help
```

### Firmware and storage

No proprietary TV firmware or prepared Android disk image is distributed here.
Use firmware you are authorized to obtain from your device or the manufacturer.
The TCL kernel links above supply **source code**, not an Android firmware image.

If you already have the extracted RT41 firmware and ARM32 renderer libraries,
you can build an emulator disk in a separate checkout. Use that checkout's
`staging/` and `output/` for this build: the builder writes intermediate images
to `staging/` and replaces the disk at the selected output path. Keep saved
sessions with their original backing disk.

From that checkout on the host, replace the four example input paths:

```bash
python3 tools/build_debloated_rom.py --skip-update-img \
    --system-dir "/path/to/system_extracted" \
    --vendor-tar "/path/to/vendor.tar" \
    --launcher-apk "/path/to/ProjectivyLauncher.apk" \
    --emulator-swiftshader-dir "/path/to/arm32-swiftshader" \
    --output-dir "./output/new-tv"
```

This needs the ARM32 compiler (`arm-none-eabi-gcc`) and ext4 tools
(`mke2fs`, `debugfs`, `e2fsck`) in addition to the QEMU build tools.
The renderer directory must contain `libEGL.so`, `libGLES_CM.so`, and
`libGLESv2.so` for this Android guest. On macOS the builder uses a temporary
case-sensitive disk image for vendor filenames. The system extraction is copied
before removing apps. Allow space for that copy as well as the partition images.

Then use the matching kernel to start the disk:

```bash
./start_qemu_android.sh --kernel "/path/to/zImage" \
    --system "./output/new-tv/emmc.raw" --overlay "./output/new-tv/session.qcow2"
```

The private ROM integration tests built the disk twice on macOS using extracted
firmware and existing renderer libraries. A freshly generated disk displayed
Projectivy’s Welcome page, then returned to the blue splash during onboarding.
Android recorded repeated launcher timeouts. Completing setup and reaching a
stable Home screen on a newly generated disk remains unverified. A fresh OTA-to-Home setup, including building
the renderer from source, remains unverified.

The existing [GUI helper](tools/run_emulator_gui.sh) is also a development helper:
it uses Cocoa, has local build-path defaults, and does not accept `-m android`.
Use the explicit command in step 5 for the public display demonstration.

### Linux verification with already-prepared assets

If you already have a compatible emulator kernel and prepared **raw** eMMC image,
set their absolute paths below. Replace the two example paths before running:

```bash
export KERNEL_BIN="/absolute/path/to/zImage"
export STORAGE_IMAGE="/absolute/path/to/emmc.raw"
```

```bash
(
    set -eu
    test -f "$KERNEL_BIN"
    test -f "$STORAGE_IMAGE"
    cd "$REPO_ROOT"
    QEMU_BIN="$QEMU_BIN" QEMU_IMG="$QEMU_IMG" \
        KERNEL_BIN="$KERNEL_BIN" STORAGE_IMAGE="$STORAGE_IMAGE" \
        STRICT_CI=1 "$PYTHON" tests/run_ci_test.py
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

## Troubleshooting

| Symptom | What to check |
|---|---|
| `brew: command not found` | Follow Homebrew's installer **Next steps**. Its usual prefix is `/opt/homebrew` on Apple Silicon or `/usr/local` on Intel; do not hard-code the other machine's prefix. |
| `Unable to locate package` on Ubuntu | Run `sudo apt update` and confirm you are using Ubuntu 24.04. For packages in Universe, use the commands below. |
| Python version error | Run `"$PYTHON" --version`. This pinned QEMU needs Python 3.12 or newer. Reapply the operating-system-specific `PYTHON` export. |
| `main directory cannot contain spaces nor colons` | Use a source/build location without spaces or colons. Quotes alone cannot fix this QEMU restriction. |
| `patch does not apply` | Check `git -C "$QEMU_SRC" rev-parse HEAD`. It must be the pinned revision, and the patch must not already be applied. Preserve edits and use a fresh source directory if needed. |
| `ninja: build stopped: subcommand failed` | Read the first compiler error above it. Memory exhaustion is one possible cause, not the only one; reduce to `-j2` if the compiler was killed for lack of memory. |
| `arm-none-eabi-as: command not found` | Install the ARM binutils package listed for your host, then rerun the tool check. The native `as` is not a substitute. |
| `unsupported machine type` | Use the newly built `$QEMU_BIN`, not a stock `qemu-system-aarch64` from your system. |
| Display backend unavailable | Check `"$QEMU_BIN" -display help`. Reconfigure with Cocoa on macOS or GTK on Linux and rebuild. |
| GTK cannot open the display | Run from a Linux desktop terminal, or use `-display none` for headless testing. |
| Tier 3 skipped | Expected on a fresh checkout. The kernel and prepared firmware disk are separate inputs. |
| `command not found` after reopening Terminal | Restore the variables under **Returning to an existing build**. |
| Terminal shows `>` and waits | A quote or multiline command is incomplete. Press Ctrl+C, then paste the complete block without spaces after `\`. |

If Ubuntu's Universe repository is disabled:

```bash
sudo apt install -y software-properties-common
sudo add-apt-repository universe
sudo apt update
```

Then rerun the Ubuntu prerequisite installation. This repository command is for
Ubuntu, not Debian or other distributions.

## License

See [LICENSE](LICENSE) and individual source-file notices for licensing terms.
QEMU and the vendor Linux kernel retain their upstream licenses. Proprietary
firmware is not included in this repository's source license.
