#include "font8x16.h"

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

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

static inline long my_syscall4(long n, long a1, long a2, long a3, long a4) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    register long r3 __asm__("r3") = a4;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2), "r"(r3) : "memory");
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

static inline long my_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    register long r3 __asm__("r3") = a4;
    register long r4 __asm__("r4") = a5;
    register long r5 __asm__("r5") = a6;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5) : "memory");
    return r0;
}

static inline unsigned long my_strlen(const char *s) {
    unsigned long len = 0;
    while (s[len]) len++;
    return len;
}

static inline void print_str(const char *s) {
    my_syscall3(4 /* __NR_write */, 1, (long)s, my_strlen(s));
}

static inline void *my_memset(void *s, int c, unsigned long n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

static inline void *my_memcpy(void *dest, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

static inline void print_dec(long val) {
    char buf[32];
    int i = 0;
    unsigned long uval;
    if (val < 0) {
        print_str("-");
        uval = (unsigned long)(-val);
    } else {
        uval = (unsigned long)val;
    }
    if (uval == 0) {
        buf[i++] = '0';
    } else {
        char temp[32];
        int ti = 0;
        while (uval > 0) {
            temp[ti++] = '0' + (char)(uval % 10);
            uval /= 10;
        }
        while (ti > 0) {
            buf[i++] = temp[--ti];
        }
    }
    buf[i] = '\0';
    print_str(buf);
}

static inline void print_hex(unsigned long val) {
    char buf[16];
    static const char hexchars[] = "0123456789abcdef";
    int i = 0;
    buf[i++] = '0';
    buf[i++] = 'x';
    for (int shift = 28; shift >= 0; shift -= 4) {
        buf[i++] = hexchars[(val >> shift) & 0xf];
    }
    buf[i] = '\0';
    print_str(buf);
}

static inline void print_ip(unsigned int ip) {
    print_dec(ip & 0xFF);
    print_str(".");
    print_dec((ip >> 8) & 0xFF);
    print_str(".");
    print_dec((ip >> 16) & 0xFF);
    print_str(".");
    print_dec((ip >> 24) & 0xFF);
}

static inline void my_memory_barrier(void) {
    __asm__ volatile(".arch armv7-a\n\tdmb ish" ::: "memory");
}

static inline const char *my_strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return s;
        s++;
    }
    return 0;
}

static inline int my_strncmp(const char *s1, const char *s2, unsigned long n) {
    for (unsigned long i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (int)(unsigned char)s1[i] - (int)(unsigned char)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

static inline int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

#ifndef WIFEXITED
#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(s)  (((s) & 0xff00) >> 8)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(s)  (((s) & 0x7f) > 0 && (((s) & 0x7f) < 0x7f))
#endif
#ifndef WTERMSIG
#define WTERMSIG(s)     ((s) & 0x7f)
#endif

static int check_mount(const char *source, const char *target, const char *fstype, unsigned long flags, const void *data) {
    long rc = my_syscall5(21 /* __NR_mount */, (long)source, (long)target, (long)fstype, (long)flags, (long)data);
    if (rc != 0) {
        print_str("[-] FAILED mount: ");
        print_str(target);
        print_str(", rc=");
        print_dec(rc);
        print_str("\n");
        return (int)rc;
    }
    return 0;
}

struct my_timeval {
    long tv_sec;
    long tv_usec;
};

struct my_sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    unsigned int   sin_addr;
    char           sin_zero[8];
};

struct my_ifreq {
    char ifr_name[16];
    union {
        struct my_sockaddr_in ifru_addr;
        short ifru_flags;
    };
};

struct my_rtentry {
    unsigned long rt_pad1;
    struct my_sockaddr_in rt_dst;
    struct my_sockaddr_in rt_gateway;
    struct my_sockaddr_in rt_genmask;
    unsigned short rt_flags;
    short rt_pad2;
    unsigned long rt_pad3;
    void *rt_pad4;
    short rt_metric;
    const char *rt_dev;
    unsigned long rt_mtu;
    unsigned long rt_window;
    unsigned short rt_irtt;
};

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
static const char msg_mount_data[]   = "[+] Mounted /data from /dev/mmcblk0p3 (ext4)\n";
static const char msg_mount_tmpfs[]  = "[+] Mounted tmpfs on /cache, /tmp, /mnt, /storage\n";
static const char msg_paint_fb[]     = "[+] Framebuffer /dev/fb0 updated: TCL Smart TV Dashboard rendered!\n";
static const char msg_exec_sh[]      = "[+] Dropping into interactive Linux root shell (/bin/sh)...\n\n";

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
static const char str_data_dev[]    = "/dev/mmcblk0p3";
static const char str_data_path[]   = "/data";
static const char str_cache_path[]  = "/cache";
static const char str_tmp_path[]    = "/tmp";
static const char str_mnt_path[]    = "/mnt";
static const char str_storage_path[]= "/storage";
static const char str_fb_path[]     = "/dev/fb0";
static const char str_ttyS0[]       = "/dev/ttyS0";
static const char str_binder[]            = "/dev/binder";
static const char str_hwbinder[]          = "/dev/hwbinder";
static const char str_vndbinder[]         = "/dev/vndbinder";
static const char str_urandom[]           = "/dev/urandom";
static const char str_servicemanager[]    = "/system/bin/servicemanager";
static const char str_hwservicemanager[]   = "/system/bin/hwservicemanager";
static const char str_vndservicemanager[]  = "/vendor/bin/vndservicemanager";
static const char str_xbin_path[]         = "/system/xbin";
static const char str_su_path[]           = "/system/xbin/su";
static const char str_misc_path[]         = "/data/misc";
static const char str_adb_path[]          = "/data/misc/adb";
static const char str_local_path[]        = "/data/local";
static const char str_tmp_local_path[]    = "/data/local/tmp";

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
static const char str_hwc[]         = "ro.hardware.hwcomposer=default";
static const char str_term[]        = "TERM=vt100";
static const char str_home[]        = "HOME=/";

static char *argv[] = {(char*)str_sh, 0};
static char *envp[] = {
    (char*)str_path,
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
    {"Ethernet",     "10.0.2.15 (Online)", "NET",     0xFF10B981}
};

static void paint_tcl_tv_display(void) {
    long fb = my_syscall3(5 /* __NR_open */, (long)str_fb_path, 2 /* O_RDWR */, 0);
    if (fb < 0) {
        print_str("[-] FAILED: open /dev/fb0, rc=");
        print_dec(fb);
        print_str("\n");
        return;
    }

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
    draw_string("[+] UART Serial Port:  /dev/ttyS0 active at MMIO 0x18062300 (115200 8N1) -> Connected to stdio", 80, info_y + 55, 1, COLOR_GREEN, 0);
    draw_string("[+] GIC Interrupts:    GIC-400 v2 | Translation domain mapped (SPI 18 SDHCI, SPI 20 ETH, SPI 25 UART)", 80, info_y + 80, 1, COLOR_GREEN, 0);
    draw_string("[+] Network Interface: Realtek RTL8168GU MMIO 0x18016000 (SPI 20) -> eth0 (10.0.2.15/24, GW 10.0.2.2)", 80, info_y + 105, 1, COLOR_GREEN, 0);
    draw_string("[+] Display Pipeline:  Simple-Framebuffer MMIO base 0x3e000000 (1920x1080 @ 32bpp ARGB8888)", 80, info_y + 130, 1, COLOR_GREEN, 0);
    draw_string("[+] Storage Gateway:   Realtek generic-sdhci MMIO 0x18010800 -> /dev/mmcblk0 (4.00 GiB GPT)", 80, info_y + 155, 1, COLOR_GREEN, 0);
    draw_string("[+] Userspace Shell:   Bionic /system/bin/sh active as root (UID 0, GID 0)", 80, info_y + 180, 1, COLOR_GREEN, 0);
    draw_string("[+] Remote Control:    Interactive Keypad Helper: scripts/send_key.py / fb_test utility", 80, info_y + 205, 1, COLOR_GOLD, 0);
    draw_string("[*] Note: Cocoa window is video output only. Enter commands via the terminal running QEMU.", 80, info_y + 210, 1, COLOR_TEXT_MUTED, 0);

    /* 6. Control Guidance Footer */
    int foot_y = 900;
    fill_rect(50, foot_y, FB_WIDTH - 100, 60, 0xFF0B0E14);
    draw_rect_border(50, foot_y, FB_WIDTH - 100, 60, 1, 0xFF1F2937);

    draw_string("REMOTE CONTROL KEYS:  [< / >] Select Input    [OK / ENTER] Switch Input    [1-6] Direct Card Jump    [fb_test --loop]", 110, foot_y + 22, 1, COLOR_SEL_BORDER, 0);

    /* 7. SMPTE 8-Color Calibration Bars */
    draw_smpte_bars(990, 70);
    fill_rect(0, 1060, FB_WIDTH, 20, COLOR_BLACK);

    /* 8. Map canvas directly to /dev/fb0 via mmap2 (guaranteed to flush to MMIO 0x3e000000) */
    long frame_bytes = FB_WIDTH * FB_HEIGHT * sizeof(unsigned int);
    void *fb_mem = (void *)my_syscall6(192 /* __NR_mmap2 */, 0, frame_bytes, 3 /* PROT_READ|PROT_WRITE */, 1 /* MAP_SHARED */, fb, 0);
    if ((long)fb_mem < 0 && (long)fb_mem > -4096) {
        print_str("[-] FAILED: mmap /dev/fb0, rc=");
        print_dec((long)fb_mem);
        print_str(", falling back to row writes\n");
        long total_written = 0;
        for (int r = 0; r < FB_HEIGHT; r++) {
            long wrc = my_syscall3(4 /* __NR_write */, fb, (long)&g_canvas[r * FB_WIDTH], FB_WIDTH * sizeof(unsigned int));
            if (wrc > 0) total_written += wrc;
        }
        print_str("[+] Boot splash written to /dev/fb0, bytes=");
        print_dec(total_written);
        print_str("\n");
    } else {
        my_memcpy(fb_mem, g_canvas, frame_bytes);
        my_memory_barrier();
        my_syscall2(91 /* __NR_munmap */, (long)fb_mem, frame_bytes);
        print_str("[+] Boot splash display mapped and copied to /dev/fb0 (8294400 bytes)\n");
    }

    my_syscall1(6 /* __NR_close */, fb);
}

static int run_process(const char *cmd, char *const proc_argv[], int wait) {
    long pid = my_syscall0(2 /* __NR_fork */);
    if (pid < 0) {
        print_str("[-] FAILED: fork failed for ");
        print_str(cmd);
        print_str(", rc=");
        print_dec(pid);
        print_str("\n");
        return (int)pid;
    }
    if (pid == 0) {
        for (int fd = 3; fd < 1024; fd++) {
            my_syscall1(6 /* __NR_close */, fd);
        }
        long exec_rc = my_syscall3(11 /* __NR_execve */, (long)cmd,
                                   (long)proc_argv, (long)envp);
        print_str("[-] FAILED: execve failed for ");
        print_str(cmd);
        print_str(", rc=");
        print_dec(exec_rc);
        print_str("\n");
        my_syscall1(1 /* __NR_exit */, 127);
    }
    if (pid > 0 && wait) {
        int status = 0;
        long ret;
        do {
            ret = my_syscall4(114 /* __NR_wait4 */, pid, (long)&status, 0, 0);
        } while (ret == -4 /* -EINTR */);
        if (ret < 0) {
            print_str("[-] FAILED: wait4 failed for ");
            print_str(cmd);
            print_str(", rc=");
            print_dec(ret);
            print_str("\n");
            return (int)ret;
        }
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code != 0) {
                print_str("[-] Process ");
                print_str(cmd);
                print_str(" exited with status ");
                print_dec(code);
                print_str("\n");
            }
            return code;
        }
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            print_str("[-] Process ");
            print_str(cmd);
            print_str(" killed by signal ");
            print_dec(sig);
            print_str("\n");
            return -1;
        }
        return -1;
    }
    return (int)pid;
}

__attribute__((noreturn)) static void exit_sm_child(int status) {
    my_syscall1(1 /* __NR_exit */, status);
    for (;;) { }
}

#define PROP_AREA_MAGIC   0x504f5250
#define PROP_AREA_VERSION 0xfc6ed0ab
#define PA_SIZE           131072

struct prop_area_hdr {
    unsigned int bytes_used;
    unsigned int serial;
    unsigned int magic;
    unsigned int version;
    unsigned int reserved[28];
};

struct prop_bt {
    unsigned int namelen;
    unsigned int prop;
    unsigned int left;
    unsigned int right;
    unsigned int children;
    char name[0];
};

struct prop_info {
    unsigned int serial;
    char value[92];
    char name[0];
};

struct prop_msg {
    unsigned int cmd;
    char name[32];
    char value[92];
};

struct my_sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};

static void prop_area_init(void *area) {
    my_memset(area, 0, PA_SIZE);
    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    hdr->bytes_used = 20; /* root_bt occupies bytes 0..19 of data area */
    hdr->serial = 0;
    hdr->magic = PROP_AREA_MAGIC;
    hdr->version = PROP_AREA_VERSION;
}

static unsigned int prop_area_alloc(void *area, unsigned int size) {
    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    unsigned int aligned = (size + 3) & ~3;
    if (hdr->bytes_used + aligned > PA_SIZE - 128) {
        return 0;
    }
    unsigned int off = hdr->bytes_used;
    hdr->bytes_used += aligned;
    return off;
}

