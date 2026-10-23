# Phase 3 — Audio Subsystem (RTD2841 / TCL tv030)

> [!NOTE]
> **Exploratory Architecture Specification**: This document contains research and register analysis from vendor drivers for planned future work. The Audio DSP, AIO, and ADMA hardware peripherals described below are **unimplemented** in the current QEMU machine model (see [HARDWARE_FIDELITY.md](../../HARDWARE_FIDELITY.md)).

Source basis: firmware kernel at `tcl-firmware-re/kernel/rtk2841/`
(Linux 4.4.3), QEMU model at `qemu-rtd2841/`.

## 1. Silicon Ground Truth

The RTD2841 audio stack has **three decoupled pieces of silicon**:

| Block | Base | HW role |
|---|---|---|
| **AIO (Audio I/O)** + global ctrl | `0x18006700` family | hw process-control regs written by SCPU driver; hardware "ALC/FEQ" control cluster |
| **ADMA** (Audio DMA) | `0x18006600` family | descriptor/buffer DMA engine; **owned by the co-CPU at runtime**, kernel only touches it briefly to quiesce it |
| **Audio co-CPU (RTK8051 / "AVCPU")** | DRAM-mapped at `0x82200000`, status at `0x82210000` | firmware that actually programs the AIO/ADMA registers and consumes the parameter ring |

Register map (verified in
`drivers/rtk_kdriver/include/rbus/audio_reg.h`):

| Name | Offset | Function |
|---|---|---|
| `AUDIO_HW_Process_Control_reg` | `0x18006700` (bus `0xB8006700`) | SCPU→hardware process-control: write `0x1` executes "process"; read `0x00010001` = "hw process callback ack OK" (audio_reg.h:5269-5273) |
| `AUDIO_global_ctrl_0_reg` | `0x18006000` | global AIO switches (power domains) |
| `AUDIO_global_int_en_reg` | `0x18006038` | interrupt enable |
| `AUDIO_INTCTL_reg` | `0x1800603C` | interrupt status w/ `hw_nafg_int` bit |
| `AUDIO_ADMA_CSR_reg` | `0x18006600` | ADMA CSR: `dma_wr_src_sel(30)`, `dma_output_src_sel(28)`, `dma_w_format(27)`, `dma_st_adj(26)` etc. (audio_reg.h:4529+) |
| `AUDIO_ADMA_WCONF_reg` | `0x18006604` | write-side config: channel num, buffer count |
| `AUDIO_ADMA_WSADR0/1_reg` | `0x18006608` / `+0x4` | write SADR[0..1] |
| `AUDIO_ADMA_DRVISR_reg` | `0x18006610` | "driver" status: `rnum(9:0)`, `dsrc(6:4)`, `wnum(3:0)` |
| `AUDIO_ADMA_RVCTR/INT_VCTR` | `0x18006618` / `+0x4` | RD ptr / int vector |
| `AUDIO_ADMA_RCONF_reg` | `0x18006628` | read-side config: `blk_sz(30:16)`, `wnv_reg(12:8)`, `w_loop(4)`, `w_upd_buf(2)` |
| `AUDIO_ADMA_WRCTR_reg` | `0x1800662C` | read-pointer count |
| `AUDIO_ADMA_RSADR0..5_reg` | `0x18006630` … `0x18006644` | read SADR for output reg 0..5 (I2S out, SPDIF, ARC, ...) |
| `AUDIO_ARC_ADMA_CSR_reg` | `0x18006680` | ARC-specific DMA |
| `AUDIO_TSADMA_CSR_reg` | `0x18006A00` | TS-A/V demux-side DMA (media pipeline) |

### Co-CPU contract ("AVCPU" module)

`rtk_avcpu.h:12-37` / `rtk_avcpu.c:25`:

* Firmware binary loaded by kernel from **eMMC header offset `0x2000000`**
  (the "AVCPU partition" header), target **DRAM address `0x82200000`**.
* Status register `0x82210000` must eventually equal
  **`RTK8051_READY_MAGIC = 0xEDEDEDED`** (bit 0 = AV CPU alive,
  bit 2 = algorithm engine alive) — `rtk_avcpu.c:rtk_avcpu_ready()`.
* Entry `0x82200000 + 0x0`, spin loop `0x82210000`; algorithm RT memory at
  DRAM `0x01000000`.
* ioctls (`rtk_avcpu.h:102-105`): `AVCPU_SUSPEND=100`,
  `AVCPU_RESUME=101`, `AVCPU_RESET_PREPARE=102`, `AVCPU_RESET_DONE=103`.

