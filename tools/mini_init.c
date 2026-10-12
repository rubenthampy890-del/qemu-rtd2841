#include "font8x16.h"

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

static inline void* sys_mmap2(void *addr, unsigned long length, unsigned long prot, unsigned long flags, long fd, unsigned long pgoff) {
    register long r7 __asm__("r7") = 192; /* __NR_mmap2 */
    register long r0 __asm__("r0") = (long)addr;
    register long r1 __asm__("r1") = (long)length;
    register long r2 __asm__("r2") = (long)prot;
    register long r3 __asm__("r3") = (long)flags;
    register long r4 __asm__("r4") = (long)fd;
    register long r5 __asm__("r5") = (long)pgoff;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5) : "memory");
    return (void*)r0;
}

static const char banner[] = 
    "\n======================================================\n"
    "[*] TCL RT41 / RTD2841 FIRMWARE TEST SHELL ACTIVATED\n"
    "[*] Target: Cortex-A53 Quad-Core (AArch32) | Linux 4.4.3\n"
    "======================================================\n\n";

static const char msg_mount_proc[]   = "[+] Mounted /proc and /sys filesystems\n";
static const char msg_mount_vendor[] = "[+] Mounted /vendor from /dev/mmcblk0p2 (ext4)\n";
static const char msg_err_vendor[]   = "[-] Failed to mount /vendor from /dev/mmcblk0p2\n";
static const char msg_mount_tmpfs[]  = "[+] Mounted tmpfs on /data, /cache, /tmp, /mnt, /storage\n";
static const char msg_paint_fb[]     = "[+] Framebuffer /dev/fb0 updated: TCL Smart TV Dashboard rendered!\n";
static const char msg_exec_sh[]      = "[+] Dropping into interactive Android root shell (/bin/sh)...\n\n";

static const char str_proc[]        = "proc";
static const char str_sysfs[]       = "sysfs";
static const char str_ext4[]        = "ext4";
static const char str_devpts[]      = "devpts";
static const char str_tmpfs[]       = "tmpfs";

static const char str_proc_path[]   = "/proc";
static const char str_sys_path[]    = "/sys";
static const char str_devpts_path[] = "/dev/pts";
static const char str_vendor_dev[]  = "/dev/mmcblk0p2";
static const char str_vendor_path[] = "/vendor";
static const char str_data_path[]   = "/data";
static const char str_cache_path[]  = "/cache";
static const char str_tmp_path[]    = "/tmp";
static const char str_mnt_path[]    = "/mnt";
static const char str_storage_path[]= "/storage";
static const char str_fb_path[]     = "/dev/fb0";
static const char str_ttyS0[]       = "/dev/ttyS0";

/* Symlinks for Android Treble compatibility */
static const char str_system_etc[]  = "/system/etc";
static const char str_etc[]         = "/etc";
static const char str_vendor_odm[]  = "/vendor/odm";
static const char str_odm[]         = "/odm";
static const char str_system_prod[] = "/system/product";
static const char str_product[]     = "/product";

static const char str_sh[]          = "/bin/sh";
static const char str_path[]        = "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin:/vendor/xbin:/bin";
static const char str_ld[]          = "LD_LIBRARY_PATH=/system/lib:/vendor/lib";
static const char str_root[]        = "ANDROID_ROOT=/system";
static const char str_data[]        = "ANDROID_DATA=/data";
static const char str_storage[]     = "ANDROID_STORAGE=/storage";
static const char str_egl[]         = "ro.hardware.egl=android";
static const char str_gralloc[]     = "ro.hardware.gralloc=default";
static const char str_term[]        = "TERM=vt100";
static const char str_home[]        = "HOME=/";

static char *argv[] = {(char*)str_sh, 0};
static char *envp[] = {
    (char*)str_path,
    (char*)str_ld,
    (char*)str_root,
    (char*)str_data,
    (char*)str_storage,
    (char*)str_egl,
    (char*)str_gralloc,
    (char*)str_term,
    (char*)str_home,
    0
};

#define FB_WIDTH  1920
#define FB_HEIGHT 1080
#define FB_SIZE   (FB_WIDTH * FB_HEIGHT * 4)

#define COLOR_BLACK       0xFF000000
#define COLOR_WHITE       0xFFFFFFFF
#define COLOR_DARK_BG     0xFF10141D
#define COLOR_CARD_BG     0xFF1C2230
#define COLOR_CARD_BORDER 0xFF2D3748
#define COLOR_SEL_BG      0xFF1E3A5F
#define COLOR_SEL_BORDER  0xFF38BDF8
#define COLOR_ACCENT_RED  0xFFE50914
#define COLOR_TEXT_MUTED  0xFF94A3B8
#define COLOR_TEXT_DIM    0xFF64748B
#define COLOR_GREEN       0xFF10B981
#define COLOR_GOLD        0xFFFBBF24

