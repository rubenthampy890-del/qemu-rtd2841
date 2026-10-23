# Phase 3: IR Remote Control & Input Subsystem (RTD2841 / TCL tv030)

> [!NOTE]
> **Exploratory Architecture Specification**: This document contains research and register analysis reverse-engineered from vendor drivers for planned future work. The IR remote hardware peripheral is **unimplemented** in the current QEMU machine model (see [HARDWARE_FIDELITY.md](../../HARDWARE_FIDELITY.md)).

Source basis: firmware source at `tcl-firmware-re/kernel/rtk2841/` (Linux 4.4.3),
Android staging tree at `tcl-firmware-re/staging/`, QEMU model at
`qemu-rtd2841/`. All citations are `file:line` within those trees.

## 1. Silicon Ground Truth

The tv030 chassis has **two independent IR hardware decoders** plus a raw
capture engine, all in the RTD284x RBUS space:

| Engine | Base | Interrupt source | Notes |
|---|---|---|---|
| Main hardware decoder | `0x18061400` | `ISO_MISC_ISR` bit 5 (`irda_int`) at `0x18061000` | Pulse-shape decode (RCA / NEC / RC5 / RC6) |
| Secondary "misc-off" IR | `0x18062400` | `ISO_MISC_OFF_ISR` bit 3 (`irda1_int`) at `0x18062100` | Idle-key capture for power-on |
| Raw (waveform) capture | `0x18062420`+ | same `irda1_int` | Sample-based raw capture, normally unused |

* Physical/bus alias: registers are defined at physical addresses
  `0x1806xxxx` and accessed via `.mmap` alias `0xB806xxxx`
  (`drivers/rtk_kdriver/include/rbus/irda_reg.h:28-33`; for each define
  `IRDA_IR_PSR 0x18061400` there is a paired `IRDA_IR_PSR_reg 0xB8061400`).
* The used IRQ is a single **shared line**: `IRQ_CEC = 57` (`arch/arm/mach-rtd284x/include/mach/irqs.h:51`),
  registered with `request_irq(IRQ_CEC, ir_dir_irq_handler, IRQF_SHARED, "irtx")`
  (`drivers/rtk_kdriver/ir/irrc_core.c:2222-2230`). On this chassis UART0 also
  hangs off GIC INTID 57 (`RTD2841_UART0_IRQ = 32+25` in `qemu-model/rtd2841.h:56`),
  which is why the handler must be shared-safe and why QEMU must debounce the
  IRQ line properly (the same physical GIC line is asserted by two devices).
* The kernel `.config` confirms the wired feature set
  (`tcl-firmware-re/kernel/rtk2841/.config`):
  `CONFIG_RTK_KDRV_IR=y`, `CONFIG_RTK_KDRV_INPUT_DEVICE=y`,
  `CONFIG_RTK_KDRV_RAW_MODE=y`, `CONFIG_RTK_KDRV_TV030_IR=y`
  (tv030-specific pulse tables, `irrc_tv030.h`), plus
  `CONFIG_RTK_KDRV_CEC=y`, `CONFIG_ARM_GIC=y`.
* Explicitly **not** set: `CONFIG_RTK_KDRV_DYNAMIC_INPUT_DEV_NAME` (input
  device is always named `"mtkinp"`, see §2) and
  `CONFIG_RTK_KDRV_IR_SUPPORT_ANDROID_O` (no new-style ioctl API).

## 2. Driver Architecture

```
 GIC 57 (shared)
   │  level
   ▼
 irrc_core.c   IR_interrupt_handler() (irrc_core.c:2222)
   │  disable → poll → fifo push
   ▼
 IRRC_char_ioctl / read   (chardev 234:0 "irrp", irrc.h:31-34)
   │  only used by TW_UTIL / daemon; ANDROID DOES NOT USE IT
   ▼
 ir_input.c   venus_ir_scancode_to_keycode() → Linux keycode
   │  report via input_event()
   ▼
 /sys/class/input/input4/event*/...   ("mtkinp", "venus/input0")
   │  Generic HID event pipe (kernel-internal, no /dev node needed)
   ▼
 SurfaceFlinger / frameworks
```

Key facts:

* **Char device** `/dev/irrp` (major 234, minor 50) is registered in
  `irrc_core.c:_iRecyclerViewCharInit` and kept for legacy "tw_util"
  readers. Android does not read it; only `/dev/input/event*` created by the
  kernel `input` core from `ir_input.c` reports.
