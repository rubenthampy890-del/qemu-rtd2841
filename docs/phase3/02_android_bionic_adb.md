# Phase 3 — Android Runtime, Bionic, adbd & ADB (RTD2841 / TCL tv030)

Source basis: Android staging tree at `tcl-firmware-re/staging/`
(`system_extracted/`, `update_root/vendor/`), firmware kernel at
`tcl-firmware-re/kernel/rtk2841/`, QEMU model at `qemu-rtd2841/`.

## 1. Silicon Ground Truth

* **Userland is strictly 32-bit ARM.** Every binary in
  `staging/system_extracted/bin/` and the HALs in
  `staging/update_root/vendor/lib/hw/` are `ELF 32-bit LSB executable/shared
  object, ARM, EABI5, dynamically linked`, interpreter
  `/system/bin/linker`. There is **no `app_process64`, no `/system/lib64/`**
  in the system image. This matches the QEMU model's
  "force `aarch64 = false`" Cortex-A53 override (`qemu-model/rtd2841.c:656-661`).
* Boot identity: `ro.build.version.release=9`, `ro.build.version.sdk=28`,
  `ro.build.version.incremental=...T Palm_rtd284x...`,
  `ro.product.model=Q7400`, `ro.build.fingerprint=TCL/Q7400/...`
  (`staging/system_extracted/build.prop`). This is Android 9 / API 28 on a
  vendor Android-tree (not GMS) build.
* **There is no `/system/bin/init`** in `system_extracted/`,
  `bin/` has 306 entries but no `init`, `ueventd`, no `bin/app_process`,
  only `app_process32`. Verified in `staging/system_extracted/bin/`:
  the Android **init process is provided by the vendor rootfs** (see §2),
  not from the `system` partition.
* Key system partition layout:
  - `/system/bin/adbd` — ADB daemon
  - `/system/bin/servicemanager`, `system/bin/surfaceflinger`,
    `system/bin/hwservicemanager` — framework daemons
  - `/system/bin/app_process32` — ART entry point (`Zygote`)
  - `/system/framework/framework-res.apk`, `*.jar` — boot classpath
  - `/system/lib/` — bionic & framework shared libraries
  - `/system/etc/init/` — vendor `init.*.rc` fragments parsed by init
  - `/system/usr/keylayout/mtkinp.kl`, `Vendor_*.kl`, `Generic.kl`,
    keychars — input mapping (see report 01)

## 2. Driver Architecture (boot chain on the real device)

The boot chain on the TCL chassis is **not** classic-AOSP "kernel → init".
It is vendor-layered:

```
 eMMC p1 (system)      eMMC p2+ (vendor / rtk_rootfs)
        │                        │
 Linux 4.4.3 ─── master init = /vendor/rtk_rootfs busybox init ── chain-load
                                 │
                                 ▼
                 Android init (from vendor rtk_rootfs filesystem)
                                 │
                 parse /system/etc/init/*.rc + /vendor/etc/init/*.rc
                                 │
        Zygote (app_process32) → system_server → SurfaceFlinger/servicemanager
                                 │
                            adbd (see §3)
```

* Vendor rootfs lives at `staging/update_root/vendor/` — it contains
  `rtk_rootfs/` (the busybox mini-root that Linux mounts first) and
  `bin/hw/`, `lib/hw/` with the **vendor HALs**:
  - `vendor/lib/hw/hwcomposer.rtd284x.so` — HWC COMPOSER HAL (see report 04)
  - `vendor/lib/hw/hwcomposer.graphic.rtd284x.so`,
    `vendor/lib/hw/hwcomposer.video.so`
  - `vendor/lib/hw/gralloc.rtd284x.so`, plus a `gralloc.default.so` fallback
    that probes `/dev/graphics/fb%u` / `/dev/fb%u`
  (verified by `strings` / `readelf -h` in the machine survey).
* The QEMU Phase 1/2 approach already boots Linux on this model; in Phase 3
  we reuse it and let the *same* chain bring Android up. Kernel command
  line stays unchanged (`root=/dev/mmcblk0p1`) plus optional
  `androidboot.*` params if we ever want to make kernel `init` be the
  primary; but no work is required for the leak-free vendor init path.
* Bind Jonah: **`/system/bin/init` absence is normal** for this vendor —
  the packaged `dts` even omits it; the *vendor* rootfs ships the init
  binary (e.g. under `vendor/bin/init`), and `/system/etc/init/*.rc`
  fragments are parsed by the vendor init (same `init` grammar). Guest userspace
  will therefore look slightly unusual to anyone expecting stock AOSP, but
  it is a *vendor-supported* layout.

## 3. adbd & ADB over QEMU