/* 8 MB Canvas in BSS (0 bytes on disk) */
static unsigned int g_canvas[FB_WIDTH * FB_HEIGHT];

static void draw_pixel(int x, int y, unsigned int color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        g_canvas[y * FB_WIDTH + x] = color;
    }
}

static void fill_rect(int x, int y, int w, int h, unsigned int color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > FB_WIDTH)  w = FB_WIDTH - x;
    if (y + h > FB_HEIGHT) h = FB_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int r = y; r < y + h; r++) {
        unsigned int *dst = &g_canvas[r * FB_WIDTH + x];
        for (int c = 0; c < w; c++) {
            dst[c] = color;
        }
    }
}

static void draw_rect_border(int x, int y, int w, int h, int border_w, unsigned int color) {
    fill_rect(x, y, w, border_w, color);
    fill_rect(x, y + h - border_w, w, border_w, color);
    fill_rect(x, y, border_w, h, color);
    fill_rect(x + w - border_w, y, border_w, h, color);
}

static void draw_char(char c, int x, int y, int scale, unsigned int color, unsigned int bg_color) {
    unsigned char uc = (unsigned char)c;
    const unsigned char *glyph = &fontdata_8x16[uc * 16];

    for (int row = 0; row < 16; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col * scale;
            int py = y + row * scale;
            unsigned int pixel_color = (bits & (0x80 >> col)) ? color : bg_color;
            if (pixel_color != 0) {
                if (scale == 1) {
                    draw_pixel(px, py, pixel_color);
                } else {
                    fill_rect(px, py, scale, scale, pixel_color);
                }
            }
        }
    }
}

static void draw_string(const char *str, int x, int y, int scale, unsigned int color, unsigned int bg_color) {
    int cur_x = x;
    while (*str) {
        if (*str == '\n') {
            y += 18 * scale;
            cur_x = x;
        } else {
            draw_char(*str, cur_x, y, scale, color, bg_color);
            cur_x += 8 * scale;
        }
        str++;
    }
}

static void draw_smpte_bars(int y_start, int height) {
    const unsigned int colors[8] = {
        0xFFFFFFFF, 0xFFFFFF00, 0xFF00FFFF, 0xFF00FF00,
        0xFFFF00FF, 0xFFFF0000, 0xFF0000FF, 0xFF000000
    };
    int bar_width = FB_WIDTH / 8;
    for (int i = 0; i < 8; i++) {
        fill_rect(i * bar_width, y_start, bar_width, height, colors[i]);
    }
}

struct CardDef {
    const char *title;
    const char *sub;
    const char *badge;
    unsigned int icon_color;
};

static const struct CardDef tv_cards[6] = {
    {"TV / DTV",     "Channel 01 - 1080p", "LIVE",    0xFFE50914},
    {"HDMI 1",       "Active 60Hz Input",  "INPUT",   0xFF38BDF8},
    {"HDMI 2",       "Standby / No Link",  "INPUT",   0xFF64748B},
    {"Android Apps", "TCL CyberUI Store",  "SMART",   0xFF10B981},
    {"Media Center", "USB / eMMC Storage", "LOCAL",   0xFFF59E0B},
    {"Settings",     "Network / System",   "CONFIG",  0xFF8B5CF6}
};