static int prop_area_set(void *area, const char *key, const char *val) {
    if (my_strlen(val) >= 92) return -1;

    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    char *data = (char *)area + 128;
    unsigned int current_bt_off = 0;

    const char *p = key;
    while (*p) {
        const char *dot = my_strchr(p, '.');
        unsigned int comp_len = dot ? (unsigned int)(dot - p) : (unsigned int)my_strlen(p);
        if (comp_len == 0) return -1;

        struct prop_bt *curr = (struct prop_bt *)(data + current_bt_off);
        unsigned int *target_link = 0;

        if (curr->children == 0) {
            unsigned int node_sz = sizeof(struct prop_bt) + comp_len + 1;
            unsigned int new_off = prop_area_alloc(area, node_sz);
            if (!new_off) return -1;
            struct prop_bt *new_node = (struct prop_bt *)(data + new_off);
            my_memset(new_node, 0, (node_sz + 3) & ~3);
            new_node->namelen = comp_len;
            my_memcpy(new_node->name, p, comp_len);
            new_node->name[comp_len] = '\0';
            curr->children = new_off;
            current_bt_off = new_off;
        } else {
            unsigned int child_off = curr->children;
            while (1) {
                struct prop_bt *child = (struct prop_bt *)(data + child_off);
                int cmp = 0;
                if (comp_len < child->namelen) {
                    cmp = -1;
                } else if (comp_len > child->namelen) {
                    cmp = 1;
                } else {
                    cmp = my_strncmp(p, child->name, comp_len);
                }

                if (cmp < 0) {
                    if (child->left == 0) {
                        target_link = &child->left;
                        break;
                    }
                    child_off = child->left;
                } else if (cmp > 0) {
                    if (child->right == 0) {
                        target_link = &child->right;
                        break;
                    }
                    child_off = child->right;
                } else {
                    current_bt_off = child_off;
                    target_link = 0;
                    break;
                }
            }

            if (target_link) {
                unsigned int node_sz = sizeof(struct prop_bt) + comp_len + 1;
                unsigned int new_off = prop_area_alloc(area, node_sz);
                if (!new_off) return -1;
                struct prop_bt *new_node = (struct prop_bt *)(data + new_off);
                my_memset(new_node, 0, (node_sz + 3) & ~3);
                new_node->namelen = comp_len;
                my_memcpy(new_node->name, p, comp_len);
                new_node->name[comp_len] = '\0';
                *target_link = new_off;
                current_bt_off = new_off;
            }
        }

        if (dot) {
            p = dot + 1;
        } else {
            break;
        }
    }

    struct prop_bt *leaf = (struct prop_bt *)(data + current_bt_off);
    unsigned int vallen = (unsigned int)my_strlen(val);
    struct prop_info *info;

    if (leaf->prop == 0) {
        unsigned int keylen = (unsigned int)my_strlen(key);
        unsigned int info_sz = 96 + keylen + 1;
        unsigned int info_off = prop_area_alloc(area, info_sz);
        if (!info_off) return -1;
        info = (struct prop_info *)(data + info_off);
        my_memset(info, 0, (info_sz + 3) & ~3);
        info->serial = (vallen << 24);
        my_memcpy(info->value, val, vallen);
        info->value[vallen] = '\0';
        my_memcpy(info->name, key, keylen);
        info->name[keylen] = '\0';
        leaf->prop = info_off;
    } else {
        /* Read-only properties (ro.*) cannot be overwritten once set */
        if (my_strncmp(key, "ro.", 3) == 0) {
            return -1;
        }
        info = (struct prop_info *)(data + leaf->prop);
        unsigned int old_serial = info->serial;
        info->serial |= 1;
        my_memory_barrier();
        my_memcpy(info->value, val, vallen);
        info->value[vallen] = '\0';
        my_memory_barrier();
        info->serial = (vallen << 24) | ((old_serial + 2) & 0x00ffffff & ~1);
    }

    hdr->serial++;
    long r_futex1 = my_syscall6(240 /* __NR_futex */, (long)&info->serial, 1 /* FUTEX_WAKE */, 0x7fffffff /* INT_MAX */, 0, 0, 0);
    long r_futex2 = my_syscall6(240 /* __NR_futex */, (long)&hdr->serial, 1 /* FUTEX_WAKE */, 0x7fffffff /* INT_MAX */, 0, 0, 0);
    (void)r_futex1;
    (void)r_futex2;
    return 0;
}

static const char *prop_area_get(void *area, const char *key) {
    char *data = (char *)area + 128;
    unsigned int current_bt_off = 0; /* root_bt */

    const char *p = key;
    while (*p) {
        const char *dot = my_strchr(p, '.');
        unsigned int comp_len = dot ? (unsigned int)(dot - p) : (unsigned int)my_strlen(p);
        if (comp_len == 0) return 0;

        struct prop_bt *curr = (struct prop_bt *)(data + current_bt_off);
        if (curr->children == 0) return 0;

        unsigned int child_off = curr->children;
        int found = 0;
        while (child_off != 0) {
            struct prop_bt *child = (struct prop_bt *)(data + child_off);
            int cmp = 0;
            if (comp_len < child->namelen) {
                cmp = -1;
            } else if (comp_len > child->namelen) {
                cmp = 1;
            } else {
                cmp = my_strncmp(p, child->name, comp_len);
            }

            if (cmp < 0) {
                child_off = child->left;
            } else if (cmp > 0) {
                child_off = child->right;
            } else {
                current_bt_off = child_off;
                found = 1;
                break;
            }
        }

        if (!found) return 0;

        if (dot) {
            p = dot + 1;
        } else {
            break;
        }
    }

    struct prop_bt *leaf = (struct prop_bt *)(data + current_bt_off);
    if (leaf->prop == 0) return 0;
    struct prop_info *info = (struct prop_info *)(data + leaf->prop);
    return info->value;
}

static void load_prop_file(void *area, const char *path) {
    long fd = my_syscall3(5 /* __NR_open */, (long)path, 0 /* O_RDONLY */, 0);
    if (fd < 0) return;
    static char buf[32768];
    long r = my_syscall3(3 /* __NR_read */, fd, (long)buf, sizeof(buf) - 1);
    my_syscall1(6 /* __NR_close */, fd);
    if (r <= 0) return;
    buf[r] = '\0';

    char *line = buf;
    while (*line) {
        while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
        if (!*line) break;
        if (*line == '#') {
            while (*line && *line != '\n') line++;
            continue;
        }
        char *eq = 0;
        char *end = line;
        while (*end && *end != '\r' && *end != '\n') {
            if (*end == '=' && !eq) eq = end;
            end++;
        }
        char term = *end;
        if (eq && eq > line) {
            *eq = '\0';
            char *val = eq + 1;
            *end = '\0';
            prop_area_set(area, line, val);
        }
        line = term ? (end + 1) : end;
    }
}

static int is_android_mode(void);

static void *init_properties_area(void) {
    static const char str_prop_file[] = "/dev/__properties__";
    my_syscall1(10 /* __NR_unlink */, (long)str_prop_file);

    long pfd = my_syscall3(5 /* __NR_open */, (long)str_prop_file,
                           0102 /* O_RDWR | O_CREAT */, 0644);
    if (pfd < 0) {
        print_str("[-] FAILED: open /dev/__properties__, rc=");
        print_dec(pfd);
        print_str("\n");
        return 0;
    }

    long r_trunc = my_syscall2(93 /* __NR_ftruncate */, pfd, PA_SIZE);
    if (r_trunc != 0) {
        /* Fallback: write PA_SIZE zero bytes */
        char zbuf[4096];
        my_memset(zbuf, 0, sizeof(zbuf));
        for (int i = 0; i < 32; i++) {
            long wrc = my_syscall3(4 /* __NR_write */, pfd, (long)zbuf, sizeof(zbuf));
            if (wrc != sizeof(zbuf)) {
                print_str("[-] FAILED: write /dev/__properties__, rc=");
                print_dec(wrc);
                print_str("\n");
                my_syscall1(6 /* __NR_close */, pfd);
                return 0;
            }
        }
    }

    void *area = sys_mmap2(0, PA_SIZE, 3 /* PROT_READ|PROT_WRITE */,
                           1 /* MAP_SHARED */, pfd, 0);
    my_syscall1(6 /* __NR_close */, pfd);

    if (area == (void *)-1 || (unsigned long)area >= (unsigned long)-4095) {
        print_str("[-] FAILED: mmap /dev/__properties__, rc=");
        print_hex((unsigned long)area);
        print_str("\n");
        return 0;
    }

    prop_area_init(area);

    /* Mandatory ro.property_service.version=2 for Android Bionic writev */
    prop_area_set(area, "ro.property_service.version", "2");

    /* Core platform properties */
    prop_area_set(area, "ro.build.version.sdk", "28");
    prop_area_set(area, "ro.build.version.release", "9");
    prop_area_set(area, "ro.build.version.codename", "REL");
    prop_area_set(area, "ro.build.version.all_codenames", "REL");
    prop_area_set(area, "ro.build.version.min_supported_target_sdk", "17");
    prop_area_set(area, "ro.build.version.security_patch", "2020-12-01");
    prop_area_set(area, "dalvik.vm.heapgrowthlimit", "128m");
    prop_area_set(area, "dalvik.vm.heapsize", "256m");
    prop_area_set(area, "dalvik.vm.heaptargetutilization", "0.75");
    prop_area_set(area, "dalvik.vm.heapminfree", "512k");
    prop_area_set(area, "dalvik.vm.heapmaxfree", "8m");
    prop_area_set(area, "ro.sf.lcd_density", "240");
    prop_area_set(area, "ro.hardware.egl", "android");
    prop_area_set(area, "ro.hardware.gralloc", "default");
    prop_area_set(area, "ro.hardware.hwcomposer", "default");
    prop_area_set(area, "service.adb.tcp.port", "5555");
    prop_area_set(area, "ro.kernel.qemu", "0");
    prop_area_set(area, "ro.debuggable", "1");
    prop_area_set(area, "ro.secure", "0");
    prop_area_set(area, "ro.adb.secure", "0");
    prop_area_set(area, "persist.sys.usb.config", "adb");
    prop_area_set(area, "ro.build.type", "eng");
    prop_area_set(area, "ro.treble.enabled", "true");
    prop_area_set(area, "ro.vndk.lite", "true");
    prop_area_set(area, "persist.sys.dalvik.vm.lib.2", "libart.so");
    prop_area_set(area, "ro.product.model", "UnionTV");
    prop_area_set(area, "ro.product.brand", "TCL");
    prop_area_set(area, "ro.product.name", "UnionTV");
    prop_area_set(area, "ro.product.device", "UnionTV");
    prop_area_set(area, "ro.product.cpu.abi", "armeabi-v7a");
    prop_area_set(area, "ro.product.cpu.abilist", "armeabi-v7a,armeabi");
    prop_area_set(area, "ro.product.cpu.abilist32", "armeabi-v7a,armeabi");
    prop_area_set(area, "ro.zygote", "zygote32");
    prop_area_set(area, "dalvik.vm.boot-image", "/system/framework/arm/boot.art");
    prop_area_set(area, "dalvik.vm.usejit", "true");
    prop_area_set(area, "dalvik.vm.isa.arm.variant", "cortex-a7");
    prop_area_set(area, "dalvik.vm.isa.arm.features", "default");
    prop_area_set(area, "vold.has_adoptable", "0");

    /* Android 9 keeps system default properties here, including the package
     * manager's required pm.dexopt filters. Load genuine firmware values.
     */
    load_prop_file(area, "/system/etc/prop.default");
    /* Load system and vendor overrides if present. */
    load_prop_file(area, "/default.prop");
    load_prop_file(area, "/build.prop");
    load_prop_file(area, "/vendor/default.prop");
    load_prop_file(area, "/system/build.prop");
    load_prop_file(area, "/vendor/build.prop");

    static const char *dexopt_keys[] = {
        "pm.dexopt.first-boot", "pm.dexopt.boot", "pm.dexopt.install",
        "pm.dexopt.bg-dexopt", "pm.dexopt.ab-ota", "pm.dexopt.inactive",
        "pm.dexopt.shared"
    };
    for (unsigned int i = 0; i < sizeof(dexopt_keys) / sizeof(dexopt_keys[0]); i++) {
        const char *value = prop_area_get(area, dexopt_keys[i]);
        if ((!value || !*value) && is_android_mode()) {
            print_str("[-] Required firmware property missing: ");
            print_str(dexopt_keys[i]);
            print_str("; check /system/etc/prop.default\n");
            return 0;
        }
    }

    /* Ensure ro.property_service.version remains 2 and ro.adb.secure remains 0 */
    prop_area_set(area, "ro.property_service.version", "2");
    prop_area_set(area, "ro.adb.secure", "0");
    prop_area_set(area, "ro.boot.selinux", "permissive");

    my_syscall3(212 /* __NR_chown32 */, (long)str_prop_file, 0, 0);
    long r_chmod = my_syscall2(15 /* __NR_chmod */, (long)str_prop_file, 0444);
    if (r_chmod != 0) {
        print_str("[-] FAILED: chmod /dev/__properties__, rc=");
        print_dec(r_chmod);
        print_str("\n");
    }

    const char *ver = prop_area_get(area, "ro.property_service.version");
    if (ver && my_strcmp(ver, "2") == 0) {
        print_str("[+] Property area initialized at /dev/__properties__ (128 KB trie, ro.property_service.version=2)\n");
    }
    return area;
}

static long my_read_all(long fd, void *buf, unsigned long count) {
    unsigned char *p = (unsigned char *)buf;
    unsigned long total = 0;
    while (total < count) {
        long r = my_syscall3(3 /* __NR_read */, fd, (long)(p + total), count - total);
        if (r < 0) {
            if (r == -4 /* -EINTR */) continue;
            return r;
        }
        if (r == 0) break;
        total += (unsigned long)r;
    }
    return (long)total;
}

