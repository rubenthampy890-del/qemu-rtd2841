# Diagnostic Test Payloads & Verification Suites

This directory contains bare-metal assembly payloads, automated CI test harnesses, and visual verification assets for the RTD2841 emulator.

---

## Source & Binary Mapping

| Binary Target | Assembly Source | Description & Test Assertions |
| :--- | :--- | :--- |
| `firmware_test.bin` | `firmware_test.s` | **Core SoC Diagnostic**: Bootstraps Cortex-A53 in AArch32 mode, verifies GIC-400 distributor (256 INTIDs), tests TC0 27 MHz monotonic timer up-count, verifies real CPU interrupt trap delivery (INTID 34) into ARM vector table, probes SDHCI eMMC controller, validates RTL8168 Ethernet MAC version (`0x50900800`), and verifies bare-metal RTL8168 bidirectional packet TX/RX (ARP exchange with 10.0.2.2) via SLIRP backend. |
| `firmware_fb_test.bin` | `firmware_fb_test.s` | **Framebuffer Display Test**: Writes eight vertical color bars into the 1920x1080, 32-bit framebuffer at `0x3E000000`, checks memory contents, and polls UART0 for input. |
| `firmware_smp_test.bin` | `firmware_smp_test.s` | **Secondary CPU Diagnostic**: Checks initial mailbox state, releases cores 1–3 individually, and checks that each reports the correct CPU identity while unreleased cores remain parked. |

---

## Reproducible Compilation

Run these **host** commands from the repository root, after installing the
prerequisites in [the setup guide](../README.md#1-install-the-host-tools):

```bash
make -C tests clean && make -C tests
dtc -I dts -O dtb -o dts/rtd284x-tv030.dtb dts/rtd284x-tv030.dts
```

The Makefile assembles **and links** each ARM32 program at `0x10000` before
converting it to a raw `.bin`. Converting an unlinked `.o` directly with
`objcopy` skips relocation and is not the supported build procedure.
It also builds `firmware_smp_test.bin` and checks the secondary CPU startup
assembly against the board source. Building that payload does not mean the
main CI runner executes it.

---

## Automated Multi-Suite CI Harness (`run_ci_test.py`)

From the repository root, with `QEMU_BIN` and `PYTHON` set as in the setup guide:

```bash
QEMU_BIN="$QEMU_BIN" "$PYTHON" tests/run_ci_test.py
```

### Testing Tiers:

1. **Tier 1: Core SoC Diagnostics**: Boots `firmware_test.bin` with `-nic user,model=rtd2841-eth` without disk attachment; asserts active up-count, hardware IRQ trap (`INTID 34`) delivery, bidirectional packet TX/RX via SLIRP backend, and overall subsystem pass.
2. **Tier 2: SDHCI eMMC Media Insertion**: Boots `firmware_test.bin` with synthetic drive attached (`-drive file=...,format=raw,if=sd`); asserts card detection status register (`PRSNT_ST = 0x01FF0000`) and block ready state.
3. **Tier 3: Linux 4.4 Kernel Boot & Outbound Network Connectivity**: Boots production Linux 4.4.3 kernel with compatible prepared ext4 eMMC storage (root filesystem and vendor partitions); mounts filesystems via checked syscalls in `/init`; asserts live ICMP ping to gateway (`10.0.2.2`), public DNS resolution (`8.8.8.8 -> google.com`), and HTTP payload retrieval (`google.com:80`).

On a public checkout, Tier 3 is skipped because its kernel and firmware disk are
not included. A pass covers only the tiers actually executed. See the
[prepared-asset instructions](../README.md#linux-verification-with-already-prepared-assets)
for required inputs and strict mode. The color-bar display has its own
[launch command](../README.md#5-open-the-display-window).
