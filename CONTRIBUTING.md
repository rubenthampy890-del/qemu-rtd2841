# Contributing to qemu-rtd2841

Developer guidelines, coding conventions, and test requirements for `qemu-rtd2841`.

---

## 1. Engineering Doctrine & Code Style

Code submitted to this repository must follow upstream QEMU conventions and defensive error checking:

1. **Comment Style**: Use C-style block comments (`/* ... */`) only. Do not use C99/C++ line comments (`//`) in QEMU model C code.
2. **Column Width**: Enforce a strict 80-column line wrap.
3. **Checked Syscalls & Returns**:
   - Always inspect return codes of syscalls, I/O operations, and memory allocations.
   - Never emit success status indicators (`[+] ...`) unless return codes explicitly equal `0` or valid non-negative descriptors.
4. **Target Architecture & ABI**:
   - Target ABI for userspace binaries is ARM32 EABI (AArch32).
   - Reference `arch/arm/include/uapi/asm/unistd.h` for syscall numbers (e.g. Syscall 114 is `__NR_wait4`; Syscall 7 is `sys_restart_syscall`).
5. **Hardware Model Transparency**:
   - Do not create silent register stubs returning 0 unless documented in `HARDWARE_FIDELITY.md`.
   - Use `qemu_log_mask(LOG_UNIMP, ...)` for unmodeled peripheral apertures.
6. **Bounds & Modulo Safety**:
   - Circular descriptor rings (Ethernet, Audio, DMA) must enforce strict modulo indexing `(idx + 1) % NUM_DESC`.
   - Clamp all incoming DMA/network packet lengths to descriptor buffer capacities before copying to physical RAM.

---

## 2. Setting Up the Development Environment

### Prerequisites
- **macOS**: `brew install ninja meson pkg-config glib pixman dtc python3 gcc-arm-embedded`
- **Linux (Ubuntu/Debian)**: `sudo apt install -y build-essential git ninja-build meson pkg-config libglib2.0-dev libpixman-1-dev device-tree-compiler python3 gcc-arm-none-eabi binutils-arm-none-eabi`

### Building the Model
```bash
# 1. Clone QEMU at pinned baseline
git clone https://gitlab.com/qemu-project/qemu.git
cd qemu
git checkout a19f59a72d96056d9a8f48cf6c77f5787491e414

# 2. Apply patch
git apply /path/to/qemu-rtd2841/patches/qemu-rtd2841-complete.patch

# 3. Configure & build
mkdir build && cd build
../configure --target-list=aarch64-softmmu --disable-werror
ninja qemu-system-aarch64
```

---

## 3. Running Automated Tests

Before submitting any changes, execute the unified test suite:

```bash
# Rebuild diagnostic test binaries from assembly source
make -C tests clean
make -C tests

# Execute multi-suite test harness
python3 tests/run_ci_test.py
```

All 3 verification suites must pass:
1. Core SoC Diagnostics (CPU, GIC, TC0, SDHCI, RTL8168 MMIO)
2. SDHCI eMMC Storage Media Insertion
3. Linux 4.4 Kernel Boot with RTL8168 Network & Outbound Internet Verification

---

## 4. Proprietary Firmware & Legal Boundaries

- **Never commit proprietary vendor firmware, TrustZone blobs, keys, or copyrighted ROM images** to this repository.
- Use only public open-source toolchains, clean assembly tests, or synthetic test payloads.
- Refer to firmware by SHA256 checksums, version strings, or chassis identifiers (`tv030`).

---

## 5. Submitting Changes

1. Create an isolated feature branch: `git checkout -b feature/my-improvement`.
2. Keep commits atomic with conventional commit prefixes: `feat:`, `fix:`, `docs:`, `chore:`, `tests:`.
3. Submit a Pull Request targeting `main`. Ensure all GitHub Actions CI checks pass.
