# Hardware & Register Notes: Realtek RTD2841 / RTK284x

Hardware notes and register references collected from reversing the Realtek Linux 4.4 vendor kernel tree (`arch/arm/mach-rtd284x/`, `include/rbus/`) and extracted `tv030` device trees.

---

## CPU & SMP Bringup

- **Cores**: Quad ARM Cortex-A53.
- **Execution Mode**: 32-bit AArch32. Realtek's Android TV stack (TCL RT41) is built for `armv7-a` with VFPv4/NEON.
- **Boot Flow**: Core 0 jumps directly to reset vector (`0x00000000`). Cores 1..3 remain powered down until PSCI `CPU_ON` is invoked or the kernel writes to the jump mailbox.
- **SMP Mailbox (`arch/arm/mach-rtd284x/include/mach/system.h`)**:
  ```c
  #define DUMMY_REG_FOR_2ND_CPU_JUMPADDRESS    0x00000504
  #define DUMMY_REG_FOR_3RD_CPU_JUMPADDRESS    0x00000508
  #define DUMMY_REG_FOR_4TH_CPU_JUMPADDRESS    0x0000050C
  ```
  In QEMU, `boot_info.smp_bootreg_addr` is set to `0x00000504`.

---

## Interrupt Controller (GIC-400)

- **Base Addresses**:
  - GICD (Distributor): `0x18081000` (4 KB)
  - GICC (CPU Interface): `0x18082000` (8 KB)
- **IRQ Lines**: 256 physical interrupt lines (verified via `GICD_TYPER = 0x00000067`, ITLinesNumber = 7 -> `32 * (7 + 1) = 256`).
- **PPI Mapping (ARM Arch Timers)**:
  - INTID 26: Hypervisor Timer (PPI 10)
  - INTID 27: Virtual Timer (PPI 11)
  - INTID 29: Secure Physical Timer (PPI 13)
  - INTID 30: Non-Secure Physical Timer (PPI 14)
- **Notable SPIs**:
  - SPI 0 (INTID 32): Hardware Watchdog (`IRQ_WDOG`)
  - SPI 2 (INTID 34): TC0..TC7 Timers (`IRQ_TIMER`)
  - SPI 18 (INTID 50): eMMC Controller (`IRQ_EMMC`)
  - SPI 25 (INTID 57): UART0 (`IRQ_UART0`)

---

## 16550A UART0 Console

- **Base**: `0x18062300`
- **Clock**: 27 MHz crystal oscillator (`TIMERINFO_BASE_CLOCK`)
- **Baud Base**: `27000000 / 16 = 1687500`
- **Register Stride**: 4 bytes (`reg_shift = 2`). Unlike typical PC 16550 UARTs with byte spacing, Realtek spaces the registers across 32-bit words on the RBUS.
- **Reference**: `arch/arm/mach-rtd284x/serial.c` and `include/rbus/misc_uart_reg.h`.

---

## Hardware Timers (TC0..TC7)

- **Base**: `0x1801B600` (512-byte aperture on RBUS)
- **Clock**: 27 MHz
- **Interrupt**: Shared SPI 2 -> GIC INTID 34
- **Register Layout (`include/rbus/timer_reg.h`)**:
  Unlike ARM SP804 which clusters registers per timer, Realtek lays them out in flat arrays across the aperture:
  - `0x00..0x1C`: `TIMER_TC0TVR` .. `TC7TVR` (target / reload values, 8x u32)
  - `0x20..0x3C`: `TIMER_TC0CVR` .. `TC7CVR` (current running counter, 8x u32)
  - `0x40..0x5C`: `TIMER_TC0CR` .. `TC7CR` (control registers)
  - `0x60..0x7C`: `TIMER_TC0ICR` .. `TC7ICR` (interrupt enable)
  - `0xE8`: `TIMER_UMSK_ISR` (unmasked raw status, bit `n` = `TCn`)
  - `0xEC`: `TIMER_ISR` (masked status, write-1-to-clear)
- **Control Register Bits**:
  - Bit 31 (`tc_en`): 1 = enable countdown
  - Bit 30 (`tc_mode`): 1 = periodic auto-reload from `TVR`, 0 = one-shot
  - Bit 29 (`tc_pause`): 1 = pause counter
- Implemented in `hw/arm/rtd2841_timer.c` using QEMU `ptimer`.

---

## Watchdog

- **Base**: `0x18062200`
- **Interrupt**: SPI 0 -> GIC INTID 32
- **Registers (`include/rbus/wdog_reg.h`)**:
  - `0x00`: `WDOG_MSK_ISR`
  - `0x04`: `WDOG_TCWCR` (enable, counter, NMI control)
  - `0x08`: `WDOG_TCWTR` (kick / clear counter)
- Implemented as a register stub so Linux watchdog drivers don't trigger reboot loops on probe.

---

## eMMC / SDHCI Storage

- **Base**: `0x18010800`
- **Interrupt**: SPI 18 -> GIC INTID 50
- Realtek uses a standard SDHCI-compliant host controller for internal eMMC.
- DTS node (`arch/arm/boot/dts/merlin2.dts`):
  ```dts
  emmc@0x18010800 {
      compatible = "realtek,rtk-emmc";
      reg = <0x18010800 0x400>;
      interrupts = <0 18 4>;
  };
  ```
- In QEMU, this is backed by `TYPE_SYSBUS_SDHCI` so standard raw disk images (`-drive file=...,format=raw,if=sd`) mount out of the box.

---

## Simple Framebuffer

- **Address**: `0x3E000000` (8 MB carveout at the top of 1024 MB DRAM)
- **Geometry**: 1920x1080 @ 32bpp ARGB8888, stride = 7680 bytes
- Mapped as an overlay with priority 1 over system RAM.
- DTS node:
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

---

## RBUS Peripheral Window

- **Base**: `0x18000000`
- **Size**: 4 MB (`0x00400000`)
- **Source**: `arch/arm/mach-rtd284x/include/mach/iomap.h`:
  ```c
  #define RBUS_BASE_PHYS      0x18000000
  #define RBUS_BASE_SIZE      0x00400000
  ```
- Sub-blocks inside the 4 MB window (TMU, pinmux, clock gates, Mali GPU at `0x18040000`, TSG at `0x1805C800`) are mapped as unimplemented devices at background priority. Reads return 0, preventing kernel bus aborts when probing unmodeled silicon blocks.
