# QEMU Realtek RTD2841 / RTK284x Machine Model & Bare-Metal Emulator

> **First-ever open-source bare-metal QEMU machine model and reverse-engineered hardware definition for Realtek Digital TV (DTV) SoCs (RTD2841 / RTK284x family, TCL RT41 Chassis, `tv030` profile).**

---

## 1. Overview

The **Realtek RTD2841** (RTK284x architecture) powers millions of smart TVs globally across manufacturers such as TCL, Xiaomi, Hisense, and RCA. Despite its ubiquity, no public QEMU machine model or hardware virtualization layer has ever been published for this SoC.

This repository provides:
- **`hw/arm/rtd2841.c`**: Production QEMU SoC and machine model (`-M rtd2841-tv030` / `-M rtd2841`) implementing quad-core ARM Cortex-A53 in AArch32 mode, GIC-400 interrupt controller, 1 GB DRAM, 16550A UART0 console, and stubbed RBUS apertures.
- **`hw/arm/rtd2841_timer.c`**: Hardware-accurate model of Realtek's 8-channel timer controller (`TC0`..`TC7`) driven by a 27 MHz reference clock, plus hardware watchdog.
- **`include/hw/arm/rtd2841.h`**: Standalone C header defining all physical base addresses, peripheral apertures, and GIC IRQ lines.
- **`tests/firmware_test.s` / `.bin`**: Standalone position-independent bare-metal ARM diagnostic firmware payload that exercises CPU state, UART0 MMIO, GIC-400, and TC0 countdown in simulation.
- **`dts/`**: Compiled and decompiled device trees for the `tv030` board profile.
- **`patches/`**: Ready-to-apply unified patches for upstream QEMU.

---

## 2. Hardware Architecture & Memory Map

The emulation accurately reflects the silicon memory map extracted from Realtek kernel sources and live firmware telemetry:

```
+------------------------------------------------------------------------+
|                      Realtek RTD2841 SoC Topology                       |
+------------------------------------------------------------------------+
|  [ Cortex-A53 Core 0 ]  [ Cortex-A53 Core 1 ]  [ Core 2 ]  [ Core 3 ]  |
|         (Boot)            (PSCI CPU_ON)      (PSCI CPU_ON)(PSCI CPU_ON)|
+------------------------------------------------------------------------+
                                   │
      ┌────────────────────────────┼───────────────────────────┐
      ▼                            ▼                           ▼
[ 1024 MB DRAM ]          [ ARM GIC-400 GICv2 ]      [ Realtek RBUS MMIO ]
(0x00000000 -             (GICD: 0x18081000)         (0x18000000 -
 0x3FFFFFFF)              (GICC: 0x18082000)          0x1808FFFF)
                          (256 Interrupt Lines)                │
                                                               │
      ┌────────────────────────────┬───────────────────────────┼────────────────┐
      ▼                            ▼                           ▼                ▼
[ UART0 Console ]        [ TC0..TC7 Timers ]          [ Watchdog Timer ] [ eMMC / SDHCI ]
(0x18062300, 27 MHz)     (0x1801B600, 27 MHz)         (0x18062200)       (0x18010800)
(SPI 25 / INTID 57)      (SPI 2 / INTID 34)           (SPI 0 / INTID 32) (SPI 28 / INTID 60)
```

### Complete Physical Memory Layout

| Region Base | Size | Description | Subsystem / IRQ |
| :--- | :--- | :--- | :--- |
| `0x00000000` | 1024 MB | Physical DRAM | Background system memory |
| `0x18000000` | 1 KB | TMU Dual-Block | Thermal Management Unit |
| `0x18000400` | 3 KB | SYSREG / Pinmux | Clock, Reset, Pinmux controls |
| `0x18010800` | 1 KB | eMMC Controller | SDHCI Host Controller (SPI 28) |
| `0x18010C00` | 1 KB | SDIO Controller | Secondary storage aperture |
| `0x1801B100` | 768 B | MISC UART1..3 | Auxiliary serial lines |
| `0x1801B600` | 512 B | Hardware Timers | Realtek TC0..TC7 (27 MHz, SPI 2) |
| `0x18040000` | 16 KB | Mali GPU Aperture | ARM Mali-T6xx GPU registers |
| `0x1805C800` | 256 B | ARM TSG Aperture | Arch Timer APB TSG EN0/1 |
| `0x18062200` | 256 B | Watchdog Timer | Hardware watchdog (SPI 0) |
| `0x18062300` | 32 B | 16550A UART0 | Primary console (4B stride, 27MHz, SPI 25) |
| `0x18080000` | 4 KB | SCU | System Control Unit |
| `0x18081000` | 4 KB | GIC-400 Distributor | GICD (256 INTIDs) |
| `0x18082000` | 8 KB | GIC-400 CPU Interface| GICC |
| `0x3E000000` | 8 MB | Simple Framebuffer | 1920x1080 @ 32bpp ARGB8888 (DRAM Carveout, priority 1) |

---

## 3. Quickstart & Live Simulation

### Option A: Test with Precompiled QEMU Binary
If using a pre-patched QEMU binary:
```bash
# Clone this repository
git clone https://github.com/rubenthampy890-del/qemu-rtd2841.git
cd qemu-rtd2841

# Execute live bare-metal diagnostic simulation
./scripts/run_qemu.sh
```