static int start_property_service(void *prop_area) {
    static const char str_sock_dir[] = "/dev/socket";
    static const char str_prop_sock[] = "/dev/socket/property_service";

    long r_mkdir = my_syscall2(39 /* __NR_mkdir */, (long)str_sock_dir, 0755);
    if (r_mkdir != 0 && r_mkdir != -17 /* -EEXIST */) {
        print_str("[-] FAILED: mkdir /dev/socket, rc=");
        print_dec(r_mkdir);
        print_str("\n");
        return (int)r_mkdir;
    }
    my_syscall2(15 /* __NR_chmod */, (long)str_sock_dir, 0755);
    my_syscall1(10 /* __NR_unlink */, (long)str_prop_sock);

    long sfd = my_syscall3(281 /* __NR_socket */, 1 /* AF_UNIX */, 1 /* SOCK_STREAM */, 0);
    if (sfd < 0) {
        print_str("[-] FAILED: socket property_service, rc=");
        print_dec(sfd);
        print_str("\n");
        return (int)sfd;
    }

    struct my_sockaddr_un sun;
    my_memset(&sun, 0, sizeof(sun));
    sun.sun_family = 1; /* AF_UNIX */
    my_memcpy(sun.sun_path, str_prop_sock, sizeof(str_prop_sock));

    long rc = my_syscall3(282 /* __NR_bind */, sfd, (long)&sun, sizeof(sun));
    if (rc != 0) {
        print_str("[-] FAILED: bind property_service, rc=");
        print_dec(rc);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sfd);
        return (int)rc;
    }

    rc = my_syscall2(15 /* __NR_chmod */, (long)str_prop_sock, 0666);
    if (rc != 0) {
        print_str("[-] FAILED: chmod property_service, rc=");
        print_dec(rc);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sfd);
        return (int)rc;
    }

    rc = my_syscall2(284 /* __NR_listen */, sfd, 8);
    if (rc != 0) {
        print_str("[-] FAILED: listen property_service, rc=");
        print_dec(rc);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sfd);
        return (int)rc;
    }

    print_str("[+] Property service socket listening on /dev/socket/property_service\n");

    long pid = my_syscall0(2 /* __NR_fork */);
    if (pid < 0) {
        print_str("[-] FAILED: fork property_service, rc=");
        print_dec(pid);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sfd);
        return (int)pid;
    }
    if (pid == 0) {
        while (1) {
            long cfd = my_syscall3(285 /* __NR_accept */, sfd, 0, 0);
            if (cfd < 0) {
                if (cfd == -4 /* -EINTR */) continue;
                continue;
            }

            struct my_timeval tv = { .tv_sec = 2, .tv_usec = 0 };
            my_syscall5(294 /* __NR_setsockopt */, cfd, 1 /* SOL_SOCKET */,
                        20 /* SO_RCVTIMEO */, (long)&tv, sizeof(tv));
            my_syscall5(294 /* __NR_setsockopt */, cfd, 1 /* SOL_SOCKET */,
                        21 /* SO_SNDTIMEO */, (long)&tv, sizeof(tv));

            uint32_t cmd = 0;
            long r = my_read_all(cfd, &cmd, 4);
            if (r == 4) {
                if (cmd == 0x00020001) { /* PROP_MSG_SETPROP2 (writev, version 2) */
                    uint32_t namelen = 0;
                    if (my_read_all(cfd, &namelen, 4) == 4 && namelen > 0 && namelen < 512) {
                        char name[512];
                        if (my_read_all(cfd, name, namelen) == (long)namelen) {
                            name[namelen] = '\0';
                            uint32_t vallen = 0;
                            char val[512];
                            val[0] = '\0';
                            if (my_read_all(cfd, &vallen, 4) == 4) {
                                if (vallen >= 92) {
                                    uint32_t resp = 1; /* error */
                                    my_syscall3(4 /* __NR_write */, cfd, (long)&resp, 4);
                                    my_syscall1(6 /* __NR_close */, cfd);
                                    continue;
                                }
                                if (vallen > 0) {
                                    if (my_read_all(cfd, val, vallen) != (long)vallen) {
                                        my_syscall1(6 /* __NR_close */, cfd);
                                        continue;
                                    }
                                }
                                val[vallen] = '\0';
                                int set_rc = prop_area_set(prop_area, name, val);
                                uint32_t resp = (set_rc == 0) ? 0 : 1; /* 0 = PROP_SUCCESS */
                                my_syscall3(4 /* __NR_write */, cfd, (long)&resp, 4);
                            }
                        }
                    }
                } else if (cmd == 1) { /* PROP_MSG_SETPROP (legacy 128 bytes) */
                    char buf[124];
                    if (my_read_all(cfd, buf, 124) == 124) {
                        char name[33];
                        char val[93];
                        my_memcpy(name, buf, 32);
                        name[32] = '\0';
                        my_memcpy(val, buf + 32, 92);
                        val[92] = '\0';
                        int set_rc = prop_area_set(prop_area, name, val);
                        uint32_t resp = (set_rc == 0) ? 0 : 1;
                        my_syscall3(4 /* __NR_write */, cfd, (long)&resp, 4);
                    }
                }
            }
            my_syscall1(6 /* __NR_close */, cfd);
        }
        my_syscall1(1 /* __NR_exit */, 0);
    }

    my_syscall1(6 /* __NR_close */, sfd);
    return (int)pid;
}

static int setup_binder_node(const char *path, int minor) {
    long rc = my_syscall2(15 /* __NR_chmod */, (long)path, 0666);
    if (rc != 0) {
        int dev = (10 << 8) | (minor & 0xff);
        rc = my_syscall3(14 /* __NR_mknod */, (long)path, 0020666 /* S_IFCHR | 0666 */, dev);
        if (rc != 0 && rc != -17 /* -EEXIST */) {
            print_str("[-] FAILED: mknod ");
            print_str(path);
            print_str(", rc=");
            print_dec(rc);
            print_str("\n");
            return (int)rc;
        }
        rc = my_syscall2(15 /* __NR_chmod */, (long)path, 0666);
        if (rc != 0) {
            print_str("[-] FAILED: chmod ");
            print_str(path);
            print_str(", rc=");
            print_dec(rc);
            print_str("\n");
            return (int)rc;
        }
    }
    return 0;
}

static int spawn_service_manager(const char *name, const char *cmd, char *const cmd_argv[]) {
    unsigned int readproc_gid = 3009;
    long rc = my_syscall0(2 /* __NR_fork */);
    if (rc < 0) {
        print_str("[-] FAILED: fork ");
        print_str(name);
        print_str(", rc=");
        print_dec(rc);
        print_str("\n");
        return (int)rc;
    }
    if (rc == 0) {
        for (int fd = 3; fd < 1024; fd++) {
            my_syscall1(6 /* __NR_close */, fd);
        }
        long child_rc;

        child_rc = my_syscall2(206 /* __NR_setgroups32 */, 1, (long)&readproc_gid);
        if (child_rc != 0) {
            print_str("[-] FAILED: ");
            print_str(name);
            print_str(" setgroups32, rc=");
            print_dec(child_rc);
            print_str("\n");
            exit_sm_child(125);
        }
        child_rc = my_syscall3(210 /* __NR_setresgid32 */, 1000, 1000, 1000);
        if (child_rc != 0) {
            print_str("[-] FAILED: ");
            print_str(name);
            print_str(" setresgid32, rc=");
            print_dec(child_rc);
            print_str("\n");
            exit_sm_child(125);
        }
        child_rc = my_syscall3(208 /* __NR_setresuid32 */, 1000, 1000, 1000);
        if (child_rc != 0) {
            print_str("[-] FAILED: ");
            print_str(name);
            print_str(" setresuid32, rc=");
            print_dec(child_rc);
            print_str("\n");
            exit_sm_child(125);
        }
        child_rc = my_syscall3(11 /* __NR_execve */, (long)cmd, (long)cmd_argv, (long)envp);
        print_str("[-] FAILED: ");
        print_str(name);
        print_str(" execve, rc=");
        print_dec(child_rc);
        print_str("\n");
        exit_sm_child(127);
    }
    return (int)rc;
}

/* Stock vendor rc files specify system:system for these bootstrap HALs.
 * Power additionally uses group wakelock (3010) and CAP_BLOCK_SUSPEND (36).
 * Check identity/capability setup before exec; never fall back to root.
 */
static int spawn_bootstrap_hal(const char *cmd, int power_capability) {
    long pid = my_syscall0(2 /* fork */);
    long rc = pid;
    const char *operation = "fork";
    if (pid < 0) goto failed;
    if (pid > 0) {
        print_str("[*] HAL child spawned: ");
        print_str(cmd);
        print_str(", pid=");
        print_dec(pid);
        print_str("; registration requires a HwBinder probe\n");
        return (int)pid;
    }
    for (int fd = 3; fd < 1024; fd++) {
        operation = "close inherited descriptor";
        rc = my_syscall1(6 /* close */, fd);
        if (rc != 0 && rc != -9 /* EBADF */) goto failed;
    }
    unsigned int groups[] = {1000, 3010};
    operation = "setgroups32";
    rc = my_syscall2(206, power_capability ? 2 : 1, (long)groups);
    if (rc != 0) goto failed;
    operation = "setresgid32";
    rc = my_syscall3(210, 1000, 1000, 1000);
    if (rc != 0) goto failed;
    if (power_capability) {
        operation = "PR_SET_KEEPCAPS";
        rc = my_syscall5(172 /* prctl */, 8, 1, 0, 0, 0);
        if (rc != 0) goto failed;
    }
    operation = "setresuid32";
    rc = my_syscall3(208, 1000, 1000, 1000);
    if (rc != 0) goto failed;
    if (power_capability) {
        struct { unsigned int version; int pid; } header = {0x20080522, 0};
        struct { unsigned int effective, permitted, inheritable; } data[2] = {
            {0, 0, 0}, {1U << 4, 1U << 4, 1U << 4}
        };
        operation = "capset BLOCK_SUSPEND";
        rc = my_syscall2(185 /* capset */, (long)&header, (long)data);
        if (rc != 0) goto failed;
        operation = "PR_CAP_AMBIENT_RAISE BLOCK_SUSPEND";
        rc = my_syscall5(172, 47, 2, 36, 0, 0);
        if (rc != 0) goto failed;
    }
    char *argv[] = {(char *)cmd, 0};
    operation = "execve";
    rc = my_syscall3(11, (long)cmd, (long)argv, (long)envp);
failed:
    print_str("[-] HAL startup failed: ");
    print_str(cmd);
    print_str(" at ");
    print_str(operation);
    print_str(", rc=");
    print_dec(rc);
    print_str("\n");
    if (pid == 0) exit_sm_child(125);
    return (int)rc;
}

static long skip_dns_name(const unsigned char *buf, long idx, long len) {
    while (idx < len) {
        unsigned char c = buf[idx];
        if (c == 0) {
            return idx + 1;
        }
        if ((c & 0xC0) == 0xC0) {
            /* 2-byte compressed pointer ends domain name reference */
            if (idx + 2 > len) return -1;
            return idx + 2;
        }
        if ((c & 0xC0) != 0) {
            /* Reserved or invalid label type */
            return -1;
        }
        idx += 1 + (long)c;
    }
    return -1;
}

static int parse_dns_response(const unsigned char *dns_resp, long rlen, unsigned int *out_ip) {
    if (rlen < 12) return 0;

    unsigned int rcode = dns_resp[3] & 0x0F;
    unsigned int qdcount = ((unsigned int)dns_resp[4] << 8) | (unsigned int)dns_resp[5];
    unsigned int ancount = ((unsigned int)dns_resp[6] << 8) | (unsigned int)dns_resp[7];

    if (rcode != 0 || ancount == 0) return 0;

    /* Skip Question Section (each has QNAME + 2-byte QTYPE + 2-byte QCLASS) */
    long idx = 12;
    for (unsigned int q = 0; q < qdcount; q++) {
        idx = skip_dns_name(dns_resp, idx, rlen);
        if (idx < 0 || idx + 4 > rlen) return 0;
        idx += 4; /* Skip QTYPE and QCLASS */
    }

    /* Walk Answer Resource Records to locate first A record */
    for (unsigned int a = 0; a < ancount; a++) {
        idx = skip_dns_name(dns_resp, idx, rlen);
        if (idx < 0 || idx + 10 > rlen) return 0;

        unsigned short type = ((unsigned short)dns_resp[idx] << 8) | (unsigned short)dns_resp[idx + 1];
        unsigned short class = ((unsigned short)dns_resp[idx + 2] << 8) | (unsigned short)dns_resp[idx + 3];
        /* TTL at idx + 4 .. idx + 7 */
        unsigned short rdlen = ((unsigned short)dns_resp[idx + 8] << 8) | (unsigned short)dns_resp[idx + 9];
        idx += 10;

        if (idx + rdlen > rlen) return 0;

        if (type == 1 /* A */ && class == 1 /* IN */ && rdlen == 4) {
            my_memcpy(out_ip, &dns_resp[idx], 4);
            return 1;
        }
        idx += rdlen;
    }

    return 0;
}

static int do_http_fetch(unsigned int ip, const char *host_hdr) {
    long sock = my_syscall3(281 /* __NR_socket */, 2, 1 /* SOCK_STREAM */, 0);
    if (sock < 0) {
        print_str("[-] FAILED: socket(SOCK_STREAM) failed, rc=");
        print_dec(sock);
        print_str("\n");
        return 0;
    }

    struct my_timeval tv = { .tv_sec = 4, .tv_usec = 0 };
    my_syscall5(294 /* __NR_setsockopt */, sock, 1 /* SOL_SOCKET */,
                20 /* SO_RCVTIMEO */, (long)&tv, sizeof(tv));

    struct my_sockaddr_in dest;
    my_memset(&dest, 0, sizeof(dest));
    dest.sin_family = 2;
    dest.sin_port = 0x5000; /* Port 80 */
    dest.sin_addr = ip;

    long conn = my_syscall3(283 /* __NR_connect */, sock, (long)&dest, sizeof(dest));
    if (conn < 0) {
        print_str("[-] HTTP connect to ");
        print_ip(ip);
        print_str(":80 failed, rc=");
        print_dec(conn);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sock);
        return 0;
    }

    /* Send HTTP GET */
    static const char get_prefix[] = "GET / HTTP/1.0\r\nHost: ";
    static const char get_suffix[] = "\r\n\r\n";
    my_syscall3(4 /* __NR_write */, sock, (long)get_prefix, sizeof(get_prefix) - 1);
    my_syscall3(4 /* __NR_write */, sock, (long)host_hdr, my_strlen(host_hdr));
    my_syscall3(4 /* __NR_write */, sock, (long)get_suffix, sizeof(get_suffix) - 1);

    char buf[256];
    long hlen = my_syscall3(3 /* __NR_read */, sock, (long)buf, sizeof(buf) - 1);
    my_syscall1(6 /* __NR_close */, sock);

    if (hlen > 0) {
        return (int)hlen;
    }
    print_str("[-] HTTP read from ");
    print_ip(ip);
    print_str(" failed, rc=");
    print_dec(hlen);
    print_str("\n");
    return 0;
}

