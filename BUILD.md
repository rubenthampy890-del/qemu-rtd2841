# Manual QEMU build

The [first-run guide](README.md#first-run-on-a-new-computer) automates this procedure. Use this reference when changing the board patch or debugging a build.

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