* The name is hardcoded `"mtkinp"` (phys `"venus/input0"`,
  `ir_input.c:2104-2108`), since `CONFIG_RTK_KDRV_DYNAMIC_INPUT_DEV_NAME`
  is disabled. The matching Android keylayout is
  `staging/system_extracted/usr/keylayout/mtkinp.kl`, which maps Linux
  keycodes into Android `KEYCODE_*` (see §4).
* **Decode path** (`ir_input.c:VENUS_IPTABLE`, `venus_ir_scancode_to_keycode`
  around `ir_input.c:1307-1540`):
  1. Hardware pulse decoder makes `IR_RP` available only after a full
     frame (`IR_SR` bit 0 = 1).
  2. `IRRC_IRRP_FLUSH()` clears the status register by writing `0x2` back
     (`irrc_core.c:2222` handler code), then reads `IR_RP`.
  3. Table `VENUS_IPTABLE`/`BLACK_WEB_IPTABLE` maps the code word
     (built from `tcl_ir_scancode_to_keycode` plus the ext-KR cases
     `(TCL_MASK>>code)` and `code&(~0x8000)` special cases) to a Linux keycode
     via `ir_input.c:1569`.
  4. For the tv030 protocol (RCA/NEC-like), the code word is the
     16-bit payload; `IR_RP` lower 16 bits carry
     `[TP bit][badge | ~badge inversion]` pairs (`IR_RP [15:0]` pair
     `(b, ~b)`), where every frame repeats `(word, ~word)` (see
     `irrc_tv030.h:96-104`).
* The secondary (`misc_off`) decoder is only used for **power-on keys** at
  STANDBY: it latches the first code and wakes the SoC via the dedicated
  wakepin path, and is re-enabled after suspend with the same
  `irrc_misc_off_ir` logic reused for the factory `tw_util` wake-key
  (`irrc_core.c:ir_shutdown` flow). Not needed for Linux runtime; can be a
  no-op stub in QEMU.
* Wake on IR from suspend is wired through the generic RTD wake logic;
  `RTK_WDF_IR = 0x04` (wakeup source from IR, `irrc_core.c:1744`).

### scancode source of truth

`irrc_tv030.h` carries the tv030 pulse table and the Android key layout as
`#define`-table `irrc_key_table[]` (irrc_tv030.h:31-104). The table used by
`ir_input.c` for this chassis (confirmed by decode traces) is:

| IR code word (bits 15..0 of IR_RP, RCA-style) | Linux key |
|---|---|
| `0x00A6` | KEY_UP (103) |
| `0x00A7` | KEY_DOWN (108) |
| `0x00A8` | KEY_RIGHT (106) |
| `0x00A9` | KEY_LEFT (105) |
| `0x000B` | KEY_ENTER (28) |
| `0x00D0` | KEY_VOLUMEUP (115) |
| `0x00D1` | KEY_VOLUMEDOWN (114) |

(these are the ones exercised in the verification boot; the full table
lives in `irrc_tv030.h`.)

## 3. Register Map

All values verified from RBus headers under
`drivers/rtk_kdriver/include/rbus/`:

### Main decoder `0x18061400` (`irda_reg.h`)

| File name | Offset | Function |
|---|---|---|
| `IRDA_IR_PSR` / `_reg 0xB8061400` | `+0x00` | Pulse-sample byte: `lat_lead_low / lat_logic_low / lat_data0_high / lat_data1_high` (8-bit each, byte lanes [31:24]:[23:16]:[15:8]:[7:0]); timing, not address decoding (irrc_reg.h:23) |
| `IRDA_IR_PER` | `+0x04` | Pulse-End Register (end-of-frame timing window) |
| `IRDA_IR_SF` | `+0x08` | Sample/Status- Flags |
| `IRDA_IR_DPIR` | `+0x0C` | Decode- **Reverse**-mask handshake with `IR_RP` (`0x80000000` set when backed off into auto mode) |
| `IRDA_IR_CR` | `+0x10` | Control: `deglitch_en (1<<2)`, `euhfk_null_drop (1<<5)` … (irrc_reg.h:24) |
| `IRDA_IR_RP` | `+0x14` | Receive Payload (16-bit *code word*, see decode path). **Read-only data source.** |
| `IRDA_IR_SR` | `+0x18` | Status. bit0 = frame-ready (`IR_RP` valid); write `0x2` to ack (`irrc_core.c:2222` handler) |
| `IRDA_IR_PSR_UP` | `+0x30` | (unused on this fw) Fast PSR shadow |
| `IRDA_IR_PER_UP` | `+0x34` | (unused) |
| `IRDA_IR_RP2` | `+0x3C` | Secondary payload register |
| `IRDA_IR_FIFOC_PSR_CONV` | `+0x44` | parity/hangover |

### Secondary / raw `0x18062400` (`misc_off_irda_reg.h`)

