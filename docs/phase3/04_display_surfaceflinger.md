# Phase 3 — Display Pipeline & SurfaceFlinger (RTD2841 / TCL tv030)

> [!NOTE]
> **Exploratory Architecture Specification**: This document contains research and HAL interface analysis for planned future work. SurfaceFlinger integration and hardware composer acceleration described below are **unimplemented** in the current repository (see [HARDWARE_FIDELITY.md](../../HARDWARE_FIDELITY.md)).

Source basis: firmware kernel at `tcl-firmware-re/kernel/rtk2841/`, Android
staging at `tcl-firmware-re/staging/`, QEMU model at `qemu-rtd2841/`.

## 1. Silicon Ground Truth

### What the RTD2841 has in hardware

* A **video decode + scalers + GDI composition** pipeline (VDEC/DI/Scalers,
  RTK GDI mixer), feeding an HDMI out — none of which we emulate.
* An ARM **Mali-style GPU** aperture at `0x18040000` (stubbed as
  `unimp` in `qemu-model/rtd2841.c:789-790`).
* A **frame buffer carveout** at DRAM `0x3E000000` (8 MB,
  `RTD2841_FB_BASE/SIZE` in `qemu-model/rtd2841.h:102-104`), pre-arranged
  1920×1080 × 32 bpp (`RTD2841_FB_STRIDE = 7680`, line `106`).

### What the guest kernel actually uses on this firmware

* The tv030 DTS declares a **`simple-framebuffer` node** at
  `dts/rtd284x-tv030.dts` (declared "okay" — see
  `qemu-rtd2841/dts`). Linux binds the generic `simplefb framebuffer`
  driver to it, yielding a single `/dev/fb0` (created as
  `/dev/graphics/fb0` by the vendor `ueventd` ACL — verified path used by
  `staging/update_root/vendor/lib/hw/gralloc.default.so`, whose strings
  include `/dev/graphics/fb%u` and `/dev/fb%u`).
* VDEC/scaler output is not part of the boot path — the vendor
  `hwcomposer.video.so` only wraps the decode HAL for **video-on-top**
  composition; SurfaceFlinger can boot and composite the UI without it
  (drawing via `hwcomposer.graphic.rtd284x.so` against the frame buffer).

## 2. Driver Architecture

```
 Applications (Android: Launcher / HAL callbacks)  ─┐
                                                    ▼
 SurfaceFlinger (/system/bin/surfaceflinger)    ~ composition
     │  uses gralloc1/gralloc0 → gralloc.rtd284x.so (or gralloc.default.so)
     │  uses HWC 2 → hwcomposer.rtd284x.so  (falls back to GPU composition)
     ▼
 vendor/lib/hw/hwcomposer.graphic.rtd284x.so  ◄── drawing engine wrapper
     ▼
 /dev/graphics/fb0  (simplefb: memory-mapped RAM)
     ▼
 QEMU console (frame buffer RAM update → window surface)
```

Key facts from the HAL binaries found in the firmware tree:

* `staging/update_root/vendor/lib/hw/` contains the real HALs:
  `gralloc.rtd284x.so`, `gralloc.default.so`, `hwcomposer.rtd284x.so`,
  `hwcomposer.graphic.rtd284x.so`, `hwcomposer.video.so` — all
  `ELF 32-bit ARM` (matches §1.1 in report 02). No HALs live under
  `system_extracted/`, so the vendor image path (`/vendor`) must remain
  mounted, which Phase 2 already does (`p2=vendor` mount in the boot flow).
* The `gralloc.default.so` AOSP-compat code paths expect either
  `/dev/graphics/fb%u` or `/dev/fb%u` (its embedded strings). The vendor
  `gralloc.rtd284x.so` is the one needing RTK ion/cma buffers — if that
  fails to find its kernel-side ion device it falls back; the boot log
  must be checked to see which gralloc actually loads
  (`gralloc.rtd284x` is the one we expect).
* `hwcomposer.rtd284x.so` is the *presentable* composer HAL. If Android's
  HWC fails to attach (no VPU present), SurfaceFlinger falls back to the
  vendor's *client composition* (i.e. into `gralloc.default` against fb0).
  For Phase 3, **client composition is what we want** because it means
  everything happens on `fb0` (the memory QEMU watches).

## 3. Register Map & Memory Layout (the part QEMU cares about)

| Item | Address | Size | Notes |
|---|---|---|---|
| Frame buffer RAM | `0x3E000000` | 8 MB | QEMU `memory_region_init_ram("rtd2841.fb")` (`qemu-model/rtd2841.c:929-934`) |
| Display pixel format | — | — | `PIXMAN_x8r8g8b8` (argb32); stride 1920×4 = 7680 (`qemu-model/rtd2841.h:106`) |
| Console | — | — | `graphic_console_init` at init (`qemu-model/rtd2841.c:936`); initialized to opaque navy `0xFF141824` so the screen is visibly non-black pre-boot (`qemu-model/rtd2841.c:937-950`) |
| Simplefb kernel node | — | — | declared in `dts/rtd284x-tv030.dts`, bound by `simplefb` → `/dev/fb0` |
| GPU aperture | `0x18040000` | 16 KB | stubbed |
| VDEC carveout | `0x10000000` | 96 MB | stubbed (report 03 & Phase 1 note) |

