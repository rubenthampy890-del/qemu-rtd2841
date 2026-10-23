# Building & Running the RTD2841 QEMU Model

Instructions to apply the model against upstream QEMU, build it, and run the test payloads.

---

## Prerequisites

### macOS (Homebrew)
```bash
brew install ninja meson pkg-config glib pixman dtc python3
```

### Linux (Debian / Ubuntu)
```bash
sudo apt update
sudo apt install -y build-essential git ninja-build meson pkg-config \
    libglib2.0-dev libpixman-1-dev device-tree-compiler python3
```

### Linux (Fedora / RHEL)
```bash
sudo dnf install -y git ninja-build meson gcc pkgconf-pkg-config \
    glib2-devel pixman-devel dtc python3
```

---

## Getting QEMU

The model is verified against upstream QEMU master. Compatibility shims (`__has_include("system/address-spaces.h")`) handle both modern QEMU (9.1+ / master with `system/` headers) and 8.2 / 9.0 (`sysemu/` headers).

```bash
git clone https://gitlab.com/qemu-project/qemu.git
cd qemu

# Verified baseline commit (or v9.1.0+):
git checkout a19f59a72d96056d9a8f48cf6c77f5787491e414
```

---

## Applying the Patch

The unified patch modifies `hw/arm/Kconfig`, `hw/arm/meson.build`, and adds the machine model:

```bash
git apply /path/to/qemu-rtd2841/patches/qemu-rtd2841-complete.patch
```

Or copy files manually:
```bash
cp /path/to/qemu-rtd2841/qemu-model/rtd2841.c hw/arm/
cp /path/to/qemu-rtd2841/qemu-model/rtd2841_timer.c hw/arm/
cp /path/to/qemu-rtd2841/qemu-model/rtd2841.h include/hw/arm/
```
And add `CONFIG_RTD2841` to `hw/arm/Kconfig` and `hw/arm/meson.build` as shown in `patches/qemu-meson-kconfig.patch`.

---

## Building QEMU

Configure for `aarch64-softmmu`:

```bash
mkdir build && cd build

../configure \
    --target-list=aarch64-softmmu \
    --disable-werror

ninja qemu-system-aarch64
```

Verify machine registration:
```bash
./qemu-system-aarch64 -M help | grep rtd2841
```

Output:
```text
rtd2841              Realtek RTD2841 SoC / TCL Smart TV (board tv030) (alias of rtd2841-tv030)
rtd2841-tv030        Realtek RTD2841 SoC / TCL Smart TV (board tv030)
```

---

## Device Trees

Device trees are located in `dts/`. Compile with `dtc`:

```bash
cd /path/to/qemu-rtd2841

# Compile tv030 board profile
dtc -I dts -O dtb -o dts/rtd284x-tv030.dtb dts/rtd284x-tv030.dts

# Compile generic reference
dtc -I dts -O dtb -o dts/rtd284x.dtb dts/rtd284x.dts
```

---

## Storage Images

To simulate the physical eMMC partitions (`system.img`, `vendor.img`, `res.img`):

```bash
cd /path/to/qemu-rtd2841
python3 tools/create_emmc_storage.py storage.img
```

---

## Running & Testing

### Automated Hardware & Linux CI Verification Suite
Runs the end-to-end multi-suite test harness verifying Core Diagnostics, eMMC card insertion, and Linux 4.4 kernel boot with outbound network connectivity:

```bash
python3 tests/run_ci_test.py
```

### Bare-Metal Hardware Diagnostics
Runs the test firmware (`tests/firmware_test.bin`) over serial console:

```bash
./build/qemu-system-aarch64 \
    -M rtd2841-tv030 \
    -display none \
    -serial stdio \
    -kernel /path/to/qemu-rtd2841/tests/firmware_test.bin
```

### Storage Detection (eMMC / SD Card)
Attach a raw image via `-drive`:

```bash
dd if=/dev/zero of=storage.img bs=1M count=64

./build/qemu-system-aarch64 \
    -M rtd2841-tv030 \
    -display none \
    -serial stdio \
    -drive file=storage.img,format=raw,if=sd \
    -kernel /path/to/qemu-rtd2841/tests/firmware_test.bin
```

Output confirms SDHCI card detection:
```text
[PASS] SDHCI eMMC (0x18010800): Controller online, media present (PRSNT_ST=0x01FF0000).
```

### Framebuffer Display Verification
Renders 8 SMPTE color bars into the 1920x1080 simple-framebuffer:

```bash
# Cocoa GUI window (macOS):
./build/qemu-system-aarch64 \
    -M rtd2841-tv030 \
    -serial stdio \
    -kernel /path/to/qemu-rtd2841/tests/firmware_fb_test.bin

# VNC:
./build/qemu-system-aarch64 \
    -M rtd2841-tv030 \
    -display vnc=:0 \
    -serial stdio \
    -kernel /path/to/qemu-rtd2841/tests/firmware_fb_test.bin
```

### Linux Kernel Boot with RTL8168 Network & Storage
```bash
./build/qemu-system-aarch64 \
    -M rtd2841-tv030 \
    -m 1024M \
    -kernel /path/to/zImage \
    -dtb /path/to/qemu-rtd2841/dts/rtd284x-tv030.dtb \
    -drive file=/path/to/emmc.raw,if=sd,format=raw \
    -nic user,model=rtd2841-eth,hostfwd=tcp::5555-:5555 \
    -append "console=ttyS0,115200 root=/dev/mmcblk0p1 rw rootwait earlyprintk" \
    -serial stdio
```