## 2. Driver Architecture

```
 audio/snd-soc (ALSA ASoC) ── optional RTL ALSA layer (rtkaudio_driver.c,
                                audio/ subdir) - exposed as /dev/snd/*
                    │ ioctl(unused in runtime path)
                    ▼
 audio_hw/audio_hw_app.c  (8796 lines)  ← the *manager*: orchestrates
                    │                     PCM, DTS, AC3 passthrough
                    │                     via Audio_Hwp*Register()
                    ▼
 audio_hw/audio_hw_driver.c (336 lines) — thin layer translating calls
                    │                     into writes of
                    │                     AUDIO_HW_Process_Control_reg
                    ▼
 audio_hw/audio_hw_rpc.c (930 lines)    ← RPC channel to the co-CPU
                    │                     manages RBUF_HEADER ring
                    ▼
 RPCDriver.c (639 lines)                 SCPU↔AVCPU RPC transport
                    │                    (ioctl on /dev/rpc_poll or
                    │                     mail)
                    ▼
 RTK8051 firmware  (runs its own loop, programs ADMA itself)
```

Facts about the pieces:

* **Kernel never programs the ADMA in the runtime path.** The only
  direct register writes are *quiesce* operations in
  `audio_hw/audio_hw_factory.c:384-390` (`rtdf_outl(AUDIO_TSADMA_CSR_reg, 0)`
  and `rtdf_outl(AUDIO_ADMA_CSR_reg, 0x30)`) — i.e., stop the TS DMA and
  put ADMA in a legacy state. All programming (`WSADR`, `RCONF`, channel
  count, sample rate) is done by the **RTK8051 firmware running at
  `0x82200000`**, which lives outside the kernel.
* Intended DMA is a ring of **6 buffers** (`ADMA_WCONF.num_out_reg = 6`);
  the driver never fills one itself — the co-CPU does, pulling from the
  RBUF ring below.
* **RPC channel** (`RPCDriver.h:clients` and `audio_hw_rpc.c`):
  - Poll ring at physical `0x1B1F1000` (AudioIntrWrite),
    `0x1B1F1800` for Video1, KCPU `0x1B1E0000` (RPCDriver.h:107-110).
  - `RPC_STRUCT` is a fixed **28-byte network-byte-order header**
    (RPCDriver.h:22-58), followed by a variable-length arg buffer.
  - Every ring command to audio passes through
    `send_krpc(RPC_AUDIO, ENUM_KERNEL_RPC_*, …)` — audio-specific commands
    listed in `audio_hw_rpc.c:462-488`:
    `ENUM_KERNEL_RPC_CREATE_AGENT / INIT_RINGBUF / PRIVATEINFO / RUN /
     PAUSE / MALLOC_ADDR / FLUSH / DESTROY / STOP / CHECK_READY / EOS`.
  - The payload includes the **RBUF_HEADER**:
    `struct rbuf_header { magic (0x4D415243 "MARC"); begin_addr; size;
       buf_id; wr_ptr; num_rd_ptr; rd_ptr[]; file_offset; }`
    (audio_hw_rpc.c) — i.e. the kernel *hands the co-CPU a pointer to a
    physical PCM ring buffer*, and the co-CPU walks `wr_ptr`/`rd_ptr`
    itself.
* The driver detects "does the co-CPU exist" by **polling the status
  register** for the `0xEDEDEDED` magic before it ever issues an RPC
  (`rtk_avcpu.c:rtk_avcpu_ready`, `audio_hw_rpc.c` init paths).
* `rtkaudio_driver.c` (2221 lines, `drivers/audio/`) is an *additional*
  chardev that exports `alsa_info` ioctls (card 0 = the vendor userspace
  lib wants a *number*, not actual kernel ALSA). In this build the actual
  sound path is vendor-lib → hw driver → RPC; **the Linux ALSA ASoC
  layer is not bound** (no DAI/codec nodes in the DTS).

## 3. Register Map (Runtime contract relevant to QEMU)

The only registers the SCPU Linux side touching in normal audio play:

| Register | Set to | When |
|---|---|---|
| `0x18006700` (HWP `Process_Control`) | `0x1` then poll `0x00010001` | every "process" command (also used by many `audio_hw_*` fns) |
| `0x18006704` (`ACPU_INFO`) | status queries co-CPU | init |
| `0x18006744` (`ACPU_REG_HW_INT_CTRL`) | enable hw int | init |
| `0x18006600` (ADMA CSR) | `0x30` | only at *factory reset / error recovery* (audio_hw_factory.c:384-390) |
| RPC poll ring `0x1B1F1000` (`AudioIntrWrite`) | 28-byte RPC_STRUCT | every `send_krpc` |
| DRAM `0x82210000` | must read `0xEDEDEDED` | boot (verifies co-CPU exists) |

