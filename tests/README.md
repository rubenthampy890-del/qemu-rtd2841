# Diagnostic Test Payloads & Verification Suites

This directory contains bare-metal assembly payloads, automated CI test harnesses, and visual verification assets for the RTD2841 emulator.

---

## Source & Binary Mapping

| Binary Target | Assembly Source | Description & Test Assertions |
| :--- | :--- | :--- |
| `firmware_test.bin` | `firmware_test.s` | **Core SoC Diagnostic**: Bootstraps Cortex-A53 in AArch32 mode, verifies GIC-400 distributor (256 INTIDs), tests TC0 27 MHz monotonic timer up-count, probes SDHCI eMMC controller, and validates RTL8168 Ethernet MAC version (`0x50900800`). |
| `firmware_fb_test.bin` | `firmware_fb_test.s` | **Framebuffer Display Test**: Configures 1920x1080 ARGB8888 simple-framebuffer at `0x3E000000`, renders 8 vertical SMPTE color calibration bars, and runs an interactive key-polling loop over UART0. |

---

## Reproducible Compilation

Both diagnostic binaries can be rebuilt from assembly source using the GNU ARM Embedded cross-toolchain:

```bash
# Using Makefile
make clean
make

# Or manually via arm-none-eabi tools:
arm-none-eabi-as -mcpu=cortex-a53 firmware_test.s -o firmware_test.o
arm-none-eabi-objcopy -O binary firmware_test.o firmware_test.bin
rm firmware_test.o

arm-none-eabi-as -mcpu=cortex-a53 firmware_fb_test.s -o firmware_fb_test.o
arm-none-eabi-objcopy -O binary firmware_fb_test.o firmware_fb_test.bin
rm firmware_fb_test.o
```

---

## Automated Multi-Suite CI Harness (`run_ci_test.py`)

Executes the automated end-to-end verification pipeline:

```bash
python3 run_ci_test.py
```

### Test Stages:
1. **Test 1: Core SoC Diagnostics**: Boots `firmware_test.bin` without disk attachment; asserts all peripheral initialization checks pass.
2. **Test 2: SDHCI eMMC Media Insertion**: Boots `firmware_test.bin` with synthetic drive attached; asserts card detection status register (`PRSNT_ST = 0x01FF0000`).
3. **Test 3: Linux 4.4 Kernel Boot & Outbound Network Connectivity**: Boots production Linux 4.4.3 kernel with dual-partition ext4 eMMC storage (`system` and `vendor`); mounts filesystems via checked syscalls in `/init`; asserts live ICMP ping to gateway (`10.0.2.2`), public DNS resolution (`8.8.8.8 -> google.com`), and HTTP payload retrieval (`google.com:80`).