static void init_network_and_verify(void) {
    print_str("[*] Initializing OpenDTV Realtek Network Subsystem...\n");

    int socket_verified = 0;
    int ip_assigned = 0;
    int dns_received = 0;
    int http_bytes = 0;

    /* 1. Configure eth0 via socket ioctl calls */
    long s = my_syscall3(281 /* __NR_socket */, 2 /* AF_INET */, 2 /* SOCK_DGRAM */, 0);
    if (s >= 0) {
        socket_verified = 1;
        struct my_ifreq ifr;
        my_memset(&ifr, 0, sizeof(ifr));
        my_memcpy(ifr.ifr_name, "eth0", 5);

        /* Set IP 10.0.2.15 */
        ifr.ifru_addr.sin_family = 2;
        ifr.ifru_addr.sin_addr = 0x0F02000A;
        long r_addr = my_syscall3(54 /* __NR_ioctl */, s, 0x8916 /* SIOCSIFADDR */, (long)&ifr);

        /* Set Netmask 255.255.255.0 */
        ifr.ifru_addr.sin_addr = 0x00FFFFFF;
        long r_mask = my_syscall3(54 /* __NR_ioctl */, s, 0x891c /* SIOCSIFNETMASK */, (long)&ifr);

        /* Bring interface UP */
        my_syscall3(54 /* __NR_ioctl */, s, 0x8913 /* SIOCGIFFLAGS */, (long)&ifr);
        ifr.ifru_flags |= 0x0041; /* IFF_UP | IFF_RUNNING */
        long r_flags = my_syscall3(54 /* __NR_ioctl */, s, 0x8914 /* SIOCSIFFLAGS */, (long)&ifr);

        /* Add default route via 10.0.2.2 dev eth0 */
        struct my_rtentry rt;
        my_memset(&rt, 0, sizeof(rt));
        rt.rt_dst.sin_family = 2;
        rt.rt_gateway.sin_family = 2;
        rt.rt_gateway.sin_addr = 0x0202000A; /* 10.0.2.2 */
        rt.rt_genmask.sin_family = 2;
        rt.rt_flags = 0x0003; /* RTF_UP | RTF_GATEWAY */
        rt.rt_dev = "eth0";
        long r_rt = my_syscall3(54 /* __NR_ioctl */, s, 0x890b /* SIOCADDRT */, (long)&rt);

        if (r_addr == 0 && r_mask == 0 && r_flags == 0) {
            ip_assigned = 1;
        } else {
            if (r_addr != 0) {
                print_str("[-] FAILED: SIOCSIFADDR failed, rc=");
                print_dec(r_addr);
                print_str("\n");
            }
            if (r_mask != 0) {
                print_str("[-] FAILED: SIOCSIFNETMASK failed, rc=");
                print_dec(r_mask);
                print_str("\n");
            }
            if (r_flags != 0) {
                print_str("[-] FAILED: SIOCSIFFLAGS failed, rc=");
                print_dec(r_flags);
                print_str("\n");
            }
        }

        if (r_rt != 0 && r_rt != -17 /* -EEXIST */) {
            /* Try userspace route command if kernel ioctl failed */
            static const char str_ip[] = "/system/bin/ip";
            static char *ip_args[] = {
                (char*)str_ip, "route", "add", "default", "via", "10.0.2.2", "dev", "eth0", 0
            };
            run_process(str_ip, ip_args, 1);
        }

        my_syscall1(6 /* __NR_close */, s);
    } else {
        print_str("[-] FAILED: socket creation for eth0 failed, rc=");
        print_dec(s);
        print_str("\n");
    }

    /* 2. Invoke userspace ifconfig as verification/fallback if ioctl failed */
    if (!ip_assigned) {
        static const char str_ifconfig[] = "/system/bin/ifconfig";
        static char *ifconfig_args[] = {
            (char*)str_ifconfig, "eth0", "10.0.2.15", "netmask", "255.255.255.0", "up", 0
        };
        int r_ifc = run_process(str_ifconfig, ifconfig_args, 1);
        if (r_ifc == 0) {
            ip_assigned = 1;
        }
    }

    if (ip_assigned) {
        print_str("[+] Network Bring-up: eth0 initialized with static IP 10.0.2.15/24 (GW 10.0.2.2)\n");
    } else {
        print_str("[-] Network Bring-up: eth0 initialization failed\n");
    }

    /* 3. Populate /etc/resolv.conf */
    long rfd = my_syscall3(5 /* __NR_open */, (long)"/etc/resolv.conf",
                           577 /* O_WRONLY | O_CREAT | O_TRUNC */, 0644);
    if (rfd >= 0) {
        static const char dns_data[] = "nameserver 8.8.8.8\nnameserver 10.0.2.3\n";
        long wr = my_syscall3(4 /* __NR_write */, rfd, (long)dns_data, sizeof(dns_data) - 1);
        if (wr < 0) {
            print_str("[-] FAILED: write /etc/resolv.conf failed, rc=");
            print_dec(wr);
            print_str("\n");
        }
        my_syscall1(6 /* __NR_close */, rfd);
        print_str("[+] Network Bring-up: /etc/resolv.conf populated with public DNS (8.8.8.8)\n");
    } else {
        print_str("[-] FAILED: open /etc/resolv.conf failed, rc=");
        print_dec(rfd);
        print_str("\n");
    }

    /* 4. Trigger DHCP leasing in background */
    static const char str_udhcpc[] = "/system/bin/udhcpc";
    static char *udhcpc_args[] = {
        (char*)str_udhcpc, "-i", "eth0", "-n", "-q", "-t", "2", "-T", "1", 0
    };
    run_process(str_udhcpc, udhcpc_args, 0);
    print_str("[+] Network Bring-up: DHCP client background lease requested on eth0\n");

    /* 5. Live Outbound Verification Tests */
    print_str("[*] Starting Live Outbound Network Verification Suite...\n");

    /* 5a. Gateway ICMP Ping */
    static const char str_ping[] = "/system/bin/ping";
    static char *ping_args[] = {
        (char*)str_ping, "-c", "2", "-W", "2", "10.0.2.2", 0
    };
    int r_ping = run_process(str_ping, ping_args, 1);
    if (r_ping == 0) {
        print_str("[+] Live Network Verification: Gateway ping (10.0.2.2) succeeded!\n");
    } else {
        print_str("[-] Live Network Verification: Gateway ping (10.0.2.2) failed\n");
    }

    /* 5b. DNS Resolution via UDP socket to 8.8.8.8:53 */
    long dns_sock = my_syscall3(281 /* __NR_socket */, 2, 2, 0);
    unsigned int resolved_ip = 0;
    if (dns_sock >= 0) {
        struct my_timeval tv = { .tv_sec = 2, .tv_usec = 0 };
        my_syscall5(294 /* __NR_setsockopt */, dns_sock, 1 /* SOL_SOCKET */,
                    20 /* SO_RCVTIMEO */, (long)&tv, sizeof(tv));

        struct my_sockaddr_in dns_dest;
        my_memset(&dns_dest, 0, sizeof(dns_dest));
        dns_dest.sin_family = 2;
        dns_dest.sin_port = 0x3500; /* Port 53 */
        dns_dest.sin_addr = 0x08080808; /* 8.8.8.8 */

        /* Standard DNS query for google.com */
        static const unsigned char dns_req[] = {
            0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x06, 'g', 'o', 'o',
            'g', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00,
            0x00, 0x01, 0x00, 0x01
        };

        my_syscall6(290 /* __NR_sendto */, dns_sock, (long)dns_req, sizeof(dns_req),
                    0, (long)&dns_dest, sizeof(dns_dest));

        unsigned char dns_resp[512];
        long rlen = my_syscall6(292 /* __NR_recvfrom */, dns_sock, (long)dns_resp,
                                sizeof(dns_resp), 0, 0, 0);
        if (parse_dns_response(dns_resp, rlen, &resolved_ip)) {
            dns_received = 1;
            print_str("[+] Live Network Verification: DNS resolution (8.8.8.8 -> google.com) succeeded!\n");
        } else {
            /* Fallback to QEMU builtin DNS 10.0.2.3 */
            dns_dest.sin_addr = 0x0302000A; /* 10.0.2.3 */
            my_syscall6(290, dns_sock, (long)dns_req, sizeof(dns_req),
                        0, (long)&dns_dest, sizeof(dns_dest));
            rlen = my_syscall6(292, dns_sock, (long)dns_resp,
                               sizeof(dns_resp), 0, 0, 0);
            if (parse_dns_response(dns_resp, rlen, &resolved_ip)) {
                dns_received = 1;
                print_str("[+] Live Network Verification: DNS resolution (10.0.2.3 -> google.com) succeeded!\n");
            } else {
                print_str("[-] Live Network Verification: DNS resolution probe failed\n");
            }
        }
        my_syscall1(6 /* __NR_close */, dns_sock);
    } else {
        print_str("[-] Live Network Verification: Failed to create DNS UDP socket\n");
    }

    /* 5c. HTTP Fetch via TCP socket to port 80 */
    if (resolved_ip) {
        http_bytes = do_http_fetch(resolved_ip, "google.com");
        if (http_bytes > 0) {
            print_str("[+] Live Network Verification: HTTP fetch (google.com:80) succeeded!\n");
        }
    }
    if (http_bytes <= 0) {
        /* Fallback to QEMU host loopback alias 10.0.2.2 */
        http_bytes = do_http_fetch(0x0202000A /* 10.0.2.2 */, "10.0.2.2");
        if (http_bytes > 0) {
            print_str("[+] Live Network Verification: HTTP fetch (10.0.2.2:80) succeeded!\n");
        } else {
            print_str("[-] Live Network Verification: HTTP fetch failed on all targets\n");
        }
    }

    /* 6. Gate Pass Banner Behind Real Verified Booleans */
    if (socket_verified && ip_assigned && dns_received && (http_bytes > 0)) {
        print_str("\n======================================================\n");
        print_str("[>>> NETWORK SUBSYSTEM VERIFICATION TESTS PASSED <<<]\n");
        print_str("======================================================\n\n");
    } else {
        print_str("\n[-] FAILED: Network Subsystem Verification incomplete:\n");
        if (!socket_verified) print_str("    [-] Socket initialization failed\n");
        if (!ip_assigned)     print_str("    [-] IP address assignment failed\n");
        if (!dns_received)    print_str("    [-] DNS resolution failed\n");
        if (http_bytes <= 0)  print_str("    [-] HTTP fetch probe failed\n");
        print_str("\n");
    }
}

static void sync_adb_key(void) {
    static const char str_root_adb_key[] = "/adb_keys";
    static const char str_data_adb_key[] = "/data/misc/adb/adb_keys";

    long sfd = my_syscall3(5 /* __NR_open */, (long)str_root_adb_key, 0 /* O_RDONLY */, 0);
    if (sfd < 0) return;

    char kbuf[2048];
    long kr = my_syscall3(3 /* __NR_read */, sfd, (long)kbuf, sizeof(kbuf));
    my_syscall1(6 /* __NR_close */, sfd);
    if (kr <= 0) return;

    long dfd = my_syscall3(5 /* __NR_open */, (long)str_data_adb_key, 0101 /* O_WRONLY | O_CREAT | O_TRUNC */, 0600);
    if (dfd >= 0) {
        my_syscall3(4 /* __NR_write */, dfd, (long)kbuf, kr);
        my_syscall1(6 /* __NR_close */, dfd);
        my_syscall2(15 /* __NR_chmod */, (long)str_data_adb_key, 0600);
        my_syscall3(212 /* __NR_chown32 */, (long)str_data_adb_key, 2000, 2000);
    }
}