Because the surface is *RAM-backed*, a guest that simply writes pixels
into `0x3E000000` shows up without QEMU knowing anything about "frame
registers" — that is how Phase 1 boots produce a picture. A modern
SurfaceFlinger writing through gralloc buffers however **does** memcpy into
frame-buffer RAM assigned by `gralloc.default` (or the vendor one) —
which ultimately aliases `0x3E000000` (the fb `screen_base` points here),
so *no additional QEMU emulation is needed for basic UI compositing*.

## 4. QEMU Integration Design

What Phase 3 needs, in priority order:

1. **Bind a real key/pixel path** — done: `rtd2841_fb_update` /
   `rtd2841_fb_invalidate` (`qemu-model/rtd2841.c:807-842`) give us
   full-screen 30 Hz repainting. This is enough for gralloc *software*
   composition. No new code.
2. **Better redraw granularity (optional)**. Full-frame repaint every
   vblank is heavy (1920×1080×4 = 8 MB per frame). For productivity:
   - set `display.update.freq` to ~10 Hz initially;
   - or implement dirty-bit tracking via `memory_region_add_subregion`
     watcher. *Not required for correctness*, so defer.
3. **SurfaceFlinger HAL selection** — nothing to do: the system picks
   `hwcomposer.rtd284x.so` (present) but the vendor's *layer* is backed by
   an opaque design we cannot emulate. If HWC queries the real VPU and
   fails, SurfaceFlinger will degrade to client composition through
   `gralloc.default.so` (AOSP-compat path probing `/dev/graphics/fb%u`).
   Since the vendor builds were originally tested with real VDEC, watch
   for `gralloc` logs (grep `gralloc` in boot log) — as long as *some*
   gralloc loads, SurfaceFlinger comes up.
4. **Fallback for no-HWC**: since HWC 2.x is the vendor ABI (Android 9),
   if the vendor HWC fails to register SurfaceFlinger logs
   `using client composition` and renders into a gralloc buffer that is
   memcpy'd to fb0. Screen content should be visible.
5. **Test & watch** — commands to capture while observing the boot:
   - `display surface` / `info display` (QMP/HMP)
   - `dump-guest-memory` to confirm the guest wrote into `0x3E000000`
   - Android: `service call SurfaceFlinger 1035` toggles `HWC service`
     debug output.

### Known breaks and their fixes

| Break | Fix |
|---|---|
| SurfaceFlinger refuses to start with no `gralloc.rtd284x` ion fd | Force `gralloc.default.so` fallback by setting `ro.hardware.gralloc=default` in `system.build.prop` |
| Vendor HWC locks up the boot look for RTK scaler ioctls | Same fallback (`ro.hardware.hwcomposer=default`), needs a quick QEMU test-boot |
| 1080p repaint heavy | Drop DTS/QEMU fb to 1280×720 (QEMU model constants `RTD2841_FB_WIDTH/HEIGHT` and the rtd2841 DTS fb node) to halve traffic; irrelevant for correctness |

## 5. Open Questions & Risks

1. **Does the vendor `hwcomposer.rtd284x.so` work at all in a
   grayscale-free software composition pipeline?** Unknown. **Toggle it
   away** (`ro.hardware.hwcomposer=default`) if boot hangs at
   `Starting surfaceflinger`; keep the RTK one if it works.
2. **Ion/CMA lack**: `gralloc.rtd284x.so` expects `/dev/ion`. This device
   is ordinary in Android kernel configs, but there is *no ion node* in
   the tv030 DTS we boot. Confirm via `staging/system_extracted/ueventd*
   .rc` + `ls /dev/ion` once booted. If absent, fall back so gralloc
   uses `/dev/mem` or private CMA allocations.
3. **Mali stub**: the GPU aperture is `unimp`, so any HWC path calling
   GL will hang. The default fallback path never calls GL. Confirm no
   HWC code path touches `/dev/mali*` / `libGLES_mali.so` on this
   vendor build.
4. **Surface flinger color depth**: the surface is managed as
   `x8r8g8b8`; a greyscale or RGB565 guest frame buffer (as simplefb
   publishes by DTS setting) will not auto-convert — ensure the DTS says
   32-bit (`display '<D>'` and `bpp` field in the DTS node, confirm
   `dts/rtd284x-tv030.dts` is correct).
5. **Benchmark note**: full-screen repaint is currently
   `dpy_gfx_update(console, 0, 0, 1920, 1080)` per vblank
   (`qemu-model/rtd2841.c:835`) — expensive but acceptable at Phase 3
   quality; do **not** optimize before booting Android UI successfully.