static void paint_tcl_tv_display(void) {
    long fb = my_syscall3(5 /* __NR_open */, (long)str_fb_path, 2 /* O_RDWR */, 0);
    if (fb < 0) return;

    /* 1. Dark Backdrop */
    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_DARK_BG);

    /* 2. Top Header */
    fill_rect(0, 0, FB_WIDTH, 70, 0xFF0B0E14);
    fill_rect(0, 68, FB_WIDTH, 2, 0xFF1F2937);

    fill_rect(50, 18, 70, 34, COLOR_ACCENT_RED);
    draw_string("TCL", 62, 22, 2, COLOR_WHITE, 0);

    draw_string("AI-IN ANDROID TV", 135, 20, 2, COLOR_WHITE, 0);
    draw_string("CHASSIS RT41 | BOARD tv030 | REALTEK RTD2841", 135, 42, 1, COLOR_TEXT_MUTED, 0);

    draw_string("EMULATOR ACTIVE [QEMU AArch32]", 1360, 26, 1, COLOR_GREEN, 0);
    draw_string("12:00 PM", 1750, 22, 2, COLOR_WHITE, 0);

    /* 3. Hero Section */
    fill_rect(50, 100, FB_WIDTH - 100, 170, 0xFF141923);
    draw_rect_border(50, 100, FB_WIDTH - 100, 170, 1, 0xFF2D3748);

    draw_string("TCL SMART TV FULL-SYSTEM FIRMWARE EMULATION", 80, 120, 2, COLOR_SEL_BORDER, 0);
    draw_string("Target Architecture: Quad-Core ARM Cortex-A53 (AArch32) | GIC-400 | SMP Enabled", 80, 160, 1, COLOR_WHITE, 0);
    draw_string("Linux Kernel: Linux 4.4.3-rtk-qemu | Display Surface: Simple-Framebuffer 1920x1080@32bpp", 80, 180, 1, COLOR_TEXT_MUTED, 0);
    draw_string("Storage Engine: 4.00 GiB eMMC GPT | Partitions: /dev/mmcblk0p1 (system), /dev/mmcblk0p2 (vendor)", 80, 200, 1, COLOR_TEXT_MUTED, 0);
    draw_string("Graphics Pipeline: Software EGL (libGLES_android.so) | Dumb Buffer FB: /dev/fb0", 80, 220, 1, COLOR_GOLD, 0);

    /* 4. Navigation Cards */
    int card_w = 270;
    int card_h = 280;
    int card_spacing = 30;
    int start_x = 55;
    int start_y = 310;

    for (int i = 0; i < 6; i++) {
        int cx = start_x + i * (card_w + card_spacing);
        int is_sel = (i == 0); /* Default to TV / DTV selected */

        unsigned int bg_color = is_sel ? COLOR_SEL_BG : COLOR_CARD_BG;
        unsigned int border_color = is_sel ? COLOR_SEL_BORDER : COLOR_CARD_BORDER;
        int border_thickness = is_sel ? 3 : 1;

        fill_rect(cx, start_y, card_w, card_h, bg_color);
        draw_rect_border(cx, start_y, card_w, card_h, border_thickness, border_color);

        fill_rect(cx + 20, start_y + 20, 60, 60, tv_cards[i].icon_color);
        draw_rect_border(cx + 20, start_y + 20, 60, 60, 1, COLOR_WHITE);

        draw_string(tv_cards[i].badge, cx + card_w - 75, start_y + 20, 1, is_sel ? COLOR_GOLD : COLOR_TEXT_DIM, 0);
        draw_string(tv_cards[i].title, cx + 20, start_y + 110, 2, COLOR_WHITE, 0);
        draw_string(tv_cards[i].sub, cx + 20, start_y + 155, 1, is_sel ? COLOR_SEL_BORDER : COLOR_TEXT_MUTED, 0);

        if (is_sel) {
            fill_rect(cx + 20, start_y + card_h - 45, card_w - 40, 26, COLOR_SEL_BORDER);
            draw_string("ACTIVE INPUT", cx + 70, start_y + card_h - 40, 1, COLOR_BLACK, 0);
        } else {
            draw_string("Press OK to Switch", cx + 20, start_y + card_h - 35, 1, COLOR_TEXT_DIM, 0);
        }
    }

    /* 5. Lower Diagnostics & System Information Panel */
    int info_y = 630;
    fill_rect(50, info_y, FB_WIDTH - 100, 240, 0xFF141923);
    draw_rect_border(50, info_y, FB_WIDTH - 100, 240, 1, 0xFF2D3748);

    draw_string("HARDWARE EMULATION STATUS & INTERFACE MAPPINGS", 80, info_y + 20, 2, COLOR_WHITE, 0);
    draw_string("[+] UART Serial Port:  /dev/ttyS0 active at MMIO 0x1801b200 (115200 8N1) -> Connected to stdio", 80, info_y + 60, 1, COLOR_GREEN, 0);
    draw_string("[+] GIC Interrupts:    GIC-400 v2 | Translation domain mapped (SPI 28 SDHCI, SPI 30 UART)", 80, info_y + 85, 1, COLOR_GREEN, 0);
    draw_string("[+] Display Pipeline:  Simple-Framebuffer MMIO base 0x3e000000 (1920x1080 @ 32bpp ARGB8888)", 80, info_y + 110, 1, COLOR_GREEN, 0);
    draw_string("[+] Storage Gateway:   Realtek generic-sdhci MMIO 0x18010800 -> /dev/mmcblk0 (4.00 GiB GPT)", 80, info_y + 135, 1, COLOR_GREEN, 0);
    draw_string("[+] Userspace Shell:   Bionic /system/bin/sh active as root (UID 0, GID 0)", 80, info_y + 160, 1, COLOR_GREEN, 0);
    draw_string("[+] Remote Control:    Interactive Keypad Helper: scripts/send_key.py / fb_test utility", 80, info_y + 185, 1, COLOR_GOLD, 0);
    draw_string("[*] Note: Cocoa window is video output only. Enter commands via the terminal running QEMU.", 80, info_y + 210, 1, COLOR_TEXT_MUTED, 0);

    /* 6. Control Guidance Footer */
    int foot_y = 900;
    fill_rect(50, foot_y, FB_WIDTH - 100, 60, 0xFF0B0E14);
    draw_rect_border(50, foot_y, FB_WIDTH - 100, 60, 1, 0xFF1F2937);

    draw_string("REMOTE CONTROL KEYS:  [< / >] Select Input    [OK / ENTER] Switch Input    [1-6] Direct Card Jump    [fb_test --loop]", 110, foot_y + 22, 1, COLOR_SEL_BORDER, 0);

    /* 7. SMPTE 8-Color Calibration Bars */
    draw_smpte_bars(990, 70);
    fill_rect(0, 1060, FB_WIDTH, 20, COLOR_BLACK);

    /* 8. Write canvas directly to /dev/fb0 via row writes (guaranteed to flush to MMIO 0x3e000000) */
    for (int r = 0; r < FB_HEIGHT; r++) {
        my_syscall3(4 /* __NR_write */, fb, (long)&g_canvas[r * FB_WIDTH], FB_WIDTH * sizeof(unsigned int));
    }

    my_syscall1(6 /* __NR_close */, fb);
}