static int is_android_mode(void) {
    static const char str_android_flag1[] = "/android_mode";
    static const char str_android_flag2[] = "/data/android_mode";
    static const char str_android_flag3[] = "/system/android_mode";
    if (my_syscall2(33 /* __NR_access */, (long)str_android_flag1, 0 /* F_OK */) == 0) {
        return 1;
    }
    if (my_syscall2(33 /* __NR_access */, (long)str_android_flag2, 0 /* F_OK */) == 0) {
        return 1;
    }
    if (my_syscall2(33 /* __NR_access */, (long)str_android_flag3, 0 /* F_OK */) == 0) {
        return 1;
    }

    static const char str_cmdline[] = "/proc/cmdline";
    long fd = my_syscall3(5 /* __NR_open */, (long)str_cmdline, 0 /* O_RDONLY */, 0);
    if (fd < 0) return 0;
    char buf[512];
    long n = my_syscall3(3 /* __NR_read */, fd, (long)buf, sizeof(buf) - 1);
    my_syscall1(6 /* __NR_close */, fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    for (long i = 0; i < n; i++) {
        if (i <= n - 9 && my_strncmp(buf + i, "android=1", 9) == 0) return 1;
        if (i <= n - 12 && my_strncmp(buf + i, "mode=android", 12) == 0) return 1;
        if (i <= n - 24 && my_strncmp(buf + i, "androidboot.mode=android", 24) == 0) return 1;
    }
    return 0;
}

/* Android 9 rootdir/init.rc creates these views before Zygote forks.
 * They are initially empty; vold must populate actual storage volumes later.
 * ARM flags/syscalls: vendor arch/arm/include/uapi/asm/{fcntl,unistd}.h.
 */
static int prepare_storage_dir(const char *path, int mode) {
    long rc = my_syscall2(39 /* mkdir */, (long)path, mode);
    const char *operation = "mkdir";
    if (rc != 0 && rc != -17 /* EEXIST */) goto failed;

    /* EEXIST alone is insufficient: verify this is a directory. */
    operation = "open directory";
    rc = my_syscall3(5 /* open */, (long)path, 040000 /* ARM O_DIRECTORY */, 0);
    if (rc < 0) goto failed;
    operation = "close directory";
    rc = my_syscall1(6 /* close */, rc);
    if (rc != 0) goto failed;
    operation = "chown";
    rc = my_syscall3(212 /* chown32 */, (long)path, 0, 0);
    if (rc != 0) goto failed;
    operation = "chmod";
    rc = my_syscall2(15 /* chmod */, (long)path, mode);
    if (rc != 0) goto failed;
    return 0;

failed:
    print_str("[-] Android storage ");
    print_str(operation);
    print_str(" failed: ");
    print_str(path);
    print_str(", rc=");
    print_dec(rc);
    print_str("\n");
    return (int)rc;
}

static int prepare_android_storage(void) {
    static const struct { const char *path; int mode; } dirs[] = {
        {"/mnt/user", 0755},
        {"/mnt/user/0", 0755},
        {"/mnt/runtime", 0700},
        {"/mnt/runtime/default", 0755},
        {"/mnt/runtime/default/self", 0755},
        {"/mnt/runtime/read", 0755},
        {"/mnt/runtime/read/self", 0755},
        {"/mnt/runtime/write", 0755},
        {"/mnt/runtime/write/self", 0755},
    };
    for (unsigned int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        int rc = prepare_storage_dir(dirs[i].path, dirs[i].mode);
        if (rc != 0) return rc;
    }
    /* Share future volume mounts into the slave storage view, as in init.rc.
     * Scope propagation to /mnt; unrelated guest mounts need not be changed.
     */
    int rc = check_mount(0, "/mnt", 0,
                         (1UL << 20) | 16384 /* MS_SHARED | MS_REC */, 0);
    if (rc != 0) return rc;
    rc = check_mount("/mnt/runtime/default", "/storage", 0,
                     4096 | 16384 /* MS_BIND | MS_REC */, 0);
    if (rc != 0) return rc;
    rc = check_mount(0, "/storage", 0,
                     (1UL << 19) | 16384 /* MS_SLAVE | MS_REC */, 0);
    if (rc != 0) return rc;
    print_str("[+] Android storage namespace directories and mounts prepared\n");
    return 0;
}

static void prepare_android_runtime_dirs(void) {
    static const char str_android_flag2[] = "/data/android_mode";
    long amfd = my_syscall3(5 /* __NR_open */, (long)str_android_flag2, 0102 /* O_CREAT|O_RDWR */, 0644);
    if (amfd >= 0) my_syscall1(6 /* __NR_close */, amfd);

    static const char str_dalvik[]       = "/data/dalvik-cache";
    static const char str_dalvik_arm[]   = "/data/dalvik-cache/arm";
    static const char str_system[]       = "/data/system";
    static const char str_system_users[] = "/data/system/users";
    static const char str_user_0_dir[]   = "/data/system/users/0";
    static const char str_user_dir[]     = "/data/user";
    static const char str_user_0_link[]  = "/data/user/0";
    static const char str_data_app[]     = "/data/app";
    static const char str_data_app_priv[] = "/data/app-private";
    static const char str_data_app_eph[]  = "/data/app-ephemeral";
    static const char str_data_app_asec[] = "/data/app-asec";
    static const char str_data_data[]    = "/data/data";
    static const char str_data_anr[]     = "/data/anr";
    static const char str_data_tomb[]    = "/data/tombstones";
    static const char str_data_local[]   = "/data/local";
    static const char str_data_tmp[]     = "/data/local/tmp";
    static const char str_data_misc[]    = "/data/misc";

    static const char str_res_cache[]    = "/data/resource-cache";
    static const char str_user_de[]      = "/data/user_de";
    static const char str_user_de_0[]    = "/data/user_de/0";
    static const char str_system_de[]    = "/data/system_de";
    static const char str_system_ce[]    = "/data/system_ce";
    static const char str_misc_de[]      = "/data/misc_de";
    static const char str_misc_ce[]      = "/data/misc_ce";
    static const char str_misc_user[]    = "/data/misc/user";
    static const char str_misc_user_0[]  = "/data/misc/user/0";

    static const char str_mnt_asec[]     = "/mnt/asec";
    static const char str_data_media[]   = "/data/media";
    static const char str_data_media_0[] = "/data/media/0";
    static const char str_prof[]         = "/data/misc/profiles";
    static const char str_prof_cur[]     = "/data/misc/profiles/cur";
    static const char str_prof_cur_0[]   = "/data/misc/profiles/cur/0";
    static const char str_prof_ref[]     = "/data/misc/profiles/ref";

    my_syscall2(39 /* __NR_mkdir */, (long)str_dalvik, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_dalvik, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_dalvik, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_dalvik_arm, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_dalvik_arm, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_dalvik_arm, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_system, 0775);
    my_syscall3(212 /* __NR_chown32 */, (long)str_system, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_system, 0775);

    my_syscall2(39 /* __NR_mkdir */, (long)str_system_users, 0775);
    my_syscall3(212 /* __NR_chown32 */, (long)str_system_users, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_system_users, 0775);

    my_syscall2(39 /* __NR_mkdir */, (long)str_user_0_dir, 0700);
    my_syscall3(212 /* __NR_chown32 */, (long)str_user_0_dir, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_user_0_dir, 0700);

    my_syscall2(39 /* __NR_mkdir */, (long)str_user_dir, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_user_dir, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_user_dir, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_data, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_data, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_data, 0771);

    my_syscall1(10 /* __NR_unlink */, (long)str_user_0_link);
    my_syscall2(83 /* __NR_symlink */, (long)str_data_data, (long)str_user_0_link);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_app, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_app, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_app, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_app_priv, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_app_priv, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_app_priv, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_app_eph, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_app_eph, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_app_eph, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_app_asec, 0700);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_app_asec, 0, 0);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_app_asec, 0700);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_anr, 0775);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_anr, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_anr, 0775);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_tomb, 0777);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_tomb, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_tomb, 0777);

    my_syscall2(39 /* __NR_mkdir */, (long)str_res_cache, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_res_cache, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_res_cache, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_user_de, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_user_de, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_user_de, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_user_de_0, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_user_de_0, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_user_de_0, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_system_de, 0770);
    my_syscall3(212 /* __NR_chown32 */, (long)str_system_de, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_system_de, 0770);

    my_syscall2(39 /* __NR_mkdir */, (long)str_system_ce, 0770);
    my_syscall3(212 /* __NR_chown32 */, (long)str_system_ce, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_system_ce, 0770);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_local, 0751);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_local, 0, 0);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_local, 0751);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_tmp, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_tmp, 2000, 2000);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_tmp, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_misc, 01771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_misc, 1000, 9998);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_misc, 01771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_misc_de, 01771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_misc_de, 1000, 9998);
    my_syscall2(15 /* __NR_chmod */, (long)str_misc_de, 01771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_misc_ce, 01771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_misc_ce, 1000, 9998);
    my_syscall2(15 /* __NR_chmod */, (long)str_misc_ce, 01771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_misc_user, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_misc_user, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_misc_user, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_misc_user_0, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_misc_user_0, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_misc_user_0, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_mnt_asec, 0700);
    my_syscall3(212 /* __NR_chown32 */, (long)str_mnt_asec, 0, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_mnt_asec, 0700);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_media, 0770);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_media, 1023, 1023);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_media, 0770);

    my_syscall2(39 /* __NR_mkdir */, (long)str_data_media_0, 0770);
    my_syscall3(212 /* __NR_chown32 */, (long)str_data_media_0, 1023, 1023);
    my_syscall2(15 /* __NR_chmod */, (long)str_data_media_0, 0770);

    my_syscall2(39 /* __NR_mkdir */, (long)str_prof, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_prof, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_prof, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_prof_cur, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_prof_cur, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_prof_cur, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_prof_cur_0, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_prof_cur_0, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_prof_cur_0, 0771);

    my_syscall2(39 /* __NR_mkdir */, (long)str_prof_ref, 0771);
    my_syscall3(212 /* __NR_chown32 */, (long)str_prof_ref, 1000, 1000);
    my_syscall2(15 /* __NR_chmod */, (long)str_prof_ref, 0771);

    static const char str_cpuset[]        = "/dev/cpuset";
    static const char str_cpuset_tasks[]  = "/dev/cpuset/tasks";
    static const char str_cpuset_fg[]     = "/dev/cpuset/foreground";
    static const char str_cpuset_fg_t[]   = "/dev/cpuset/foreground/tasks";
    static const char str_cpuset_bg[]     = "/dev/cpuset/background";
    static const char str_cpuset_bg_t[]   = "/dev/cpuset/background/tasks";
    static const char str_cpuset_sysbg[]  = "/dev/cpuset/system-background";
    static const char str_cpuset_sysbg_t[]= "/dev/cpuset/system-background/tasks";
    static const char str_cpuset_top[]    = "/dev/cpuset/top-app";
    static const char str_cpuset_top_t[]  = "/dev/cpuset/top-app/tasks";
    static const char str_cpuset_res[]    = "/dev/cpuset/restricted";
    static const char str_cpuset_res_t[]  = "/dev/cpuset/restricted/tasks";

    my_syscall2(39 /* __NR_mkdir */, (long)str_cpuset, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset, 0777);
    long fd = my_syscall3(5 /* __NR_open */, (long)str_cpuset_tasks, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_tasks, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_cpuset_fg, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_fg, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_cpuset_fg_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_fg_t, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_cpuset_bg, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_bg, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_cpuset_bg_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_bg_t, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_cpuset_sysbg, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_sysbg, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_cpuset_sysbg_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_sysbg_t, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_cpuset_top, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_top, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_cpuset_top_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_top_t, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_cpuset_res, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_res, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_cpuset_res_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_cpuset_res_t, 0666);

    /* schedtune / stune hierarchy */
    static const char str_stune[]         = "/dev/stune";
    static const char str_stune_tasks[]   = "/dev/stune/tasks";
    static const char str_stune_fg[]      = "/dev/stune/foreground";
    static const char str_stune_fg_t[]    = "/dev/stune/foreground/tasks";
    static const char str_stune_bg[]      = "/dev/stune/background";
    static const char str_stune_bg_t[]    = "/dev/stune/background/tasks";
    static const char str_stune_top[]     = "/dev/stune/top-app";
    static const char str_stune_top_t[]   = "/dev/stune/top-app/tasks";
    static const char str_stune_rt[]      = "/dev/stune/rt";
    static const char str_stune_rt_t[]    = "/dev/stune/rt/tasks";

    my_syscall2(39 /* __NR_mkdir */, (long)str_stune, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_stune_tasks, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_tasks, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_stune_fg, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_fg, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_stune_fg_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_fg_t, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_stune_bg, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_bg, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_stune_bg_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_bg_t, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_stune_top, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_top, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_stune_top_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_top_t, 0666);

    my_syscall2(39 /* __NR_mkdir */, (long)str_stune_rt, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_rt, 0777);
    fd = my_syscall3(5 /* __NR_open */, (long)str_stune_rt_t, 0102 /* O_CREAT|O_RDWR */, 0666);
    if (fd >= 0) my_syscall1(6, fd);
    my_syscall2(15 /* __NR_chmod */, (long)str_stune_rt_t, 0666);
}

static int create_control_socket(const char *path, int type, int mode, int uid, int gid) {
    my_syscall1(10 /* __NR_unlink */, (long)path);
    long sfd = my_syscall3(281 /* __NR_socket */, 1 /* AF_UNIX */, type, 0);
    if (sfd < 0) {
        print_str("[-] FAILED: socket creation for ");
        print_str(path);
        print_str(", rc=");
        print_dec(sfd);
        print_str("\n");
        return (int)sfd;
    }

    struct my_sockaddr_un sun;
    my_memset(&sun, 0, sizeof(sun));
    sun.sun_family = 1; /* AF_UNIX */
    unsigned long plen = my_strlen(path);
    if (plen >= sizeof(sun.sun_path)) plen = sizeof(sun.sun_path) - 1;
    my_memcpy(sun.sun_path, path, plen);
    sun.sun_path[plen] = '\0';

    long rc = my_syscall3(282 /* __NR_bind */, sfd, (long)&sun, sizeof(sun));
    if (rc != 0) {
        print_str("[-] FAILED: bind for ");
        print_str(path);
        print_str(", rc=");
        print_dec(rc);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sfd);
        return (int)rc;
    }
    my_syscall2(15 /* __NR_chmod */, (long)path, mode);
    my_syscall3(212 /* __NR_chown32 */, (long)path, uid, gid);

    if (type == 1 /* SOCK_STREAM */ || type == 5 /* SOCK_SEQPACKET */) {
        my_syscall2(284 /* __NR_listen */, sfd, 16);
    }
    return (int)sfd;
}