### Option B: Patch Upstream QEMU from Source
```bash
# Clone upstream QEMU
git clone https://gitlab.com/qemu-project/qemu.git
cd qemu

# Apply complete RTD2841 patch
git apply /path/to/qemu-rtd2841/patches/qemu-rtd2841-complete.patch

# Configure and build aarch64 emulator
mkdir build && cd build
../configure --target-list=aarch64-softmmu --disable-werror
ninja qemu-system-aarch64

# Verify machine registration
./qemu-system-aarch64 -M help | grep rtd2841
```

Expected output:
```text
rtd2841              Realtek RTD2841 SoC / TCL Smart TV (board tv030) (alias of rtd2841-tv030)
rtd2841-tv030        Realtek RTD2841 SoC / TCL Smart TV (board tv030)
```

---

## 4. Live Simulation Verification Output

When running the bundled bare-metal payload (`tests/firmware_test.bin`), QEMU executes Cortex-A53 Core 0 at physical reset vector `0x00000000` in AArch32 Supervisor mode, writing diagnostic data through UART0 directly to stdio:

```text
========================================================
[RTD2841-TV030] Bare-Metal Simulation Starting...
[RTD2841-TV030] CPU 0: ARM Cortex-A53 in AArch32 Mode
[RTD2841-TV030] UART0 Output via 0x18062300: ACTIVE
========================================================
[+] Realtek GIC-400 Interrupt Controller (0x18081000) verified.
[+] Realtek TC0 Hardware Timer (0x1801B600) configured (27MHz, CR=0xC0000000).

[>>> RTD2841 QEMU HARDWARE SIMULATION PASSED <<<]
```

### Verified Register State
```text
CPU#0
R00=18062300 R01=0000021c R02=0000000a R03=00000000
R04=1801b600 R05=0004ff9c R06=00000000 R07=00000000
R08=00000000 R09=00000000 R10=00000000 R11=00000000
R12=00000000 R13=00100000 R14=00000198 R15=00000204
PSR=600001d3 -ZC- A S svc32
```

---

## 5. Reverse Engineering Findings

1. **AArch32 Core Configuration**: Although the Cortex-A53 is a 64-bit ARMv8-A core, Realtek's production DTV firmware executes strictly in 32-bit `armv7l` / `armhf` mode. The model forces `aarch64=false` and resets into low exception vectors (`reset-hivecs=false`).
2. **UART Register Stride**: UART0 uses standard 16550A registers, but addresses are mapped on a **4-byte stride** (`shift = 2`) rather than contiguous bytes. Clock base is derived from the 27 MHz crystal oscillator (`baudbase = 1687500`).
3. **Timer Decrement Engine**: Realtek TC0..TC7 timers are down-counters with separate value (`TVR`, offset `0x00`), count (`CV`, offset `0x20`), and control (`CR`, offset `0x40`) registers. Bit 31 enables the timer; bit 30 selects periodic auto-reload.
4. **RPC & Vendor Daemons**: Userland services (`rtkd`, `cusd`, `pmoded`) communicate with `/dev/rpc*` and `/dev/sys-misc` via custom ioctls. C headers and shims are provided in [`analysis/rtk_rpc_stubs.h`](analysis/rtk_rpc_stubs.h).

---

## 6. Hardware Virtual Display Pipeline & Simple-Framebuffer

The machine model implements a virtual display pipeline backed by an 8 MB contiguous DRAM carveout and QEMU `GraphicConsole`:

- **Physical Base**: `0x3E000000` (offset 992 MB in 1024 MB physical DRAM space).
- **Resolution**: `1920x1080` @ 32 bpp (`ARGB8888` / `XRGB8888`).
- **Stride**: `7680` bytes (`1920 * 4`).
- **Buffer Size**: `8,294,400` bytes (carved out as 8 MB `0x00800000` memory region with priority 1 overlay).
- **Device Tree Node**:
  ```dts
  framebuffer@3e000000 {
      compatible = "simple-framebuffer";
      reg = <0x3e000000 0x00800000>;
      width = <1920>;
      height = <1080>;
      stride = <7680>;
      format = "a8r8g8b8";
      status = "okay";
  };
  ```

### Graphical Display Backends

```bash
# Launch with native macOS Darwin Cocoa GUI window (Retina 1080p scaled)
DISPLAY_BACKEND=cocoa ./scripts/run_qemu.sh tests/firmware_fb_test.bin

# Launch with VNC server on port 5900
DISPLAY_BACKEND="vnc=:0" ./scripts/run_qemu.sh tests/firmware_fb_test.bin

# Headless / CI simulation (default)
./scripts/run_qemu.sh tests/firmware_fb_test.bin
```

When running `tests/firmware_fb_test.bin`, the bare-metal payload renders 8 SMPTE color bars across the full 1920x1080 simple-framebuffer at `0x3E000000`, verifying the DRAM carveout and display pipeline end-to-end.

---

## 7. Roadmap

- [x] Bare-metal SoC model with Cortex-A53 quad-core and AArch32 support.
- [x] ARM GIC-400 interrupt controller wiring (256 IRQs).
- [x] 16550A UART0 console with 4-byte stride.
- [x] Realtek TC0..TC7 27 MHz timer controller & hardware watchdog.
- [x] Standalone bare-metal diagnostic validation payload.
- [x] Connect QEMU `sysbus-sdhci` at `0x18010800` for raw Android `/system` and `/vendor` eMMC image mounting.
- [x] Add `simple-framebuffer` display aperture at `0x3E000000` for graphical VNC / Cocoa window rendering.
- [ ] Automated kernel cross-compilation harness for Linux 4.4 `zImage`.

---

## 8. License

This project is licensed under the **GNU General Public License v2.0 or later** (`GPL-2.0-or-later`), consistent with QEMU licensing.
