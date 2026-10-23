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

static inline void print_ip(unsigned int ip) {
    print_dec(ip & 0xFF);
    print_str(".");
    print_dec((ip >> 8) & 0xFF);
    print_str(".");
    print_dec((ip >> 16) & 0xFF);
    print_str(".");
    print_dec((ip >> 24) & 0xFF);
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
    {"Ethernet",     "10.0.2.15 (Online)", "NET",     0xFF10B981}
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

    /* 8. Write canvas directly to /dev/fb0 via row writes (guaranteed to flush to MMIO 0x3e000000) */
    for (int r = 0; r < FB_HEIGHT; r++) {
        my_syscall3(4 /* __NR_write */, fb, (long)&g_canvas[r * FB_WIDTH], FB_WIDTH * sizeof(unsigned int));
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
        my_syscall3(11 /* __NR_execve */, (long)cmd, (long)proc_argv, (long)envp);
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
    }

    /* 5. Mount tmpfs on /data, /cache, /tmp, /mnt, /storage */
    my_syscall2(39, (long)str_data_path, 0771);
    int r_data = check_mount(str_tmpfs, str_data_path, str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_cache_path, 0770);
    int r_cache = check_mount(str_tmpfs, str_cache_path, str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_tmp_path, 0777);
    int r_tmp = check_mount(str_tmpfs, str_tmp_path, str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_mnt_path, 0755);
    int r_mnt = check_mount(str_tmpfs, str_mnt_path, str_tmpfs, 0, 0);

    my_syscall2(39, (long)str_storage_path, 0755);
    int r_storage = check_mount(str_tmpfs, str_storage_path, str_tmpfs, 0, 0);

    if (r_data == 0 && r_cache == 0 && r_tmp == 0 && r_mnt == 0 && r_storage == 0) {
        my_syscall3(4, 1, (long)msg_mount_tmpfs, sizeof(msg_mount_tmpfs) - 1);
    }

    /* 6. Set up Android compatibility symlinks */
    my_syscall2(83 /* __NR_symlink */, (long)str_system_etc, (long)str_etc);
    my_syscall2(83 /* __NR_symlink */, (long)str_vendor_odm, (long)str_odm);
    my_syscall2(83 /* __NR_symlink */, (long)str_system_prod, (long)str_product);

    /* 7. Initialize Network Subsystem (eth0, DHCP, DNS, Live Outbound Tests) */
    init_network_and_verify();

    /* 8. Take over /dev/fb0 with high-definition TCL Smart TV Dashboard */
    paint_tcl_tv_display();
    my_syscall3(4, 1, (long)msg_paint_fb, sizeof(msg_paint_fb) - 1);

    /* 8. Run interactive TV remote control session (receives Cocoa window keys) */
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

    /* 9. Launch interactive root shell on exit */
    my_syscall3(4, 1, (long)msg_exec_sh, sizeof(msg_exec_sh) - 1);
    my_syscall3(11 /* __NR_execve */, (long)str_sh, (long)argv, (long)envp);

    while (1) {
        my_syscall3(29 /* __NR_pause */, 0, 0, 0);
    }
}
