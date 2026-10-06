static inline long my_syscall0(long n) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0");
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7) : "memory");
    return r0;
}

static inline long my_syscall1(long n, long a1) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0) : "memory");
    return r0;
}

static inline long my_syscall2(long n, long a1, long a2) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1) : "memory");
    return r0;
}

static inline long my_syscall3(long n, long a1, long a2, long a3) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline long my_syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    register long r3 __asm__("r3") = a4;
    register long r4 __asm__("r4") = a5;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4) : "memory");
    return r0;
}

static const char banner[] = 
    "\n======================================================\n"
    "[*] TCL RT41 / RTD2841 FIRMWARE TEST SHELL ACTIVATED\n"
    "[*] Target: Cortex-A53 Quad-Core (AArch32) | Linux 4.4.3\n"
    "======================================================\n\n";

static const char msg_mount_proc[] = "[+] Mounted /proc and /sys filesystem\n";
static const char msg_mount_vendor[] = "[+] Mounted /vendor from /dev/mmcblk0p2 (ext4)\n";
static const char msg_err_vendor[] = "[-] Failed to mount /vendor\n";
static const char msg_paint_fb[] = "[+] Framebuffer /dev/fb0 updated: SMPTE 1080p color bars rendered!\n";
static const char msg_exec_sh[] = "[+] Dropping into interactive Android root shell (/bin/sh)...\n\n";

static const char str_proc[] = "proc";
static const char str_sysfs[] = "sysfs";
static const char str_ext4[] = "ext4";
static const char str_devpts[] = "devpts";

static const char str_proc_path[] = "/proc";
static const char str_sys_path[] = "/sys";
static const char str_vendor_dev[] = "/dev/mmcblk0p2";
static const char str_vendor_path[] = "/vendor";
static const char str_devpts_path[] = "/dev/pts";
static const char str_fb_path[] = "/dev/fb0";
static const char str_ttyS0[] = "/dev/ttyS0";

static const char str_sh[] = "/bin/sh";
static const char str_path[] = "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin:/vendor/xbin:/bin";
static const char str_ld[] = "LD_LIBRARY_PATH=/system/lib:/vendor/lib";
static const char str_term[] = "TERM=vt100";
static const char str_home[] = "HOME=/";

static char *argv[] = {(char*)str_sh, 0};
static char *envp[] = {(char*)str_path, (char*)str_ld, (char*)str_term, (char*)str_home, 0};

static unsigned int row_buf[1920];

void _start(void) {
    // 1. Establish session and controlling terminal
    my_syscall0(66 /* __NR_setsid */);
    long tty = my_syscall3(5 /* __NR_open */, (long)str_ttyS0, 2 /* O_RDWR */, 0);
    if (tty >= 0) {
        my_syscall3(54 /* __NR_ioctl */, tty, 0x540E /* TIOCSCTTY */, 0);
        my_syscall2(63 /* __NR_dup2 */, tty, 0);
        my_syscall2(63 /* __NR_dup2 */, tty, 1);
        my_syscall2(63 /* __NR_dup2 */, tty, 2);
    }

    my_syscall3(4 /* __NR_write */, 1, (long)banner, sizeof(banner) - 1);

    // 2. Mount /proc and /sys
    my_syscall5(21 /* __NR_mount */, (long)str_proc, (long)str_proc_path, (long)str_proc, 0, 0);
    my_syscall5(21 /* __NR_mount */, (long)str_sysfs, (long)str_sys_path, (long)str_sysfs, 0, 0);
    my_syscall3(4, 1, (long)msg_mount_proc, sizeof(msg_mount_proc) - 1);

    // 3. Mount /dev/pts
    my_syscall2(39 /* __NR_mkdir */, (long)str_devpts_path, 0755);
    my_syscall5(21 /* __NR_mount */, (long)str_devpts, (long)str_devpts_path, (long)str_devpts, 0, 0);

    // 4. Mount /vendor from partition 2
    my_syscall2(39 /* __NR_mkdir */, (long)str_vendor_path, 0755);
    long ret = my_syscall5(21 /* __NR_mount */, (long)str_vendor_dev, (long)str_vendor_path, (long)str_ext4, 0, 0);
    if (ret == 0) {
        my_syscall3(4, 1, (long)msg_mount_vendor, sizeof(msg_mount_vendor) - 1);
    } else {
        my_syscall3(4, 1, (long)msg_err_vendor, sizeof(msg_err_vendor) - 1);
    }

    // 5. Paint 1080p SMPTE test pattern to /dev/fb0
    long fb = my_syscall3(5 /* __NR_open */, (long)str_fb_path, 2 /* O_RDWR */, 0);
    if (fb >= 0) {
        const unsigned int colors[8] = {
            0xFFFFFFFF, // White
            0xFFFFFF00, // Yellow
            0xFF00FFFF, // Cyan
            0xFF00FF00, // Green
            0xFFFF00FF, // Magenta
            0xFFFF0000, // Red
            0xFF0000FF, // Blue
            0xFF000000  // Black
        };
        for (int col = 0; col < 1920; col++) {
            row_buf[col] = colors[col / 240];
        }
        for (int r = 0; r < 1080; r++) {
            my_syscall3(4 /* __NR_write */, fb, (long)row_buf, 1920 * sizeof(unsigned int));
        }
        my_syscall1(6 /* __NR_close */, fb);
        my_syscall3(4, 1, (long)msg_paint_fb, sizeof(msg_paint_fb) - 1);
    }

    // 6. Launch interactive root shell
    my_syscall3(4, 1, (long)msg_exec_sh, sizeof(msg_exec_sh) - 1);
    my_syscall3(11 /* __NR_execve */, (long)str_sh, (long)argv, (long)envp);

    while (1) {
        my_syscall3(29 /* __NR_pause */, 0, 0, 0);
    }
}