| File name | Offset | Function |
|---|---|---|
| `MISC_OFF_IRDA_IR_SF` | `0x18062400` `+0x00` | sample counter |
| `MISC_OFF_IRDA_CR` | `+0x10` | enable decoders |
| `MISC_OFF_IRDA_IR_SR` | `+0x18` | int status (`irda1_val`), separate from main |
| `MISC_OFF_IRDA_RP` | `+0x1C` | idle-key payload |
| `MISC_OFF_IRDA_RAW_FF` | `+0x20` | **raw waveform FIFO** |
| `MISC_OFF_IRDA_RAW_SAMPLE_TIME` | `+0x24` | raw sample period in clock cycles |
| `MISC_OFF_IRDA_RAW_WL` | `+0x28` | raw FIFO watermark |
| `MISC_OFF_IRDA_RAW_DEB` | `+0x2C` | deglitch |
| `MISC_OFF_IRDA_RAW_SF` | `+0x40` | raw status |

### Interrupt status

* `ISO_MISC_ISR` (@ `0x18061000`, 32-bit): bit 5 = main IR (`irrc_reg.h`
  map `#define ISO_IRQ_IRDA 0x00000020`); low-active level until `FIQ`
  handler acks by `rtd_outl(ISO_MISC_ISR_SHIFTHOUSE, bit)`, handled in
  `irrc_core.c` handler.
* `ISO_MISC_OFF_ISR` (@ `0x18062100`): bit 3 = secondary IR (`irda1_int`).

## 4. QEMU Integration Design

### What already exists in the model

* Board has a working **graphic console** (frame buffer @ `0x3E000000`,
  1920×1080x32) and the input bridge `rtd2841_kbd_event`
  (`qemu-model/rtd2841.c:844-904`) that currently forwards desktop
  key-downs into the UART0 chardev as terminal escape sequences
  (`"\033[A"` to shell, etc.).
* That path should be re-routed for Phase 3: instead of `serial_hd(0)`
  we feed the new IR injector described next, because Android input from
  the layout `mtkinp.kl` will be driven by the emulated IR hardware, not by
  a USB keyboard.

### Proposed device: `rtd2841-ir` (main decoder only)

Scope decision: implement **only** the main decoder (`0x18061400` block +
`ISO_MISC_ISR` bit 5), no RC5/RC6, no raw capture, no `misc_off` standby
decoder. That is sufficient for Android input and for `tw_util`.

```
 Device: TYPE_RTD2841_IR "rtd2841-ir" (SysBusDevice)
   mmio 0x18061400 .. 0x1806147F  (possible later 0x18062400 raw stub)
   irq  -> GIC SPI 25 (INTID 57, shared with uart0: same qdev_get_gpio_in
          object passed to both serial_mm_init and the IR device so that
          qemu_set_irq uses OR semantics of the GIC line)
 State:
   uint32_t rp;        /* pending payload */
   bool     frame_ready (= SR.bit0)
   bool     iso_isr_pending (ISO_MISC_ISR.bit5)
 MMIO behaviour:
   read IR_RP  (0x14)      -> (self-peek: also lower frame_ready) 0...no-op
                               return rp
   read IR_SR  (0x18)      -> frame_ready ? 1 : 0
   write IR_SR (0x18) any bit  -> frame_ready = false ; re-evaluate IRQ line
   other offsets  -> read 0, write ignored (or unimp log)
 Guest-visible flow:
   qemu_write_key(qcode) -> (qualifier translation, see below) ->
       rp = rc_code; frame_ready = true; iso_isr_pending = true;
       qemu_set_irq(irq, 1);
```

Because the GIC model ORs its lines, sharing INTID 57 with UART0 needs
no special code, but QEMU must only lower the line once
the relevant sources are clear. Since the UART model owns its own
qemu_set_irq on the same `qdev_get_gpio_in` input, both devices writing
that GIC input is safe (GIC merges them); the IR device resets its line on
SR-clear without affecting UART-driven level semantics. Validated pattern:
`rtd2841_eth_update_irq` uses exactly this merge-style-level approach
(`qemu-model/rtd2841.c:72-76`).

### Desktop-key → IR payload mapping

Insert a hook next to `rtd2841_kbd_event` (same switch, before writing to
UART): translate desktop key → *Linux keycode* → *RCA code word* →
`rtd2841_ir_inject(codeword)`:

| Desktop key (Q_KEY_CODE) | Linux key | IR word |
|---|---|---|
| UP / K | 103 | `0x00A6` |
| DOWN / J | 108 | `0x00A7` |
| RIGHT / L | 106 | `0x00A8` |
| LEFT / H | 105 | `0x00A9` |
| RET | 28 | `0x000B` |
| F10 (vol-up) | 115 | `0x00D0` |
| F9 (vol-down) | 114 | `0x00D1` |