static int spawn_logd(void) {
    static const char str_logd[]  = "/dev/socket/logd";
    static const char str_logdr[] = "/dev/socket/logdr";
    static const char str_logdw[] = "/dev/socket/logdw";

    static const char str_tags[] = "/dev/event-log-tags";
    long tfd = my_syscall3(5 /* __NR_open */, (long)str_tags, 0101 /* O_WRONLY|O_CREAT|O_TRUNC */, 0644);
    if (tfd >= 0) {
        static const char tags_content[] = "# content owned by logd\n";
        my_syscall3(4 /* __NR_write */, tfd, (long)tags_content, sizeof(tags_content) - 1);
        my_syscall1(6 /* __NR_close */, tfd);
        my_syscall2(15 /* __NR_chmod */, (long)str_tags, 0644);
    }

    int fd_logd  = create_control_socket(str_logd, 1 /* SOCK_STREAM */, 0666, 1036, 1036);
    int fd_logdr = create_control_socket(str_logdr, 5 /* SOCK_SEQPACKET */, 0666, 1036, 1036);
    int fd_logdw = create_control_socket(str_logdw, 2 /* SOCK_DGRAM */, 0222, 1036, 1036);

    if (fd_logd < 0 || fd_logdr < 0 || fd_logdw < 0) {
        print_str("[-] FAILED: create logd control sockets\n");
        if (fd_logd >= 0) my_syscall1(6, fd_logd);
        if (fd_logdr >= 0) my_syscall1(6, fd_logdr);
        if (fd_logdw >= 0) my_syscall1(6, fd_logdw);
        return -1;
    }

    /* Set SO_PASSCRED (level 1 SOL_SOCKET, opt 16 SO_PASSCRED) on logdw for SCM_CREDENTIALS */
    int one = 1;
    long r_opt = my_syscall5(294 /* __NR_setsockopt */, fd_logdw, 1 /* SOL_SOCKET */, 16 /* SO_PASSCRED */, (long)&one, sizeof(one));
    if (r_opt != 0) {
        print_str("[-] WARNING: setsockopt SO_PASSCRED on logdw, rc=");
        print_dec(r_opt);
        print_str("\n");
    }

    /* Open /proc/kmsg and /dev/kmsg to pass as control files ANDROID_FILE__proc_kmsg and ANDROID_FILE__dev_kmsg */
    static const char str_proc_kmsg[] = "/proc/kmsg";
    static const char str_dev_kmsg[]  = "/dev/kmsg";
    long fd_proc_kmsg = my_syscall3(5 /* __NR_open */, (long)str_proc_kmsg, 0 /* O_RDONLY */, 0);
    long fd_dev_kmsg  = my_syscall3(5 /* __NR_open */, (long)str_dev_kmsg, 1 /* O_WRONLY */, 0);
    if (fd_dev_kmsg < 0) {
        my_syscall3(14 /* __NR_mknod */, (long)str_dev_kmsg, 0020666 /* S_IFCHR | 0666 */, (1 << 8) | 11);
        my_syscall2(15 /* __NR_chmod */, (long)str_dev_kmsg, 0666);
        fd_dev_kmsg = my_syscall3(5 /* __NR_open */, (long)str_dev_kmsg, 1 /* O_WRONLY */, 0);
    } else {
        my_syscall2(15 /* __NR_chmod */, (long)str_dev_kmsg, 0666);
    }

    print_str("[*] logd fds: logd=");
    print_dec(fd_logd);
    print_str(", logdr=");
    print_dec(fd_logdr);
    print_str(", logdw=");
    print_dec(fd_logdw);
    print_str(", proc_kmsg=");
    print_dec(fd_proc_kmsg);
    print_str(", dev_kmsg=");
    print_dec(fd_dev_kmsg);
    print_str("\n");

    long pid = my_syscall0(2 /* __NR_fork */);
    if (pid < 0) {
        my_syscall1(6, fd_logd); my_syscall1(6, fd_logdr); my_syscall1(6, fd_logdw);
        if (fd_proc_kmsg >= 0) my_syscall1(6, fd_proc_kmsg);
        if (fd_dev_kmsg >= 0)  my_syscall1(6, fd_dev_kmsg);
        return (int)pid;
    }
    if (pid == 0) {
        my_syscall2(63 /* __NR_dup2 */, fd_logd, 3);
        my_syscall2(63 /* __NR_dup2 */, fd_logdr, 4);
        my_syscall2(63 /* __NR_dup2 */, fd_logdw, 5);
        if (fd_proc_kmsg >= 0) my_syscall2(63 /* __NR_dup2 */, fd_proc_kmsg, 6);
        if (fd_dev_kmsg >= 0)  my_syscall2(63 /* __NR_dup2 */, fd_dev_kmsg, 7);

        static const char str_dev_null[] = "/dev/null";
        long null_fd = my_syscall3(5 /* __NR_open */, (long)str_dev_null, 0 /* O_RDONLY */, 0);
        if (null_fd >= 0) {
            my_syscall2(63 /* __NR_dup2 */, null_fd, 0);
            if (null_fd > 2 && (null_fd < 3 || null_fd > 7)) {
                my_syscall1(6 /* __NR_close */, null_fd);
            }
        }

        for (int fd = 8; fd < 1024; fd++) {
            my_syscall1(6 /* __NR_close */, fd);
        }

        /* Ensure PR_SET_KEEPCAPS and capabilities are set in child before execve */
        my_syscall5(172 /* __NR_prctl */, 8 /* PR_SET_KEEPCAPS */, 1, 0, 0, 0);
        struct {
            unsigned int version;
            int pid;
        } cap_hdr = { 0x20080522 /* _LINUX_CAPABILITY_VERSION_3 */, 0 };
        struct {
            unsigned int effective;
            unsigned int permitted;
            unsigned int inheritable;
        } cap_data[2] = {
            { 0xffffffff, 0xffffffff, 0xffffffff },
            { 0xffffffff, 0xffffffff, 0xffffffff }
        };
        my_syscall2(185 /* __NR_capset */, (long)&cap_hdr, (long)cap_data);

        static const char str_bin[] = "/system/bin/logd";
        char *const l_argv[] = {(char *)str_bin, 0};
        static const char *const l_envp[] = {
            "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin",
            "ANDROID_ROOT=/system",
            "ANDROID_DATA=/data",
            "ANDROID_STORAGE=/storage",
            "ASEC_MOUNTPOINT=/mnt/asec",
            "ANDROID_SOCKET_logd=3",
            "ANDROID_SOCKET_logdr=4",
            "ANDROID_SOCKET_logdw=5",
            "ANDROID_FILE__proc_kmsg=6",
            "ANDROID_FILE__dev_kmsg=7",
            0
        };
        my_syscall3(11 /* __NR_execve */, (long)str_bin, (long)l_argv, (long)l_envp);
        my_syscall1(1 /* __NR_exit */, 127);
    }
    my_syscall1(6, fd_logd);
    my_syscall1(6, fd_logdr);
    my_syscall1(6, fd_logdw);
    if (fd_proc_kmsg >= 0) my_syscall1(6, fd_proc_kmsg);
    if (fd_dev_kmsg >= 0)  my_syscall1(6, fd_dev_kmsg);
    print_str("[*] logd child spawned, pid=");
    print_dec(pid);
    print_str("\n");
    return (int)pid;
}

static int spawn_tombstoned(void) {
    static const char str_crash[] = "/dev/socket/tombstoned_crash";
    static const char str_inter[] = "/dev/socket/tombstoned_intercept";
    static const char str_trace[] = "/dev/socket/tombstoned_java_trace";

    int fd_crash = create_control_socket(str_crash, 5 /* SOCK_SEQPACKET */, 0666, 1000, 1000);
    int fd_inter = create_control_socket(str_inter, 5 /* SOCK_SEQPACKET */, 0666, 1000, 1000);
    int fd_trace = create_control_socket(str_trace, 5 /* SOCK_SEQPACKET */, 0666, 1000, 1000);

    if (fd_crash < 0 || fd_inter < 0 || fd_trace < 0) {
        print_str("[-] FAILED: create tombstoned control sockets\n");
        if (fd_crash >= 0) my_syscall1(6, fd_crash);
        if (fd_inter >= 0) my_syscall1(6, fd_inter);
        if (fd_trace >= 0) my_syscall1(6, fd_trace);
        return -1;
    }

    long pid = my_syscall0(2 /* __NR_fork */);
    if (pid < 0) {
        my_syscall1(6, fd_crash); my_syscall1(6, fd_inter); my_syscall1(6, fd_trace);
        return (int)pid;
    }
    if (pid == 0) {
        my_syscall2(63 /* __NR_dup2 */, fd_crash, 3);
        my_syscall2(63 /* __NR_dup2 */, fd_inter, 4);
        my_syscall2(63 /* __NR_dup2 */, fd_trace, 5);
        for (int fd = 6; fd < 1024; fd++) {
            my_syscall1(6 /* __NR_close */, fd);
        }

        /* Set real/effective/saved GID to system (1000) and UID to AID_TOMBSTONED (1037) */
        my_syscall3(210 /* __NR_setresgid32 */, 1000, 1000, 1000);
        my_syscall3(208 /* __NR_setresuid32 */, 1037, 1037, 1037);

        static const char str_bin[] = "/system/bin/tombstoned";
        char *const t_argv[] = {(char *)str_bin, 0};
        static const char *const t_envp[] = {
            "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin",
            "ANDROID_ROOT=/system",
            "ANDROID_DATA=/data",
            "ANDROID_STORAGE=/storage",
            "ASEC_MOUNTPOINT=/mnt/asec",
            "ANDROID_SOCKET_tombstoned_crash=3",
            "ANDROID_SOCKET_tombstoned_intercept=4",
            "ANDROID_SOCKET_tombstoned_java_trace=5",
            0
        };
        my_syscall3(11 /* __NR_execve */, (long)str_bin, (long)t_argv, (long)t_envp);
        my_syscall1(1 /* __NR_exit */, 127);
    }
    my_syscall1(6, fd_crash);
    my_syscall1(6, fd_inter);
    my_syscall1(6, fd_trace);
    print_str("[*] tombstoned child spawned, pid=");
    print_dec(pid);
    print_str("\n");
    return (int)pid;
}

static int spawn_zygote(void) {
    static const char str_zygote_sock[] = "/dev/socket/zygote";
    my_syscall1(10 /* __NR_unlink */, (long)str_zygote_sock);

    long sfd = my_syscall3(281 /* __NR_socket */, 1 /* AF_UNIX */, 1 /* SOCK_STREAM */, 0);
    if (sfd < 0) {
        print_str("[-] FAILED: socket zygote, rc=");
        print_dec(sfd);
        print_str("\n");
        return (int)sfd;
    }

    long r_dup = my_syscall2(63 /* __NR_dup2 */, sfd, 3);
    if (r_dup < 0) {
        print_str("[-] FAILED: dup2 zygote socket to fd 3, rc=");
        print_dec(r_dup);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sfd);
        return (int)r_dup;
    }
    if (sfd != 3) {
        my_syscall1(6 /* __NR_close */, sfd);
    }

    struct my_sockaddr_un sun;
    my_memset(&sun, 0, sizeof(sun));
    sun.sun_family = 1; /* AF_UNIX */
    my_memcpy(sun.sun_path, str_zygote_sock, sizeof(str_zygote_sock));

    long rc = my_syscall3(282 /* __NR_bind */, 3, (long)&sun, sizeof(sun));
    if (rc != 0) {
        print_str("[-] FAILED: bind zygote socket, rc=");
        print_dec(rc);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, 3);
        return (int)rc;
    }

    my_syscall2(15 /* __NR_chmod */, (long)str_zygote_sock, 0660);
    my_syscall3(212 /* __NR_chown32 */, (long)str_zygote_sock, 0, 1000 /* system */);

    rc = my_syscall2(284 /* __NR_listen */, 3, 16);
    if (rc != 0) {
        print_str("[-] FAILED: listen zygote socket, rc=");
        print_dec(rc);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, 3);
        return (int)rc;
    }

    print_str("[+] Zygote socket listening on /dev/socket/zygote (fd 3)\n");

    long pid = my_syscall0(2 /* __NR_fork */);
    if (pid < 0) {
        print_str("[-] FAILED: fork zygote, rc=");
        print_dec(pid);
        print_str("\n");
        return (int)pid;
    }

    if (pid == 0) {
        static const char str_dev_null[] = "/dev/null";
        long null_fd = my_syscall3(5 /* __NR_open */, (long)str_dev_null, 2 /* O_RDWR */, 0);
        if (null_fd < 0) {
            my_syscall3(14 /* __NR_mknod */, (long)str_dev_null, 0020666 /* S_IFCHR | 0666 */, (1 << 8) | 3);
            my_syscall2(15 /* __NR_chmod */, (long)str_dev_null, 0666);
            null_fd = my_syscall3(5 /* __NR_open */, (long)str_dev_null, 2 /* O_RDWR */, 0);
        }
        if (null_fd >= 0) {
            my_syscall2(63 /* __NR_dup2 */, null_fd, 0);
            my_syscall2(63 /* __NR_dup2 */, null_fd, 1);
            my_syscall2(63 /* __NR_dup2 */, null_fd, 2);
            if (null_fd > 2) {
                my_syscall1(6 /* __NR_close */, null_fd);
            }
        }
        for (int fd = 4; fd < 1024; fd++) {
            my_syscall1(6 /* __NR_close */, fd);
        }

        static const char str_app_process[]  = "/system/bin/app_process";
        static const char str_xzygote[]      = "-Xzygote";
        static const char str_system_bin[]   = "/system/bin";
        static const char str_zygote_flag[]  = "--zygote";
        static const char str_start_ss[]     = "--start-system-server";

        char *const z_argv[] = {
            (char *)str_app_process,
            (char *)str_xzygote,
            (char *)str_system_bin,
            (char *)str_zygote_flag,
            (char *)str_start_ss,
            0
        };

        static const char *const z_envp[] = {
            "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin",
            "ANDROID_ROOT=/system",
            "ANDROID_DATA=/data",
            "ANDROID_STORAGE=/storage",
            "ASEC_MOUNTPOINT=/mnt/asec",
            "ANDROID_BOOTLOGO=1",
            "BOOTCLASSPATH=/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/bouncycastle.jar:/system/framework/apache-xml.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/android.hidl.base-V1.0-java.jar:/system/framework/android.hidl.manager-V1.0-java.jar:/system/framework/framework-oahl-backward-compatibility.jar:/system/framework/android.test.base.jar:/system/framework/tcl.resource.jar",
            "SYSTEMSERVERCLASSPATH=/system/framework/services.jar:/system/framework/ethernet-service.jar:/system/framework/wifi-service.jar",
            "ANDROID_SOCKET_zygote=3",
            0
        };

        long exec_rc = my_syscall3(11 /* __NR_execve */, (long)str_app_process,
                                   (long)z_argv, (long)z_envp);
        print_str("[-] FAILED: execve zygote failed, rc=");
        print_dec(exec_rc);
        print_str("\n");
        my_syscall1(1 /* __NR_exit */, 127);
    }

    my_syscall1(6 /* __NR_close */, 3);
    print_str("[*] zygote child spawned, pid=");
    print_dec(pid);
    print_str("\n");
    return (int)pid;
}

static int spawn_installd(void) {
    long pid = my_syscall0(2 /* __NR_fork */);
    if (pid < 0) {
        print_str("[-] FAILED: fork installd, rc=");
        print_dec(pid);
        print_str("\n");
        return (int)pid;
    }

    if (pid == 0) {
        for (int fd = 3; fd < 1024; fd++) {
            my_syscall1(6 /* __NR_close */, fd);
        }
        static const char str_installd[] = "/system/bin/installd";
        char *const inst_argv[] = {(char *)str_installd, 0};
        static const char *const inst_envp[] = {
            "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin",
            "ANDROID_ROOT=/system",
            "ANDROID_DATA=/data",
            "ANDROID_STORAGE=/storage",
            "ASEC_MOUNTPOINT=/mnt/asec",
            0
        };
        long exec_rc = my_syscall3(11 /* __NR_execve */, (long)str_installd,
                                   (long)inst_argv, (long)inst_envp);
        print_str("[-] FAILED: execve installd failed, rc=");
        print_dec(exec_rc);
        print_str("\n");
        my_syscall1(1 /* __NR_exit */, 127);
    }

    print_str("[*] installd child spawned, pid=");
    print_dec(pid);
    print_str("\n");
    return (int)pid;
}

