---
name: Bug Report
about: Create a report to help us improve the RTD2841 emulator
title: '[BUG] '
labels: bug
assignees: ''
---

## Description
A clear and concise description of the bug.

## Environment & Toolchain
- **Host OS**: (e.g. macOS 14.5 Sonoma / Ubuntu 22.04 LTS / Debian 12)
- **Repository Commit**: (e.g. `git rev-parse HEAD`)
- **Upstream QEMU Revision**: (e.g. `a19f59a72d96` or custom)
- **Cross-Compiler Version**: (`arm-none-eabi-gcc --version`)

## Reproduction Command
Exact command used to launch QEMU:
```bash
./build/qemu-system-aarch64 -M rtd2841-tv030 ...
```

## Firmware & Payload Details
- **Test Payload / Image**: (e.g. `tests/firmware_test.bin` / custom kernel)
- **SHA256 Hash**:
```text
sha256sum <file>
```

## Serial Console Log & Observed Output
```text
(Paste full serial console output or attach log file)
```

## Expected Behavior
A clear description of what was expected to happen.