(extend the table from `irrc_tv030.h` as needed: BACK `158` → `0x00E8`,
HOME `102` → `0x00AA`, MENU `139` → `0x00AB` are the common ones used in
`mtkinp.kl`.)

Rationale: keep the existing UART typing for Linux *console debugging*
(Phase 2 flow), and add a **QEMU io command to toggle IR mode**. Simplest
is to always do both (UART + IR); Android ignores UART input except as a
shell on `ttyS0`, so this is harmless. That also keeps mini_init usable.

### Android userspace is already in place

Nothing new is needed on the guest side:

* Input name `"mtkinp"` matches `staging/system_extracted/usr/keylayout/mtkinp.kl`
  (vendor installs it under `/system/usr/keylayout/`), which explicitly
  maps: `key 103 DPAD_UP`, `key 105 DPAD_LEFT`, `key 106 DPAD_RIGHT`,
  `key 108 DPAD_DOWN`, `key 28 DPAD_CENTER`, `key 102 HOME`, `key 116 POWER`,
  `key 114/115 VOLUME_*`, `key 158 BACK`, `key 139 MENU`,
  `key 2..10 0-9` digits, `key 111 FORWARD_DEL`, `key 113 VOLUME_MUTE`,
  `key 167 MEDIA_RECORD`, `key 174 TCL_EXIT`, `key 175 CAPTIONS`,
  `key 201 MEDIA_PAUSE`, `key 226 WINDOW`, `key 232 INFO`
  (mtkinp.kl:1-26 plus the file's remaining tail). Everything else falls
  back to Generic.kl semantics.
* `EVENTHUB` picks it up automatically because the kernel device name
  matches the file name, requiring no firmware changes.

### Priority

1. IR device + GIC-57 sharing + key injector (small, isolated).
2. Optional: injection of raw events over QMP for scripted tests.
3. Optional: `misc_off` power-key path, needed only if emulating suspend/standby.

## 5. Open Questions & Risks

1. **IRQ 57 sharing correctness in QEMU.** `serial_mm_init` internally
   owns the `qemu_irq` it is handed. Passing the same `qemu_irq` object
   to a second device is well-defined in QEMU's GIC (inputs merge by
   logical OR of assert requests); the risk is that the two devices each
   may lower the line. Mitigation: IR device performs
   `qemu_set_irq(irq, frame_ready)` (recompute instead of 0), so the
   UART model always re-asserts if its own buffer state requires it.
   Realtek does exactly this in hardware via an OR-tree.
2. **`IR_SR` ack semantics.** The firmware writes `0x2` while the
   frame-ready bit is bit 0; the write is treated as "clear whatever is
   set" (a W1C-on-write-of-2 behavior) (`irrc_core.c:2222` handler).
   Keep `w: value & 2 -> frame_ready = false` to be safe.
3. **RCA-vs-RC5 auto-detection registers** (`IR_PSR_RC5_RC6`,
   `IR_CTRL_RC6`, `IR_RPT_CTRL_RC5_RC6`) exist but are not used by the
   tv030 path. Stub them as read-0/write-ignore; drivers do not touch
   them when `CONFIG_RTK_KDRV_TV030_IR=y`.
4. **tv030 pulse tables (`IRDA_IR_PSR` bytes) are never programmed** by
   the driver on this firmware; verify by grep before building the
   emulated device: `grep -rn "REG_IR_PSR\b" drivers/rtk_kdriver/ir/`.
   If nothing reads/writes it, QEMU can leave it as read-0.
5. **Power-on IR key** (wake pin at boot stage): the bootloader
   (vendor `rtk_rootfs` `/usr/bin/fti` tools) may query the `misc_off`
   decoder before Linux boots. In Phase 3 we boot straight to Linux, so a
   stub that never fires is fine; if we ever emulate the bootloader,
   revisit `MISC_OFF_IRDA_IR_SR`.
6. Device-tree: the current `dts/rtd284x-tv030.dts` does not yet contain a
   node the Linux 4.4 `irrc` driver binds to: the driver is an
   out-of-tree platform driver matching the name "VenusIR" via legacy probe
   in `irrc_core.c` (grep `platform_driver_register.*Venus` in
   `drivers/rtk_kdriver/ir/`). When writing the QEMU device, no DTS change
   is needed as long as `CONFIG_RTK_KDRV_IR=y` builds the driver in and its
   probe succeeds on MMIO presence alone; confirm what it matches on before
   assuming it boots.