## 4. QEMU Integration Design

Phase 3 goal: keep boot and Android UI working. **We do not need real
sound** for debugging/UI; we *do* need the driver probes not to hang.

### Strategy A — cooperative "silent" co-CPU stub (recommended, smallest)

Implement QEMU memory of DRAM segment `0x822000xx` + a no-op MMIO register
block at the ADMA + AIO bases with "write reads ignored" behavior, and pre-seed
the status:

* Seed DRAM `0x82210000 = 0xEDEDEDED` at machine init → every
  `rtk_avcpu_ready()` call returns "co-CPU alive", every load of the audio
  FW (memory_map `0x2000000`) is treated as already-executing (binary
  already there, nothing actually runs).
* Make the **RPC ring** work with a trivial QEMU "AVCPU shim": when the
  kernel writes a 28-byte RPC_STRUCT into the poll ring and then spins
  waiting for a completion, the shim can either
  1. do nothing (if the driver only requires `CHECK_READY` to be queued)
     — the *in-ring* polling thread might block but will time out, or
  2. more useful: parse `ENUM_KERNEL_RPC_CHECK_READY` and write back an
    ASCII-success record so the driver proceeds to set up the ADMA and
    then simply never receives any further completions (sound "plays" to
    an emulated sink). Optionally wire the ADMA `WSADR` ring into QEMU's
    ALSA yield (`-audiodev coreaudio` or `sdl`) later.
* Optional extension: `qemu-model/rtd2841_io.c` stubs for the registers
  listed in §3 (RCONF/WSADR are just storage; never fire real interrupts).

**Why silent co-CPU is enough:** sound usage on an Android TV means
"UI clicks and media" — as long as the driver *thinks* it is running,
boot/login works and no regression to the Phase-2 network stack occurs.

### Strategy B — skip audio entirely

QEMU only seeds `0x82210000 = 0xEDEDEDED` and returns 0 from all audio
MMIO. If the driver still probes fine (we must verify by running the
image), zero code needed beyond 16 bytes of memory-seed. This is the
Phase 3 default: a **one-line patch** to the machine realize path.

### Out of scope (defer to Phase 4+)

* Real PCM output to the host (need to buffer RBUF, feed `-audiodev`).
* Video-decoder / TS pipeline (`AUDIO_TSADMA_*`, DVR Demux) — that was
  Part 2's `VDEC_MEM` carveout at `0x10000000` and the ARM-TSG block
  (`qemu-model/rtd2841.h:98-99`), considered dead for QEMU media playback.

## 5. Open Questions & Risks

1. **Does the driver actually block or merely warn when the co-CPU does
   not exist?** If `audio_hw_rpc.c` *returns success* with just the status
   magic, Strategy B is a 1-line patch; if it *blocks* on
   `0xEDEDEDED`, a **device model** implementing at least the poll ring
   handshake (Strategy A) is required. **Decide empirically** — this is
   the first experiment to run when audio is try-integrated.
2. **RPC poll thread vs interrupt thread.** `RPCDriver.c` has both
   `rpc_poll_thread` and `rpc_intr_thread`; the shipped config selects
   "poll mode" (verify in the boot log — it prints `RPC_POLL` once). If
   that is true, no GIC line needs to be modeled, which makes the QEMU
   stub easier.
3. **eMMC audio firmware partition.** The AVCPU binary lives at header
   offset `0x2000000` (16 MB into the disk, inside `p3`?). Confirm with
   `tools/create_emmc_storage.py` and cut it out; QEMU can leave those
   blocks as zeros since no real co-CPU loads it (Strategy A/B).
4. **`rtkaudio_driver.c` ioctls** may be called by vendor lib
   (`librtk_audio.so` userspace). If the *userspace* HAL probes for a
   device node that our stub does not emulated, HAL registration fails
   gracefully (Android continues without an audio HAL) — acceptable for
   Phase 3.
5. **`/dev/rpc` char devices** (`RPC_POLL_DEV`, `RPC_INTR`) at major 193
   are created by `RPCDriver.c` module init — they need *no* QEMU support
   because the kernel module owns them; only the *shared DRAM ring* and
   the status magic need the stub.
