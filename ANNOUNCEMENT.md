# Community Release & Announcement Templates

Templates formatted for announcing the **QEMU Realtek RTD2841 Machine Model** release across security, embedded, and Android modding communities.

---

## 1. Hacker News / Lobste.rs

**Title:** Show HN: First open-source QEMU emulator for Realtek RTD2841 Smart TV SoCs

**Body:**
> Millions of Smart TVs (TCL RT41 chassis, Xiaomi, Hisense) run on the Realtek RTD2841 / RTK284x SoC family. Until now, upstream QEMU had zero support for this silicon, making firmware modding, security research, and kernel vulnerability analysis risky and blind without physical hardware teardown.
>
> We just released the first open-source bare-metal QEMU machine model (`-M rtd2841-tv030`) for this SoC:
>
> - Quad-core Cortex-A53 in AArch32 mode with PSCI SMC conduit
> - Reverse-engineered Realtek TC0..TC7 27 MHz timer engine and watchdog
> - GIC-400 interrupt distributor (256 lines)
> - 16550A UART0 console with 4-byte stride at `0x18062300`
> - Full memory map definitions and bundled bare-metal verification harness
>
> GitHub: https://github.com/rubenthampy890-del/qemu-rtd2841
>
> Feedback, patches, and driver PRs welcome!

---

## 2. Reddit (r/ReverseEngineering, r/embedded, r/AndroidTV)

**Title:** [Release] First-Ever Open Source QEMU Machine Model for Realtek RTD2841 / TCL RT41 Android TVs

**Post:**
Hey everyone,

If you’ve ever tried to reverse engineer, root, or debloat a modern Smart TV running on Realtek silicon (such as the widespread TCL 40" RT41 chassis / `tv030` profile), you know the biggest hurdle: **JTAG is permanently disabled, and testing custom firmware or modified kernels carries a high risk of hard-bricking the eMMC.**

To solve this, I've developed and released the **first bare-metal QEMU machine model for the Realtek RTD2841 (RTK284x) SoC**:

🔗 **GitHub Repository**: https://github.com/rubenthampy890-del/qemu-rtd2841

### Highlights:
- **Exact MMIO Hardware Map**: Reverse engineered from Realtek kernel sources and live firmware telemetry.
- **Hardware Timers**: Modeled the Realtek TC0–TC7 decrement counter engine clocked at 27 MHz.
- **Interrupt Routing**: GIC-400 with 256 IRQ lines mapped to match silicon SPI routing.
- **Serial Console**: 16550A-compatible UART0 mapped at `0x18062300` with 4-byte stride.
- **Bundled Bare-Metal Harness**: Includes a position-independent diagnostic binary that tests CPU registers, GIC read, and timer ticks live in QEMU.

### Roadmap:
Next up is wiring QEMU's SDHCI controller at `0x18010800` to mount Android `/system` and `/vendor` partition images, and adding a `simple-framebuffer` aperture to render Projectivy Launcher directly to a virtual display.

Contributions and stars are welcome!

---

## 3. XDA Developers (TCL / Android TV Forum)

**Thread Title:** [DEV][QEMU] Bare-Metal Emulator & Machine Model for TCL RT41 Chassis (Realtek RTD2841)

**Post Content:**
Greetings TCL modders and developers,

Testing custom ROMs, debloated firmware, and kernel modifications on TCL RT41 TVs (`V8-R41KT01-LF1`, `TCL-AP-RT41DT-S1`) has always been dangerous because a single boot error bricks the TV without JTAG access.

I have open-sourced a custom QEMU machine model that simulates the TV's Realtek RTD2841 SoC on your PC/Mac:

**GitHub Repo**: https://github.com/rubenthampy890-del/qemu-rtd2841

### What this gives the community:
1. **Zero-Brick Testing**: You can boot and test modified kernels, boot scripts, and system partitions in a safe VM before touching physical flash.
2. **UART Console Logging**: Inspect full early kernel boot logs via terminal stdio without soldering wires to the TV board.
3. **Firmware Research**: Extracted device tree (`rtd284x-tv030.dtb`), hardware memory definitions, and RPC ioctl stubs are included.

Check out the repo, build the patch, and let's bring custom ROMs to the RT41 chassis!

---

## 4. Twitter / X Thread

1/4 🚨 Announcing the first-ever open-source QEMU machine model for the Realtek RTD2841 Smart TV SoC (TCL RT41 chassis)!
Hardware reverse-engineering, GIC-400 interrupt routing, 27MHz timers & UART0 console now emulated.
GitHub: https://github.com/rubenthampy890-del/qemu-rtd2841

2/4 Why this matters: Millions of smart TVs run this chip family, but upstream QEMU had zero support. Security researchers and firmware developers were forced to risk bricking physical hardware. Now you can emulate and debug in Ring-0 via QEMU + GDB.

3/4 Includes:
✅ Quad-core Cortex-A53 in AArch32 mode
✅ Realtek TC0..TC7 27 MHz timer controller
✅ GIC-400 distributor (256 lines)
✅ 16550A UART0 console
✅ Diagnostic bare-metal validation payload

4/4 Repo includes complete QEMU patches, DTBs, and a one-command test launcher. Check it out and star the project on GitHub! ⭐️
https://github.com/rubenthampy890-del/qemu-rtd2841
