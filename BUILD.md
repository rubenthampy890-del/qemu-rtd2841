# Build and run Realtek RTD2841 QEMU Model

Follow the numbered steps in [README.md](README.md#1-install-the-host-tools).
They cover macOS and Ubuntu 24.04 dependencies, the pinned QEMU source,
patch application, ARM diagnostic compilation, tests, the display window,
and returning to an existing build.

The public build runs hardware diagnostics without firmware. The
[Android and kernel development section](README.md#android-and-kernel-development)
explains the additional inputs and the current limits of reproducing the
Android research image. A successful hardware test is not an Android boot.