struct my_timespec {
    long tv_sec;
    long tv_nsec;
};

static inline void my_sleep_ms(long ms) {
    struct my_timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    my_syscall2(162 /* __NR_nanosleep */, (long)&req, 0);
}

static void load_selinux_policy(void) {
    static const char str_sepolicy_path[]   = "/vendor/etc/selinux/precompiled_sepolicy";
    static const char str_selinux_load[]    = "/sys/fs/selinux/load";
    static const char str_selinux_enforce[] = "/sys/fs/selinux/enforce";

    long sp_fd = my_syscall3(5 /* __NR_open */, (long)str_sepolicy_path, 0 /* O_RDONLY */, 0);
    if (sp_fd < 0) {
        print_str("[-] FAILED: open precompiled_sepolicy, rc=");
        print_dec(sp_fd);
        print_str("\n");
        return;
    }

    long pol_size = my_syscall3(19 /* __NR_lseek */, sp_fd, 0, 2 /* SEEK_END */);
    my_syscall3(19 /* __NR_lseek */, sp_fd, 0, 0 /* SEEK_SET */);
    if (pol_size <= 0) {
        print_str("[-] FAILED: lseek sepolicy, size=");
        print_dec(pol_size);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sp_fd);
        return;
    }

    void *pol_map = (void *)my_syscall6(192 /* __NR_mmap2 */, 0, pol_size, 1 /* PROT_READ */, 2 /* MAP_PRIVATE */, sp_fd, 0);
    if ((unsigned long)pol_map >= (unsigned long)-4095L) {
        print_str("[-] FAILED: mmap sepolicy, rc=");
        print_dec((long)pol_map);
        print_str("\n");
        my_syscall1(6 /* __NR_close */, sp_fd);
        return;
    }

    long load_fd = my_syscall3(5 /* __NR_open */, (long)str_selinux_load, 1 /* O_WRONLY */, 0);
    if (load_fd >= 0) {
        long wr = my_syscall3(4 /* __NR_write */, load_fd, (long)pol_map, pol_size);
        my_syscall1(6 /* __NR_close */, load_fd);
        if (wr == pol_size) {
            print_str("[+] Loaded precompiled_sepolicy into /sys/fs/selinux/load (");
            print_dec(pol_size);
            print_str(" bytes)\n");
        } else {
            print_str("[-] FAILED: write /sys/fs/selinux/load, rc=");
            print_dec(wr);
            print_str("\n");
        }
    } else {
        print_str("[-] FAILED: open /sys/fs/selinux/load, rc=");
        print_dec(load_fd);
        print_str("\n");
    }

    my_syscall2(91 /* __NR_munmap */, (long)pol_map, pol_size);
    my_syscall1(6 /* __NR_close */, sp_fd);

    /* Enforce permissive mode */
    long enf_fd = my_syscall3(5 /* __NR_open */, (long)str_selinux_enforce, 1 /* O_WRONLY */, 0);
    if (enf_fd >= 0) {
        long wr = my_syscall3(4 /* __NR_write */, enf_fd, (long)"0", 1);
        my_syscall1(6 /* __NR_close */, enf_fd);
        if (wr == 1) {
            print_str("[+] Switched SELinux to permissive mode (/sys/fs/selinux/enforce = 0)\n");
        } else {
            print_str("[-] FAILED: write /sys/fs/selinux/enforce, rc=");
            print_dec(wr);
            print_str("\n");
        }
    } else {
        print_str("[-] FAILED: open /sys/fs/selinux/enforce, rc=");
        print_dec(enf_fd);
        print_str("\n");
    }
}