void _start(void) {
    /* 1. Establish session and controlling terminal */
    my_syscall0(66 /* __NR_setsid */);
    long tty = my_syscall3(5 /* __NR_open */, (long)str_ttyS0, 2 /* O_RDWR */, 0);
    if (tty >= 0) {
        my_syscall3(54 /* __NR_ioctl */, tty, 0x540E /* TIOCSCTTY */, 0);
        my_syscall2(63 /* __NR_dup2 */, tty, 0);
        my_syscall2(63 /* __NR_dup2 */, tty, 1);
        my_syscall2(63 /* __NR_dup2 */, tty, 2);
    }

    my_syscall3(4 /* __NR_write */, 1, (long)banner, sizeof(banner) - 1);

    /* 2. Mount /proc and /sys */
    my_syscall5(21 /* __NR_mount */, (long)str_proc, (long)str_proc_path, (long)str_proc, 0, 0);
    my_syscall5(21 /* __NR_mount */, (long)str_sysfs, (long)str_sys_path, (long)str_sysfs, 0, 0);
    my_syscall3(4, 1, (long)msg_mount_proc, sizeof(msg_mount_proc) - 1);

    /* 3. Mount /dev/pts */
    my_syscall2(39 /* __NR_mkdir */, (long)str_devpts_path, 0755);
    my_syscall5(21 /* __NR_mount */, (long)str_devpts, (long)str_devpts_path, (long)str_devpts, 0, 0);

    /* 4. Mount /vendor from partition 2 */
    my_syscall2(39 /* __NR_mkdir */, (long)str_vendor_path, 0755);
    long ret = my_syscall5(21 /* __NR_mount */, (long)str_vendor_dev, (long)str_vendor_path, (long)str_ext4, 0, 0);
    if (ret == 0) {
        my_syscall3(4, 1, (long)msg_mount_vendor, sizeof(msg_mount_vendor) - 1);
    } else {
        my_syscall3(4, 1, (long)msg_err_vendor, sizeof(msg_err_vendor) - 1);
    }

    /* 5. Mount tmpfs on /data, /cache, /tmp, /mnt, /storage */
    my_syscall2(39, (long)str_data_path, 0771);
    my_syscall5(21, (long)str_tmpfs, (long)str_data_path, (long)str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_cache_path, 0770);
    my_syscall5(21, (long)str_tmpfs, (long)str_cache_path, (long)str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_tmp_path, 0777);
    my_syscall5(21, (long)str_tmpfs, (long)str_tmp_path, (long)str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_mnt_path, 0755);
    my_syscall5(21, (long)str_tmpfs, (long)str_mnt_path, (long)str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_storage_path, 0755);
    my_syscall5(21, (long)str_tmpfs, (long)str_storage_path, (long)str_tmpfs, 0, 0);

    my_syscall3(4, 1, (long)msg_mount_tmpfs, sizeof(msg_mount_tmpfs) - 1);

    /* 6. Set up Android compatibility symlinks */
    my_syscall2(83 /* __NR_symlink */, (long)str_system_etc, (long)str_etc);
    my_syscall2(83 /* __NR_symlink */, (long)str_vendor_odm, (long)str_odm);
    my_syscall2(83 /* __NR_symlink */, (long)str_system_prod, (long)str_product);

    /* 7. Take over /dev/fb0 with high-definition TCL Smart TV Dashboard */
    paint_tcl_tv_display();
    my_syscall3(4, 1, (long)msg_paint_fb, sizeof(msg_paint_fb) - 1);

    /* 8. Launch interactive root shell */
    my_syscall3(4, 1, (long)msg_exec_sh, sizeof(msg_exec_sh) - 1);
    my_syscall3(11 /* __NR_execve */, (long)str_sh, (long)argv, (long)envp);

    while (1) {
        my_syscall3(29 /* __NR_pause */, 0, 0, 0);
    }
}