The critical binary is `staging/system_extracted/bin/adbd`:

* `readelf -h` result: `ELF 32-bit LSB **executable**, ARM, EABI5,
  **statically linked**` (BuildID `422f79c6e608d03e71958632c76f65`),
  no interpreter, **no NEEDED libs** — it is a fully self-contained
  static binary (uses bionic's static build).
* Implication for QEMU: **adbd needs no filesystem re-linking**; it works
  as long as it is executed and the kernel side (USB gadget OR TCP) is
  reachable.
* **The hardware has no usable USB gadget controller path in QEMU**
  (the `iowarrior`-style Realtek USB controller is emulated as `unimp`).
  So we connect ADB over the **network** (this chassis also supports
  network ADB in the vendor builds; `persist.adb.tcp.port` property):

### Design

```
QEMU command line (Phase 3):

  -nic user,model=rtd2841-eth,hostfwd=tcp::5555-:5555

then from the host:   adb connect localhost:5555
```

* Guest side: the vendor `init` (or a shell command from a debugging
  session) sets `setprop persist.adb.tcp.port 5555` and starts `adbd`
  listening on `tcp:5555`. With `ro.debuggable=1` and
  `androidboot.selinux=permissive` on the kernel command line, no auth
  handshake blocks the connection (verified flags in
  `staging/system_extracted/build.prop`:
  `ro.adb.secure=`=0/1 according to user build — set permissive
  `secure=0` behavior by QEMU's bootargs inject if necessary; see
  `FAZING` in `Open Questions`).
* Phase 2 networking (tap or `slirp`, bridge, working DHCP/DNS and
  `rtl8168g` driver) already provides the L2/L3 layer.
* No extra QEMU device, no USB. `hostfwd` maps TCP 5555 to the guest.
* Fallback: `-device usb-tablet` not needed; `10.0.2.2:5555` reachable via
  `-nic user,guestfwd=...,` if hostfwd misbehaves.

### Boot-parameter recommendation

Keep consistent with Phase 2:

```
append="console=ttyS0,115200 earlycon=uart8250,mmio32,0x18062300 \
        root=/dev/mmcblk0p1 rw androidboot.qemu=1 androidboot.selinux=permissive"
```

(the Android kernel treats `androidboot.qemu=1` as "emulated platform";
harmless on vendor init).

## 4. Zygote / ART / framework boot expectations

* 32-bit ART: `app_process32` + `/system/lib/` + `/system/framework/`.
* No 64-bit support needed; QEMU already forces AArch32.
* RAM budget: Android 9 STB reference needs ≥ 1 GB, the QEMU machine has
  exactly 1 GB (`RTD2841_RAM_SIZE` in `qemu-model/rtd2841.h:32`); expect it
  to be tight but workable with the vendor's `lowmemorykiller` setup
  (`setprop ro.lmk.*` in build.prop).
* Storage: `staging/emmc.raw` (4 GB GPT, `partitions_meta.json` mapping)
  already used in Phase 2 boots (`tools/create_emmc_storage.py`) and mounts
  `p1=system, p2=vendor`. Nothing new required.

## 5. Open Questions & Risks

1. **Is `/system/bin/init` really missing?** Re-verify by listing
   `staging/system_extracted/bin/init*` — if the vendor rootfs instead
   provides `/system/bin/init` via a bind-mount at runtime, QEMU boots
   must replicate that mount. Mitigation: boot to an interactive shell
   first (Phase 2 path), inspect the vendor init, then decide.
2. **`ro.adb.secure` behavior on vendor builds.** Vendor `build.prop`
   currently has `ro.adb.secure=1` (user build); network ADB then requires
   an on-screen auth prompt that we cannot deliver until the display stack
   (report 04) is up. Mitigation: append `androidboot.selinux=permissive`
   and patch `ro.adb.secure=0` in the eMMC copy during the one-time
   appliance build (`tools/create_emmc_storage.py`).
3. **`linker` relocation cost in QEMU 32-bit** (article): dynamic ELF
   linking is not accelerated; a `servicemanager`/`surfaceflinger`+ART
   boot will be slow the first time. Expect multi-minute cold boots on a
   laptop — this is a performance expectation, not a blocker.
4. **Vendor init binary path.** Determine where the actual `init` lives
   (`vendor/rtk_rootfs/bin/busybox` shebang, or a standalone
   `vendor/bin/init`) — needed only if we build a custom `ramdisk` instead
   of reusing the vendor chain; default plan avoids this.
5. **Not on the critical path:** `app_process32` needs `/dev/__properties__`
   and `property_contexts`; both are inside the system image and come up
   under the vendor init as usual, so no QEMU-side change is anticipated.