static const unsigned char default_panel_param[140] = {
    0x05, 0x30, 0x96, 0x00, 0x32, 0x00, 0x3c, 0x00, 0x20, 0xee, 0xd9, 0x08, 0x98, 0x08, 0x65, 0x04,
    0x2c, 0x05, 0x94, 0x00, 0x14, 0x08, 0x24, 0x00, 0x5c, 0x04, 0x00, 0x3c, 0xc0, 0x21, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0xc8, 0x00, 0xc8, 0x00, 0x28, 0x00, 0xe8, 0x03, 0x10, 0x27,
    0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb9, 0x04, 0xa8, 0x02, 0x78, 0x00, 0x64, 0x00, 0x78, 0x00, 'T',  'C',  'L',  '_',  'R',  'T',
    '4',  '1',  '_',  'F',  'H',  'D',  '_',  'P',  'A',  'N',  'E',  'L',  0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void setup_panel_parameter(void) {
    static const char str_panel_file[] = "/tmp/panel_parameter";
    static const char str_sys_panel1[] = "/sys/realtek_boards/panel_parameter";
    static const char str_sys_panel2[] = "/sys/devices/platform/rtd284x_boards/panel_parameter";

    long fd = my_syscall3(5 /* __NR_open */, (long)str_panel_file, 0101 /* O_WRONLY|O_CREAT|O_TRUNC */, 0644);
    if (fd < 0) {
        print_str("[-] FAILED: open /tmp/panel_parameter, rc=");
        print_dec(fd);
        print_str("\n");
        return;
    }
    long wr = my_syscall3(4 /* __NR_write */, fd, (long)default_panel_param, sizeof(default_panel_param));
    my_syscall1(6 /* __NR_close */, fd);
    if (wr != sizeof(default_panel_param)) {
        print_str("[-] FAILED: write /tmp/panel_parameter, rc=");
        print_dec(wr);
        print_str("\n");
        return;
    }

    long m1 = my_syscall5(21 /* __NR_mount */, (long)str_panel_file, (long)str_sys_panel1, 0, 4096 /* MS_BIND */, 0);
    if (m1 == 0) {
        print_str("[+] Bound synthetic panel_parameter -> /sys/realtek_boards/panel_parameter\n");
    } else {
        print_str("[-] WARNING: mount bind /sys/realtek_boards/panel_parameter, rc=");
        print_dec(m1);
        print_str("\n");
    }

    long m2 = my_syscall5(21 /* __NR_mount */, (long)str_panel_file, (long)str_sys_panel2, 0, 4096 /* MS_BIND */, 0);
    if (m2 == 0) {
        print_str("[+] Bound synthetic panel_parameter -> /sys/devices/platform/rtd284x_boards/panel_parameter\n");
    }
}

static void setup_software_egl(void) {
    static const char str_gles_sw[]      = "/system/lib/egl/libGLES_software.so";
    static const char str_gles_android[] = "/system/lib/egl/libGLES_android.so";
    static const char str_gles_mali[]    = "/vendor/lib/egl/libGLES_mali.so";
    static const char str_gles_vendor[]  = "/vendor/lib/egl/libGLES.so";

    const char *src = str_gles_android;
    long fd = my_syscall3(5 /* __NR_open */, (long)str_gles_sw, 0 /* O_RDONLY */, 0);
    if (fd >= 0) {
        my_syscall1(6 /* __NR_close */, fd);
        src = str_gles_sw;
        print_str("[+] Found custom software GLES 2.0 driver at /system/lib/egl/libGLES_software.so\n");
    }

    long m1 = my_syscall5(21 /* __NR_mount */, (long)src, (long)str_gles_mali, 0, 4096 /* MS_BIND */, 0);
    if (m1 == 0) {
        print_str("[+] Bound software GLES -> /vendor/lib/egl/libGLES_mali.so\n");
    } else {
        print_str("[-] WARNING: mount bind libGLES_mali.so, rc=");
        print_dec(m1);
        print_str("\n");
    }

    long m2 = my_syscall5(21 /* __NR_mount */, (long)src, (long)str_gles_vendor, 0, 4096 /* MS_BIND */, 0);
    if (m2 == 0) {
        print_str("[+] Bound software GLES -> /vendor/lib/egl/libGLES.so\n");
    }

    long m3 = my_syscall5(21 /* __NR_mount */, (long)src, (long)str_gles_android, 0, 4096 /* MS_BIND */, 0);
    if (m3 == 0) {
        print_str("[+] Bound software GLES -> /system/lib/egl/libGLES_android.so\n");
    }
}

static void disable_vendor_hwc(void) {
    static const char str_hwc_video[]     = "/vendor/lib/hw/hwcomposer.video.so";
    static const char str_hwc_video_dis[] = "/vendor/lib/hw/hwcomposer.video.so.disabled";
    static const char str_hwc_rtd[]       = "/vendor/lib/hw/hwcomposer.rtd284x.so";
    static const char str_hwc_rtd_dis[]   = "/vendor/lib/hw/hwcomposer.rtd284x.so.disabled";
    static const char str_hwc_grp[]       = "/vendor/lib/hw/hwcomposer.graphic.rtd284x.so";
    static const char str_hwc_grp_dis[]   = "/vendor/lib/hw/hwcomposer.graphic.rtd284x.so.disabled";

    long r1 = my_syscall2(38 /* __NR_rename */, (long)str_hwc_video, (long)str_hwc_video_dis);
    long r2 = my_syscall2(38 /* __NR_rename */, (long)str_hwc_rtd, (long)str_hwc_rtd_dis);
    long r3 = my_syscall2(38 /* __NR_rename */, (long)str_hwc_grp, (long)str_hwc_grp_dis);
    if (r1 == 0 || r2 == 0) {
        print_str("[+] Disabled vendor HWC modules to trigger HWC2OnFbAdapter fallback\n");
    } else {
        print_str("[-] WARNING: rename vendor HWC modules failed (r1=");
        print_dec(r1);
        print_str(", r2=");
        print_dec(r2);
        print_str(", r3=");
        print_dec(r3);
        print_str("), binding non-ELF dummy over vendor HWC\n");
        static const char str_dummy[] = "/tmp/panel_parameter";
        my_syscall5(21 /* __NR_mount */, (long)str_dummy, (long)str_hwc_video, 0, 4096 /* MS_BIND */, 0);
        my_syscall5(21 /* __NR_mount */, (long)str_dummy, (long)str_hwc_rtd, 0, 4096 /* MS_BIND */, 0);
        my_syscall5(21 /* __NR_mount */, (long)str_dummy, (long)str_hwc_grp, 0, 4096 /* MS_BIND */, 0);
    }
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
        if (tty > 2) {
            my_syscall1(6 /* __NR_close */, tty);
        }
    }

    my_syscall3(4 /* __NR_write */, 1, (long)banner, sizeof(banner) - 1);

    /* 2. Mount /proc and /sys */
    int r_proc = check_mount(str_proc, str_proc_path, str_proc, 0, 0);
    int r_sys  = check_mount(str_sysfs, str_sys_path, str_sysfs, 0, 0);
    if (r_proc == 0 && r_sys == 0) {
        my_syscall3(4, 1, (long)msg_mount_proc, sizeof(msg_mount_proc) - 1);
    } else {
        print_str("[-] ABORT: Critical system mounts (/proc, /sys) failed.\n");
        while (1) {
            my_syscall3(29 /* __NR_pause */, 0, 0, 0);
        }
    }

    static const char str_selinuxfs[]   = "selinuxfs";
    static const char str_selinux_path[]= "/sys/fs/selinux";
    my_syscall2(39 /* __NR_mkdir */, (long)str_selinux_path, 0755);
    check_mount(str_selinuxfs, str_selinux_path, str_selinuxfs, 0, 0);

    /* 3. Mount /dev/pts */
    my_syscall2(39 /* __NR_mkdir */, (long)str_devpts_path, 0755);
    int r_pts = check_mount(str_devpts, str_devpts_path, str_devpts, 0, 0);
    if (r_pts != 0) {
        print_str("[-] WARNING: /dev/pts mount failed\n");
    }

    /* 4. Mount /vendor from partition 2 */
    my_syscall2(39 /* __NR_mkdir */, (long)str_vendor_path, 0755);
    int r_vendor = check_mount(str_vendor_dev, str_vendor_path, str_ext4, 0, 0);
    if (r_vendor == 0) {
        my_syscall3(4, 1, (long)msg_mount_vendor, sizeof(msg_mount_vendor) - 1);

        /* Set up canonical /system mount hierarchy for Android Bionic and Zygote FD whitelist:
         * Partition 1 is mounted at root '/', but Android 9 Zygote (fd_utils.cpp)
         * strictly whitelists open bootclasspath jars matching prefix "/system/framework/".
         * Unlink legacy symlink /system -> '.', create /system mountpoint directory,
         * and bind-mount '/' onto '/system'. Any file opened via /system/... resolves to that prefix.
         */
        static const char str_system_path[] = "/system";
        static const char str_root_path[]   = "/";
        long r_un = my_syscall1(10 /* __NR_unlink */, (long)str_system_path);
        if (r_un != -21 /* -EISDIR */) {
            my_syscall2(39 /* __NR_mkdir */, (long)str_system_path, 0755);
        }
        long r_sys_bind = my_syscall5(21 /* __NR_mount */, (long)str_root_path, (long)str_system_path, 0, 4096 /* MS_BIND */, 0);
        if (r_sys_bind == 0) {
            print_str("[+] Bound rootfs onto /system for Zygote path whitelist\n");
        } else {
            print_str("[-] WARNING: Failed to bind-mount / onto /system, rc=");
            print_dec(r_sys_bind);
            print_str("\n");
        }

        /* Bind /vendor onto /system/vendor */
        static const char str_sys_vendor[] = "/system/vendor";
        my_syscall2(39 /* __NR_mkdir */, (long)str_sys_vendor, 0755);
        my_syscall5(21 /* __NR_mount */, (long)str_vendor_path, (long)str_sys_vendor, 0, 4096 /* MS_BIND */, 0);

        /* Load genuine precompiled SELinux policy from vendor partition */
        load_selinux_policy();

        /* Bind mount software GLES implementation over non-emulated Mali libraries */
        setup_software_egl();

        /* Disable non-functional vendor HWC modules to trigger AOSP HWC2OnFbAdapter fallback */
        disable_vendor_hwc();
    }

    /* 5. Mount ext4 on /data from /dev/mmcblk0p3, fallback to tmpfs */
    my_syscall2(39 /* __NR_mkdir */, (long)str_data_path, 0771);
    int r_data = check_mount(str_data_dev, str_data_path, str_ext4, 0, 0);
    if (r_data == 0) {
        my_syscall3(4, 1, (long)msg_mount_data, sizeof(msg_mount_data) - 1);
    } else {
        print_str("[-] WARNING: /data mount on /dev/mmcblk0p3 failed, falling back to tmpfs\n");
        check_mount(str_tmpfs, str_data_path, str_tmpfs, 0, 0);
    }

    my_syscall2(39, (long)str_cache_path, 0770);
    int r_cache = check_mount(str_tmpfs, str_cache_path, str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_tmp_path, 0777);
    int r_tmp = check_mount(str_tmpfs, str_tmp_path, str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_mnt_path, 0755);
    int r_mnt = check_mount(str_tmpfs, str_mnt_path, str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_storage_path, 0755);
    int r_storage = check_mount(str_tmpfs, str_storage_path, str_tmpfs, 0, 0);

    if (r_cache == 0 && r_tmp == 0 && r_mnt == 0 && r_storage == 0) {
        my_syscall3(4, 1, (long)msg_mount_tmpfs, sizeof(msg_mount_tmpfs) - 1);
    }

    /* Set up synthetic panel parameter in sysfs for librtk_hal_panel and HWC */
    setup_panel_parameter();

    /* 6. Set up Android compatibility symlinks */
    my_syscall2(83 /* __NR_symlink */, (long)str_system_etc, (long)str_etc);
    my_syscall2(83 /* __NR_symlink */, (long)str_vendor_odm, (long)str_odm);
    my_syscall2(83 /* __NR_symlink */, (long)str_system_prod, (long)str_product);

    static const char str_ld_vndk_lite[] = "/system/etc/ld.config.vndk_lite.txt";
    static const char str_ld_config[]    = "/system/etc/ld.config.txt";
    my_syscall2(83 /* __NR_symlink */, (long)str_ld_vndk_lite, (long)str_ld_config);

    /* 7. Setup device nodes and permissions */
    setup_binder_node(str_binder, 60);
    setup_binder_node(str_hwbinder, 59);
    setup_binder_node(str_vndbinder, 58);
    my_syscall2(15 /* __NR_chmod */, (long)str_urandom, 0666);

    /* Graphics and shared memory nodes for SurfaceFlinger & HWC */
    static const char str_graphics_dir[] = "/dev/graphics";
    static const char str_graphics_fb0[] = "/dev/graphics/fb0";
    static const char str_ashmem[]       = "/dev/ashmem";
    static const char str_sw_sync[]      = "/dev/sw_sync";

    my_syscall2(39 /* __NR_mkdir */, (long)str_graphics_dir, 0755);
    my_syscall2(83 /* __NR_symlink */, (long)str_fb_path, (long)str_graphics_fb0);
    my_syscall2(15 /* __NR_chmod */, (long)str_fb_path, 0666);
    my_syscall2(15 /* __NR_chmod */, (long)str_ashmem, 0666);
    my_syscall2(15 /* __NR_chmod */, (long)str_sw_sync, 0666);

    /* 8. Prepare directory hierarchies */
    static const char str_xbin_su[] = "/xbin/su";
    my_syscall2(39 /* __NR_mkdir */, (long)str_xbin_path, 0755);
    long r_chown = my_syscall3(212 /* __NR_chown32 */, (long)str_su_path, 0, 0 /* root:root */);
    if (r_chown == 0) {
        long r_chmod = my_syscall2(15 /* __NR_chmod */, (long)str_su_path, 06755);
        if (r_chmod != 0) {
            print_str("[-] FAILED: chmod /system/xbin/su, rc=");
            print_dec(r_chmod);
            print_str("\n");
        }
    } else if (r_chown != -2 /* -ENOENT */) {
        print_str("[-] FAILED: chown /system/xbin/su, rc=");
        print_dec(r_chown);
        print_str("\n");
    }
    r_chown = my_syscall3(212 /* __NR_chown32 */, (long)str_xbin_su, 0, 0 /* root:root */);
    if (r_chown == 0) {
        long r_chmod = my_syscall2(15 /* __NR_chmod */, (long)str_xbin_su, 06755);
        if (r_chmod != 0) {
            print_str("[-] FAILED: chmod /xbin/su, rc=");
            print_dec(r_chmod);
            print_str("\n");
        }
    } else if (r_chown != -2 /* -ENOENT */) {
        print_str("[-] FAILED: chown /xbin/su, rc=");
        print_dec(r_chown);
        print_str("\n");
    }

    my_syscall2(39 /* __NR_mkdir */, (long)str_misc_path, 0771);
    my_syscall2(39 /* __NR_mkdir */, (long)str_adb_path, 0770);
    my_syscall3(212 /* __NR_chown32 */, (long)str_adb_path, 2000, 2000);
    sync_adb_key();

    my_syscall2(39 /* __NR_mkdir */, (long)str_local_path, 0771);
    my_syscall2(39 /* __NR_mkdir */, (long)str_tmp_local_path, 0777);
    my_syscall2(15 /* __NR_chmod */, (long)str_tmp_local_path, 0777);

    /* 9. Initialize Android Property Area & Property Service */
    void *prop_area = init_properties_area();
    if (prop_area) {
        int prop_pid = start_property_service(prop_area);
        if (prop_pid > 0) {
            print_str("[*] property_service child spawned, pid=");
            print_dec(prop_pid);
            print_str("\n");
        }
    }

    /* 10. Spawn Context Managers */
    char *sm_argv[] = {(char *)str_servicemanager, 0};
    int sm_pid = spawn_service_manager("servicemanager", str_servicemanager, sm_argv);
    if (sm_pid > 0) {
        print_str("[*] servicemanager child spawned, pid=");
        print_dec(sm_pid);
        print_str("; Binder readiness requires a client probe\n");
    }

    char *hwsm_argv[] = {(char *)str_hwservicemanager, 0};
    int hwsm_pid = spawn_service_manager("hwservicemanager", str_hwservicemanager, hwsm_argv);
    if (hwsm_pid > 0) {
        print_str("[*] hwservicemanager child spawned, pid=");
        print_dec(hwsm_pid);
        print_str("; HwBinder readiness requires a client probe\n");
    }

    char *vndsm_argv[] = {(char *)str_vndservicemanager, (char *)str_vndbinder, 0};
    int vndsm_pid = spawn_service_manager("vndservicemanager", str_vndservicemanager, vndsm_argv);
    if (vndsm_pid > 0) {
        print_str("[*] vndservicemanager child spawned, pid=");
        print_dec(vndsm_pid);
        print_str("; VndBinder readiness requires a client probe\n");
    }

    /* 7. Initialize Network Subsystem (eth0, DHCP, DNS, Live Outbound Tests) */
    init_network_and_verify();

    /* adbd is ARM32 static; its authenticated TCP listener is checked by ADB. */
    static const char str_adbd[] = "/system/bin/adbd";
    static char *adbd_argv[] = {(char *)str_adbd, 0};
    int adbd_pid = run_process(str_adbd, adbd_argv, 0);
    if (adbd_pid > 0) {
        print_str("[*] adbd child spawned, pid=");
        print_dec(adbd_pid);
        print_str("; TCP and authentication require an ADB probe\n");
    }

    if (is_android_mode()) {
        print_str("[+] Boot mode: Android Userspace (rendering boot splash display)\n");
        paint_tcl_tv_display();
        prepare_android_runtime_dirs();
        if (r_mnt != 0 || r_storage != 0 || prepare_android_storage() != 0) {
            print_str("[-] Android startup stopped: storage setup failed\n");
            /* Keep PID 1 alive and ADB available for diagnosis. */
            while (1) {
                int status = 0;
                long child = my_syscall4(114 /* wait4 */, -1,
                                         (long)&status, 0, 0);
                if (child < 0 && child != -4 /* EINTR */) {
                    my_sleep_ms(1000);
                }
            }
        }

        /* Spawn logd and tombstoned early so logging and crash reporting work */
        spawn_logd();
        spawn_tombstoned();

        /* These stock HALs unblock system_server bootstrap. Their hardware
         * operations remain vendor code; registration is checked at runtime.
         */
        int memtrack_pid = spawn_bootstrap_hal(
            "/vendor/bin/hw/android.hardware.memtrack@1.0-service", 0);
        int power_pid = spawn_bootstrap_hal(
            "/vendor/bin/hw/android.hardware.power@1.0-service", 1);
        int light_pid = spawn_bootstrap_hal(
            "/vendor/bin/hw/android.hardware.light@2.0-service", 0);

        /* Spawn Graphics HALs */
        static const char str_configstore[] = "/vendor/bin/hw/android.hardware.configstore@1.1-service";
        static char *cs_argv[] = {(char *)str_configstore, 0};
        int cs_pid = run_process(str_configstore, cs_argv, 0);
        if (cs_pid > 0) {
            print_str("[*] configstore HAL spawned, pid=");
            print_dec(cs_pid);
            print_str("\n");
        }

        static const char str_allocator[] = "/vendor/bin/hw/android.hardware.graphics.allocator@2.0-service";
        static char *alloc_argv[] = {(char *)str_allocator, 0};
        int alloc_pid = run_process(str_allocator, alloc_argv, 0);
        if (alloc_pid > 0) {
            print_str("[*] graphics.allocator HAL spawned, pid=");
            print_dec(alloc_pid);
            print_str("\n");
        }

        static const char str_composer[] = "/vendor/bin/hw/android.hardware.graphics.composer@2.1-service";
        static char *comp_argv[] = {(char *)str_composer, 0};
        int comp_pid = run_process(str_composer, comp_argv, 0);
        if (comp_pid > 0) {
            print_str("[*] graphics.composer HAL spawned, pid=");
            print_dec(comp_pid);
            print_str("\n");
        }

        /* Allow graphics HALs to initialize IPC before SurfaceFlinger starts */
        my_sleep_ms(1500);

        /* Spawn SurfaceFlinger */
        static const char str_surfaceflinger[] = "/system/bin/surfaceflinger";
        static char *sf_argv[] = {(char *)str_surfaceflinger, 0};
        int sf_pid = run_process(str_surfaceflinger, sf_argv, 0);
        if (sf_pid > 0) {
            print_str("[*] SurfaceFlinger spawned, pid=");
            print_dec(sf_pid);
            print_str("\n");
        }

        /* Allow Binder IPC endpoints to settle before installd */
        my_sleep_ms(1000);

        /* Spawn installd */
        spawn_installd();

        /* Allow installd socket and environment to settle before zygote */
        my_sleep_ms(1000);

        /* A fork failure is fatal to framework bootstrap, while the existing
         * ADB/graphics services remain available for diagnosis. Child exec
         * failures are reported explicitly and reaped below.
         */
        if (memtrack_pid > 0 && power_pid > 0 && light_pid > 0) {
            int zygote_pid = spawn_zygote();
            if (zygote_pid < 0) {
                print_str("[-] Android framework not started: Zygote failed\n");
            }
        } else {
            print_str("[-] Android framework not started: HAL fork failed\n");
        }

        /* Enter PID 1 child reaping loop:
         * Blocking wait4(-1, (long)&status, 0, 0) wakes up whenever any child exits.
         * Reaps the child immediately, preventing zombie accumulation, and logs exit/signal.
         */
        while (1) {
            int status = 0;
            long r = my_syscall4(114 /* __NR_wait4 */, -1, (long)&status, 0, 0);
            if (r > 0) {
                print_str("[*] PID 1 reaped child pid=");
                print_dec(r);
                if (WIFEXITED(status)) {
                    print_str(", exit status=");
                    print_dec(WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    print_str(", killed by signal=");
                    print_dec(WTERMSIG(status));
                }
                print_str("\n");
            }
        }
    } else {
        /* Diagnostic Mode: Take over /dev/fb0 with high-definition TCL Smart TV Dashboard */
        paint_tcl_tv_display();
        my_syscall3(4, 1, (long)msg_paint_fb, sizeof(msg_paint_fb) - 1);

        /* Run interactive TV remote control session (receives Cocoa window keys) */
        static const char str_fb_test[] = "/bin/fb_test";
        static const char str_loop[] = "--loop";
        static char *fb_argv[] = {(char*)str_fb_test, (char*)str_loop, 0};

        long pid = my_syscall0(2 /* __NR_fork */);
        if (pid == 0) {
            my_syscall3(11 /* __NR_execve */, (long)str_fb_test, (long)fb_argv, (long)envp);
            my_syscall1(1 /* __NR_exit */, 127);
        }
        if (pid > 0) {
            int status = 0;
            long ret;
            do {
                ret = my_syscall4(114 /* __NR_wait4 */, pid, (long)&status, 0, 0);
            } while (ret == -4 /* -EINTR */);
            if (ret < 0) {
                print_str("[-] FAILED: wait4 failed for fb_test, rc=");
                print_dec(ret);
                print_str("\n");
            } else if (WIFEXITED(status)) {
                int code = WEXITSTATUS(status);
                if (code != 0) {
                    print_str("[-] fb_test process exited with status ");
                    print_dec(code);
                    print_str("\n");
                } else {
                    print_str("[+] fb_test process exited cleanly (status 0)\n");
                }
            } else if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                print_str("[-] fb_test process killed by signal ");
                print_dec(sig);
                print_str("\n");
            }
        } else if (pid < 0) {
            print_str("[-] FAILED: fork failed for fb_test, rc=");
            print_dec(pid);
            print_str("\n");
        }

        /* Launch interactive root shell on exit */
        my_syscall3(4, 1, (long)msg_exec_sh, sizeof(msg_exec_sh) - 1);
        my_syscall3(11 /* __NR_execve */, (long)str_sh, (long)argv, (long)envp);

        while (1) {
            my_syscall3(29 /* __NR_pause */, 0, 0, 0);
        }
    }
}
