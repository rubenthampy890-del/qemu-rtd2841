# OpenDTV — RTD2841 TV emulator

[![CI Build & Emulation Test](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml/badge.svg)](https://github.com/rubenthampy890-del/qemu-rtd2841/actions/workflows/ci.yml)

OpenDTV models the Realtek RTD2841 / RTK284x hardware used by the TCL RT41 TV chassis, board profile `tv030`, inside QEMU. The target has four Cortex-A53 CPUs running **32-bit ARM code (AArch32)**. The retail software stack is Linux 4.4.3 and Android 9 Pie, also called UnionTV.

**Start with the hardware diagnostics below.** You can build and run them using public source code, without a TV or a firmware download. Linux and Android experiments additionally need separately prepared firmware assets.

**Android bring-up is still in progress.** ADB and several native services run locally, but the latest independent check did not reach a working Android application screen. A color pattern or a running SurfaceFlinger process does not mean Android has finished booting.

## Choose your goal

| Goal | What you need | Where to start |
| --- | --- | --- |
| Build the emulator and test the hardware model | This repository, build tools and internet access | Steps 1–6 |
| Open the framebuffer color bars | The build from steps 1–6 and a desktop session | Step 7 |
| Test a compatible Linux kernel and prepared eMMC disk | The build plus your own compatible boot assets | [Linux firmware tests](#linux-firmware-tests) |
| Work on Android startup and graphics | Vendor system/vendor files, compatible kernel, prepared disk and development tools | [Android and kernel development](#android-and-kernel-development) |

## Before typing commands

- **Host** means your Mac or Linux computer. **Guest** means the emulated TV. All numbered setup steps run in a **host terminal**.
- On macOS, open **Terminal**. On Ubuntu Desktop, open **Terminal**, usually with **Ctrl+Alt+T**.
- Use the **same terminal window** throughout setup. Variables such as `OPENDTV_ROOT` exist in that terminal session.
- Copy commands without a leading `$` or other prompt. None of the command blocks below include prompt characters.
- A backslash `\` at the end of a line continues the command on the next line. It must be the **last character**: no spaces or comments after it. Paste the whole block. If the terminal waits at a `>` prompt after an incomplete paste, press **Ctrl+C** and paste again.
- Keep the source and build directories free of spaces and colons. This pinned QEMU version rejects such paths even when quoted. The example uses `~/opendtv-work/qemu-rtd2841`.
- Stop if a command reports an error. Do not continue to the next step until the failed step succeeds. [Troubleshooting](#troubleshooting) covers common failures.
- Reserve several gigabytes for QEMU source/build output, plus additional space for any firmware images. Each emulated guest here is configured with 1 GiB RAM. These are working allocations, not measured minimum host requirements.

The detailed Linux instructions target **Ubuntu 24.04 LTS**. Other distributions need equivalent packages and Python 3.12 or newer; their package commands are not interchangeable. The Mac build was checked on Apple Silicon. Intel Mac execution has not been independently checked in this documentation pass.

## 1. Install the host tools

Choose **one** subsection for your operating system.

### macOS

First check for Apple's developer tools:

```bash
xcode-select -p
```

If this reports an active developer directory, continue. Otherwise run:

```bash
xcode-select --install
```

Finish the installer before continuing. Install [Homebrew using its official instructions](https://docs.brew.sh/Installation), including the installer's **Next steps** for adding `brew` to your shell. Then check:

```bash
brew --version
```

Install the packages; do not prefix `brew install` with `sudo`:

```bash
brew install git ninja pkgconf glib pixman libslirp dtc python@3.14 arm-none-eabi-binutils
```

Use Bash for the remaining commands, and select the installed Python explicitly:

```bash
bash
export PYTHON="$(brew --prefix python@3.14)/bin/python3.14"
export OPENDTV_DISPLAY=cocoa
```

Package names were checked against Homebrew's catalogue. [`pkgconf`](https://formulae.brew.sh/formula/pkgconf) provides the `pkg-config` command; [`arm-none-eabi-binutils`](https://formulae.brew.sh/formula/arm-none-eabi-binutils) provides the ARM assembler, linker and binary converter. `dtc` compiles device trees. The public assembly tests do **not** need an ARM C compiler.

### Ubuntu 24.04 LTS

Check your release:

```bash
cat /etc/os-release
```

Install the build dependencies:

```bash
sudo apt update
sudo apt install -y build-essential git ninja-build pkg-config \
    libglib2.0-dev libpixman-1-dev libslirp-dev libfdt-dev zlib1g-dev \
    libgtk-3-dev device-tree-compiler python3 python3-venv \
    binutils-arm-none-eabi ca-certificates
```

Some packages are in Ubuntu's **Universe** repository. If `apt` cannot locate them, enable Universe and retry the installation:

```bash
sudo apt install -y software-properties-common
sudo add-apt-repository universe
sudo apt update
```

Use Bash and select the host Python and graphical backend:

```bash
bash
export PYTHON=python3
export OPENDTV_DISPLAY=gtk
```

The names above were checked against the Ubuntu 24.04 package catalogue, including [`binutils-arm-none-eabi`](https://packages.ubuntu.com/noble/binutils-arm-none-eabi), [`python3-venv`](https://packages.ubuntu.com/noble/python3-venv) and [`libgtk-3-dev`](https://packages.ubuntu.com/noble/libgtk-3-dev). GTK is for the Linux display window; terminal-only tests do not need a desktop session.

### Check prerequisites on either host

```bash
"$PYTHON" -c 'import sys; print(sys.version); assert sys.version_info >= (3, 12), "QEMU requires Python 3.12 or newer"'
for tool in git make cc ninja pkg-config dtc arm-none-eabi-as arm-none-eabi-ld arm-none-eabi-objcopy; do
    command -v "$tool" || break
done
pkg-config --modversion glib-2.0 pixman-1 slirp
```

The loop must print a path for **every** listed tool. The final command must print three library version numbers. Fix any missing tool/library before building. Python 3.12 is required by the pinned QEMU `configure` script; an older Python bundled with the operating system may be insufficient.

## 2. Download OpenDTV

For a new checkout:

```bash
mkdir -p "$HOME/opendtv-work"
cd "$HOME/opendtv-work"
git clone https://github.com/rubenthampy890-del/qemu-rtd2841.git
cd qemu-rtd2841
export OPENDTV_ROOT="$PWD"
printf 'Project directory: %s\n' "$OPENDTV_ROOT"
git rev-parse --short HEAD
```

Record the printed commit when reporting a problem. If `qemu-rtd2841` already exists, use that checkout deliberately; do not delete it to make the clone command succeed. Check local changes with `git status` first.

QEMU and the diagnostic payloads are separate builds:

```text
Your computer runs qemu-system-aarch64
    └── its rtd2841-tv030 machine runs an ARM32 diagnostic or Linux kernel
```

The name `qemu-system-aarch64` describes the emulator executable. It does not turn this TV firmware into 64-bit Android. A normal distribution/Homebrew QEMU package does not include this project's machine model.

## 3. Download the exact QEMU revision

From the OpenDTV directory:

```bash
export QEMU_SRC="$OPENDTV_ROOT/build/qemu-src"
mkdir -p "$QEMU_SRC"
git -C "$QEMU_SRC" init
git -C "$QEMU_SRC" remote add origin https://gitlab.com/qemu-project/qemu.git
git -C "$QEMU_SRC" fetch --depth 1 origin a19f59a72d96056d9a8f48cf6c77f5787491e414
git -C "$QEMU_SRC" checkout --detach FETCH_HEAD
git -C "$QEMU_SRC" rev-parse HEAD
```

The last command must print `a19f59a72d96056d9a8f48cf6c77f5787491e414`. A detached-HEAD notice is expected: this intentionally selects one source revision. Do not substitute the latest QEMU release; the patch is tied to this revision.

These are first-time commands. If Git says the remote already exists, inspect the existing source directory instead of repeatedly applying setup commands.

## 4. Apply the model and build QEMU

Check the patch before applying it:

```bash
git -C "$QEMU_SRC" apply --check "$OPENDTV_ROOT/patches/qemu-rtd2841-complete.patch"
git -C "$QEMU_SRC" apply "$OPENDTV_ROOT/patches/qemu-rtd2841-complete.patch"
```

Successful patch commands normally print nothing. An error is not success. Apply the patch only once to a clean pinned QEMU checkout.

**macOS configuration:**

```bash
mkdir -p "$QEMU_SRC/build"
cd "$QEMU_SRC/build"
../configure --target-list=aarch64-softmmu --enable-slirp --enable-pixman \
    --enable-tools --enable-cocoa --disable-rust --disable-werror \
    --disable-docs --python="$PYTHON"
```

**Ubuntu configuration:**

```bash
mkdir -p "$QEMU_SRC/build"
cd "$QEMU_SRC/build"
../configure --target-list=aarch64-softmmu --enable-slirp --enable-pixman \
    --enable-tools --enable-gtk --disable-rust --disable-werror \
    --disable-docs --python="$PYTHON"
```

`--enable-slirp` supplies user-mode networking. Cocoa and GTK are the Mac and Linux window backends. `--enable-tools` includes the `qemu-img` disk utility. Rust devices are not needed by this machine. Configuration may download QEMU subprojects and Python build dependencies into the build environment; allow it to finish. Do not use `sudo pip` to solve a configuration failure.

**Then, on either host:**

```bash
ninja -j4 qemu-system-aarch64 qemu-img
cd "$OPENDTV_ROOT"
export QEMU_BIN="$QEMU_SRC/build/qemu-system-aarch64"
export QEMU_IMG="$QEMU_SRC/build/qemu-img"
"$QEMU_BIN" --version
"$QEMU_BIN" -machine help | grep -F rtd2841-tv030
"$QEMU_BIN" -display help
"$QEMU_IMG" --version
```

`-j4` limits the build to four concurrent jobs. Use `-j2` if the compiler runs out of memory. Wait for Ninja to finish without `FAILED` or `ninja: build stopped` messages.

The machine list must include `rtd2841-tv030`. The display list must include `cocoa` on macOS or `gtk` on Ubuntu for step 7. Neither a system QEMU binary nor a successful download alone passes these checks.

## 5. Build the ARM diagnostics and device tree

```bash
cd "$OPENDTV_ROOT"
make -C tests clean
make -C tests
dtc -I dts -O dtb -o dts/rtd284x-tv030.dtb dts/rtd284x-tv030.dts
```

The Makefile assembles **and links** the ARM code at the required address before creating `.bin` files. It also checks the SMP startup instructions. Rebuild these files even if binaries already exist in the checkout.

The device-tree source (`.dts`) describes the board's memory and devices. `dtc` converts it into the binary `.dtb` passed to the emulator. Compiler warnings are printed separately from fatal errors; retain the complete output when reporting a failure.

## 6. Run the automated hardware tests

```bash
cd "$OPENDTV_ROOT"
QEMU_BIN="$QEMU_BIN" "$PYTHON" tests/run_ci_test.py
```

On a fresh checkout without firmware, expect:

| Test | Expected result | What it checks |
| --- | --- | --- |
| Tier 1 | Pass | CPU execution, timer up-count, IRQ delivery, SDHCI registers, Ethernet TX/RX |
| Tier 2 | Pass | Card detection using a temporary synthetic disk |
| Tier 3 | Explicit skip | Requires a compatible local Linux kernel and firmware disk |

The final `ALL CI HARDWARE AUTOMATION TESTS PASSED` banner covers the tiers that actually ran. A Tier 3 skip is normal here; it is **not** an Android boot result. The runner is headless, so no display window is expected.

If the command fails, retain its output and check the troubleshooting table. Do not proceed on the assumption that a pass from an earlier checkout still applies.

## 7. Open the color-bar display

Run from the same host terminal. `OPENDTV_DISPLAY` was set in step 1:

```bash
"$QEMU_BIN" -M rtd2841-tv030 -m 1024M -smp 4 \
    -dtb "$OPENDTV_ROOT/dts/rtd284x-tv030.dtb" \
    -kernel "$OPENDTV_ROOT/tests/firmware_fb_test.bin" \
    -nic user,model=rtd2841-eth \
    -monitor none -serial mon:stdio -display "$OPENDTV_DISPLAY"
```

You should see eight vertical color bars in a 1920×1080 framebuffer. This diagnostic uses no Android firmware and no disk. It remains running after drawing; that is expected.

**Quit from the host terminal:** hold **Control**, press **A**, release both keys, then press **X**. This is QEMU's serial escape sequence, not Command+X on a Mac. To open the QEMU monitor instead, use **Control+A**, then **C**; type `quit` and press Enter. The monitor prompt is `(qemu)` and is not a guest Linux shell.

For a Linux server without a desktop, keep using step 6. GTK requires a working local graphical session; a successful SSH login alone does not provide one. The diagnostic screenshot below is an example, not proof of the state of your current run.

![Bare-metal color-bar diagnostic](tests/smpte_color_bars.png)

## 8. Return later or rebuild after changes

A new terminal does not remember the variables from setup. Recreate them:

```bash
bash
export OPENDTV_ROOT="$HOME/opendtv-work/qemu-rtd2841"
export QEMU_SRC="$OPENDTV_ROOT/build/qemu-src"
export QEMU_BIN="$QEMU_SRC/build/qemu-system-aarch64"
export QEMU_IMG="$QEMU_SRC/build/qemu-img"
cd "$OPENDTV_ROOT"
```

On macOS:

```bash
export PYTHON="$(brew --prefix python@3.14)/bin/python3.14"
export OPENDTV_DISPLAY=cocoa
```

On Ubuntu:

```bash
export PYTHON=python3
export OPENDTV_DISPLAY=gtk
```

You can now repeat steps 6–7 without downloading or rebuilding. If you edited diagnostic source, repeat step 5. If you edited the **applied QEMU source**, rebuild with:

```bash
ninja -C "$QEMU_SRC/build" -j4 qemu-system-aarch64 qemu-img
```

Editing `qemu-model/` in OpenDTV does not automatically update the applied files in `build/qemu-src/`. Likewise, pulling a new OpenDTV patch does not update an existing patched QEMU tree. For a new patch version, preserve the old build and repeat steps 3–4 with a new `QEMU_SRC` directory, such as `build/qemu-src-next`.

## Linux firmware tests

This section is for people who **already have compatible prepared assets**. Neither an OTA `Update.img` nor the kernel source directory is a bootable `zImage` or composite disk.

| Input | Meaning |
| --- | --- |
| `zImage` | Built ARM32 RTD2841 Linux kernel compatible with the model and DTB |
| `emmc.raw` | Composite disk with the expected root filesystem and vendor partitions; the test's `/init` must exist |
| `qemu-img` | Host disk utility built in step 4 |

Set the first two paths to files you actually have. These example paths are placeholders:

```bash
export KERNEL_BIN="/absolute/path/to/zImage"
export STORAGE_IMAGE="/absolute/path/to/emmc.raw"
test -s "$KERNEL_BIN" && test -s "$STORAGE_IMAGE"
"$QEMU_IMG" info -f raw "$STORAGE_IMAGE"
```

If either file check fails, stop. Then run:

```bash
QEMU_BIN="$QEMU_BIN" QEMU_IMG="$QEMU_IMG" \
    KERNEL_BIN="$KERNEL_BIN" STORAGE_IMAGE="$STORAGE_IMAGE" \
    "$PYTHON" tests/run_ci_test.py
```

The runner uses a temporary qcow2 overlay for the Linux test: guest writes go to the overlay, preserving the backing raw file. Passing Tier 3 establishes the runner's Linux/storage/network checks, not a complete Android UI. The root filesystem must provide the expected diagnostic init behavior; an arbitrary ARM Linux image will not satisfy that contract.

## Android and kernel development

### Where the assets come from

TCL publishes [RTD2841 kernel source](https://github.com/TCLOpenSource/rtk2841). Its [kernel source index](https://github.com/TCLOpenSource/TCL_Kernel_OpenSource) identifies the 40S6500 family with Android P and Linux 4.4.3. That source is separate from retail firmware and is not a prebuilt kernel.

For firmware availability, use TCL's support channel for the **exact model, RT41 chassis and region**. This project does not provide a verified universal firmware download. It also does not redistribute the retail system/vendor images or a prepared Android disk. The kernel source license does not establish redistribution permission for every component in a retail firmware package.

### What is needed to reproduce the Android research image

The local research setup consists of these separate pieces:

1. A compatible vendor kernel build with the relevant patches, configuration and DTB.
2. Extracted ARM32 Android `/system` and `/vendor` content from the matching firmware.
3. A composite eMMC image with root, vendor and persistent `/data` partitions.
4. Source-built `mini_init`, `su`, the experimental graphics library, device setup and the required Android service environment.
5. The developer's own ADB public key installed in the guest; the private key stays on the host.

**There is not yet a verified beginner recipe that creates a complete working Android TV from these inputs on a fresh machine.** Current tools expose parts of the process, but publishing them as a finished installation sequence would reproduce known startup/rendering failures:

| File | Current purpose and boundary |
| --- | --- |
| `scripts/build_kernel.sh` | Vendor-kernel build helper. Requires a suitable source tree, cross compiler and configuration. It has historical path/container assumptions; it is not the public QEMU build command. |
| `patches/linux-4.4.3-rtd2841-qemu.patch` | Vendor-kernel integration patch. Successful parsing alone does not prove a fresh kernel build. |
| `tools/create_emmc_storage.py` | Builds images from an already-extracted firmware layout. Its vendor staging uses macOS-specific utilities; it is not a verified Linux provisioning workflow. It writes image files and should not be run casually over existing staging assets. |
| `tools/mini_init.c` | Experimental ARM32 PID 1 for mounting filesystems, properties and starting services. It does not implement all Android init behavior. |
| `tools/software_egl_gles2.c` | Experimental graphics implementation. Shader/native-window behavior remains incomplete. |
| `tests/run_milestone6_test.py` | Local provisioning/verification runner with expected asset paths. Its saved pass result does not establish a working framework/app screen. |
| `tools/run_emulator_gui.sh` | Developer Mac launcher with Cocoa and expected local assets. It is not a portable installer or a Linux launcher. |

As of the November 11 verification, the console-descriptor Zygote failure was fixed, but framework startup subsequently aborted on `/framework/core-oj.jar`. Property waiter notification and shader output also failed independent tests, and `logd` exited. These are runtime tasks to finish before claiming an Android release. The hardware-only build above remains useful independently.

For the current hardware contracts and limitations, read [HARDWARE_FIDELITY.md](HARDWARE_FIDELITY.md). Do not use diagnostic dashboards or test-written pixels as evidence that Android rendered an application.

## Repository structure

```text
qemu-rtd2841/
├── README.md                 Start here: build, run, expected results
├── BUILD.md                  Short pointer to this build sequence
├── HARDWARE_FIDELITY.md       Modeled hardware and verification boundaries
├── qemu-model/               RTD2841 board, timer and IR model source
├── patches/                  Patches applied to upstream QEMU/vendor Linux
├── dts/                      Board device-tree sources and compiled DTBs
├── tests/                    ARM diagnostics and host verification runners
├── tools/                    Research init, graphics, storage and display tools
├── scripts/                  Developer launch/build/input helpers
└── build/                    Local generated files; ignored by Git
    └── qemu-src/             Pinned, patched upstream QEMU checkout
        └── build/            qemu-system-aarch64 and qemu-img
```

`staging/`, `output/` and local research records are also ignored. Their presence on the author's computer does not mean a new clone contains them.

The main implemented paths are the AArch32 CPUs, GIC interrupts, UART, timers, SDHCI/eMMC, Ethernet, simple framebuffer and IR input. Some other peripherals are partial or unimplemented. Consult the fidelity matrix rather than assuming the whole SoC is modeled.

The project roadmap remains: **1. firmware extraction → 2. core silicon/Linux → 3. Android userspace → 4. multimedia/audio → 5. security/customization → 6. upstream QEMU submission.** Phase 3 is active; later phases are not requirements for the public diagnostic demo.

## Troubleshooting

| What you see | What to check or do |
| --- | --- |
| `brew: command not found` | Complete Homebrew's printed shell setup and reopen the terminal; then restore the variables from step 8. |
| `Unable to locate package ...` on Ubuntu | Confirm Ubuntu 24.04, run `sudo apt update`, and enable Universe as described in step 1. Do not substitute guessed package names. |
| Python version error / `ensurepip` missing | Mac: use the selected Homebrew Python. Ubuntu: install `python3-venv`. Check `"$PYTHON" --version`; this pinned QEMU needs 3.12+. |
| `main directory cannot contain spaces nor colons` | Use a new source/build location without spaces or colons. Quoting the path does not remove QEMU's restriction. |
| `patch does not apply` | Check the exact upstream commit and whether the patch was already applied. Do not use `--reject` and continue with a partial model. |
| `Dependency ... not found` / missing `slirp` | Recheck step 1 and `pkg-config --modversion glib-2.0 pixman-1 slirp`. Reconfigure after resolving dependencies. |
| `ninja: ... build.ninja: No such file` | Configuration did not complete, or you are in the wrong directory. Read the first configure error; do not treat Ninja's message as the original cause. |
| Compiler killed / out of memory | Rerun Ninja with `-j2`; already-built objects are reused. |
| `unsupported machine type` | Check `"$QEMU_BIN" -machine help`. Use the patched executable, not a system QEMU. |
| No window during CI tests | Expected: step 6 is headless. Use step 7 for the color bars. |
| `Display 'cocoa'/'gtk' is not available` | Check `"$QEMU_BIN" -display help`; configure the matching backend and rebuild. Cocoa is Mac-only. |
| GTK cannot open a display | Run in a Linux desktop session or use the headless tests; do not assume SSH provides graphics. |
| Terminal shows `>` and nothing happens | Cancel the incomplete command with Ctrl+C; paste the entire block, checking the final backslashes. |
| Backspace behaves differently in the QEMU window | The GUI forwards keys to the emulated hardware. Use the host terminal for serial-shell typing. Mac's Delete key is normally Backspace; Forward Delete is a different key. Use the terminal escape sequence from step 7 to quit. |
| Color bars appear, but no Android menus | Correct result for `firmware_fb_test.bin`. It is a bare-metal diagnostic. |
| Tier 3 is skipped | Kernel/disk inputs were not found. This is expected for a fresh public checkout. |
| Android screen stays blank despite a test pass | Check actual services/crashes; the current Android stack is incomplete. A framebuffer probe can pass without a functioning framework. |

For an issue report, include the OpenDTV commit, host OS/CPU, Python version, QEMU version, the exact command and its complete error output. Keep firmware images, private ADB keys and other credentials out of public attachments.

## Verification of this guide

Package spellings were checked against the official Homebrew and Ubuntu 24.04 catalogues. Commands, quoting and continuation characters were reviewed for Bash. The macOS source-build and diagnostic results are recorded in the documentation change's verification notes. A fresh Ubuntu installation and Intel Mac run were not performed in this pass; Linux package verification is not being represented as an executed Linux build.

## License

See [LICENSE](LICENSE) and individual source notices. Firmware is not included; its components have separate licensing terms.
