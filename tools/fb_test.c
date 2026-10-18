#include "font8x16.h"

/* ARM Linux System Call Wrappers (no libc / no CRT dependency) */
static inline long sys_open(const char *path, long flags, long mode) {
    register long r7 __asm__("r7") = 5; /* __NR_open */
    register long r0 __asm__("r0") = (long)path;
    register long r1 __asm__("r1") = flags;
    register long r2 __asm__("r2") = mode;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline long sys_close(long fd) {
    register long r7 __asm__("r7") = 6; /* __NR_close */
    register long r0 __asm__("r0") = fd;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0) : "memory");
    return r0;
}

static inline long sys_read(long fd, void *buf, unsigned long count) {
    register long r7 __asm__("r7") = 3; /* __NR_read */
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = count;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline long sys_write(long fd, const void *buf, unsigned long count) {
    register long r7 __asm__("r7") = 4; /* __NR_write */
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = count;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline long sys_lseek(long fd, long offset, long whence) {
    register long r7 __asm__("r7") = 19; /* __NR_lseek */
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = offset;
    register long r2 __asm__("r2") = whence;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline void sys_exit(long status) {
    register long r7 __asm__("r7") = 1; /* __NR_exit */
    register long r0 __asm__("r0") = status;
    __asm__ volatile("swi #0" : : "r"(r7), "r"(r0));
    while (1) {}
}

#define FB_WIDTH  1920
#define FB_HEIGHT 1080
#define FB_SIZE   (FB_WIDTH * FB_HEIGHT * 4)

/* Colors (32-bit ARGB) */
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

static unsigned int g_canvas[FB_WIDTH * FB_HEIGHT];
static long g_fb_fd = -1;

static int my_strlen(const char *s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static void print_str(const char *s) {
    sys_write(1, s, my_strlen(s));
}

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
    fill_rect(x, y, w, border_w, color);                         /* Top */
    fill_rect(x, y + h - border_w, w, border_w, color);         /* Bottom */
    fill_rect(x, y, border_w, h, color);                         /* Left */
    fill_rect(x + w - border_w, y, border_w, h, color);         /* Right */
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

struct CardInfo {
    const char *title;
    const char *sub;
    const char *badge;
    unsigned int icon_color;
};

static const struct CardInfo cards[6] = {
    {"TV / DTV",     "Channel 01 - 1080p", "LIVE",    0xFFE50914},
    {"HDMI 1",       "Active 60Hz Input",  "INPUT",   0xFF38BDF8},
    {"HDMI 2",       "Standby / No Link",  "INPUT",   0xFF64748B},
    {"Android Apps", "TCL CyberUI Store",  "SMART",   0xFF10B981},
    {"Media Center", "USB / eMMC Storage", "LOCAL",   0xFFF59E0B},
    {"Settings",     "Network / System",   "CONFIG",  0xFF8B5CF6}
};

static void flush_canvas(void) {
    sys_lseek(g_fb_fd, 0, 0);
    for (int r = 0; r < FB_HEIGHT; r++) {
        sys_write(g_fb_fd, &g_canvas[r * FB_WIDTH], FB_WIDTH * sizeof(unsigned int));
    }
}

void render_tcl_dashboard(int selected_index) {
    /* 1. Background Canvas */
    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_DARK_BG);

    /* 2. Top Header Navigation Bar */
    fill_rect(0, 0, FB_WIDTH, 70, 0xFF0B0E14);
    fill_rect(0, 68, FB_WIDTH, 2, 0xFF1F2937);

    fill_rect(50, 18, 70, 34, COLOR_ACCENT_RED);
    draw_string("TCL", 62, 22, 2, COLOR_WHITE, 0);

    draw_string("AI-IN ANDROID TV", 135, 20, 2, COLOR_WHITE, 0);
    draw_string("CHASSIS RT41 | BOARD tv030 | REALTEK RTD2841", 135, 42, 1, COLOR_TEXT_MUTED, 0);

    draw_string("EMULATOR ACTIVE [QEMU AArch32]", 1360, 26, 1, COLOR_GREEN, 0);
    draw_string("12:00 PM", 1750, 22, 2, COLOR_WHITE, 0);

    /* 3. Hero Section / Banner */
    fill_rect(50, 100, FB_WIDTH - 100, 170, 0xFF141923);
    draw_rect_border(50, 100, FB_WIDTH - 100, 170, 1, 0xFF2D3748);

    draw_string("TCL SMART TV FULL-SYSTEM FIRMWARE EMULATION", 80, 120, 2, COLOR_SEL_BORDER, 0);
    draw_string("Target Architecture: Quad-Core ARM Cortex-A53 (AArch32) | GIC-400 | SMP Enabled", 80, 160, 1, COLOR_WHITE, 0);
    draw_string("Linux Kernel: Linux 4.4.3-rtk-qemu | Display Surface: Simple-Framebuffer 1920x1080@32bpp", 80, 180, 1, COLOR_TEXT_MUTED, 0);
    draw_string("Storage Engine: 4.00 GiB eMMC GPT | Partitions: /dev/mmcblk0p1 (system), /dev/mmcblk0p2 (vendor)", 80, 200, 1, COLOR_TEXT_MUTED, 0);
    draw_string("Graphics Pipeline: Software EGL (libGLES_android.so) | Dumb Buffer FB: /dev/fb0", 80, 220, 1, COLOR_GOLD, 0);

    /* 4. Horizontal Card Deck (Inputs & Apps) */
    int card_w = 270;
    int card_h = 280;
    int card_spacing = 30;
    int start_x = 55;
    int start_y = 310;

    for (int i = 0; i < 6; i++) {
        int cx = start_x + i * (card_w + card_spacing);
        int is_sel = (i == selected_index);

        unsigned int bg_color = is_sel ? COLOR_SEL_BG : COLOR_CARD_BG;
        unsigned int border_color = is_sel ? COLOR_SEL_BORDER : COLOR_CARD_BORDER;
        int border_thickness = is_sel ? 3 : 1;

        fill_rect(cx, start_y, card_w, card_h, bg_color);
        draw_rect_border(cx, start_y, card_w, card_h, border_thickness, border_color);

        fill_rect(cx + 20, start_y + 20, 60, 60, cards[i].icon_color);
        draw_rect_border(cx + 20, start_y + 20, 60, 60, 1, COLOR_WHITE);

        draw_string(cards[i].badge, cx + card_w - 75, start_y + 20, 1, is_sel ? COLOR_GOLD : COLOR_TEXT_DIM, 0);
        draw_string(cards[i].title, cx + 20, start_y + 110, 2, COLOR_WHITE, 0);
        draw_string(cards[i].sub, cx + 20, start_y + 155, 1, is_sel ? COLOR_SEL_BORDER : COLOR_TEXT_MUTED, 0);

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
    draw_string("[+] GIC Interrupts:    GIC-400 v2 | Translation domain mapped (SPI 18 SDHCI, SPI 25 UART)", 80, info_y + 85, 1, COLOR_GREEN, 0);
    draw_string("[+] Display Pipeline:  Simple-Framebuffer MMIO base 0x3e000000 (1920x1080 @ 32bpp ARGB8888)", 80, info_y + 110, 1, COLOR_GREEN, 0);
    draw_string("[+] Storage Gateway:   Realtek generic-sdhci MMIO 0x18010800 -> /dev/mmcblk0 (4.00 GiB GPT)", 80, info_y + 135, 1, COLOR_GREEN, 0);
    draw_string("[+] Userspace Shell:   Bionic /system/bin/sh active as root (UID 0, GID 0)", 80, info_y + 160, 1, COLOR_GREEN, 0);
    draw_string("[+] Remote Control:    Interactive Keypad Helper: scripts/send_key.py / sendevent input mapping", 80, info_y + 185, 1, COLOR_GOLD, 0);
    draw_string("[*] Note: Cocoa window is video output only. Enter commands via the terminal running QEMU.", 80, info_y + 210, 1, COLOR_TEXT_MUTED, 0);

    /* 6. Control Guidance Footer */
    int foot_y = 900;
    fill_rect(50, foot_y, FB_WIDTH - 100, 60, 0xFF0B0E14);
    draw_rect_border(50, foot_y, FB_WIDTH - 100, 60, 1, 0xFF1F2937);

    draw_string("REMOTE CONTROL KEYS:  [< / >] Select Input    [OK / ENTER] Switch Input    [1-6] Direct Card Jump    [Q] Exit To Shell", 110, foot_y + 22, 1, COLOR_SEL_BORDER, 0);

    /* 7. SMPTE 8-Color Calibration Bars */
    draw_smpte_bars(990, 70);
    fill_rect(0, 1060, FB_WIDTH, 20, COLOR_BLACK);

    flush_canvas();
}

int my_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

int parse_card_arg(const char *s) {
    if (s && s[0] >= '0' && s[0] <= '5') {
        return s[0] - '0';
    }
    return 0;
}

void main_entry(unsigned long *sp) {
    int argc = (int)sp[0];
    char **argv = (char**)&sp[1];

    const char *dev_fb = "/dev/fb0";
    g_fb_fd = sys_open(dev_fb, 2 /* O_RDWR */, 0);
    if (g_fb_fd < 0) {
        print_str("[-] Error: Cannot open /dev/fb0\n");
        sys_exit(1);
    }

    int selected = 0;
    int interactive = 0;

    if (argc > 1) {
        if (my_strcmp(argv[1], "--clear") == 0) {
            fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BLACK);
            flush_canvas();
            print_str("[+] Framebuffer cleared to black.\n");
            sys_close(g_fb_fd);
            sys_exit(0);
        } else if (my_strcmp(argv[1], "--bars") == 0) {
            fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BLACK);
            draw_smpte_bars(0, FB_HEIGHT);
            flush_canvas();
            print_str("[+] SMPTE 1080p full-screen bars rendered.\n");
            sys_close(g_fb_fd);
            sys_exit(0);
        } else if (my_strcmp(argv[1], "--card") == 0 && argc > 2) {
            selected = parse_card_arg(argv[2]);
            render_tcl_dashboard(selected);
            print_str("[+] TCL Dashboard rendered with selected card: ");
            print_str(cards[selected].title);
            print_str("\n");
            sys_close(g_fb_fd);
            sys_exit(0);
        } else if (my_strcmp(argv[1], "--loop") == 0 || my_strcmp(argv[1], "-i") == 0) {
            interactive = 1;
        } else {
            render_tcl_dashboard(0);
            print_str("[+] TCL Smart TV Dashboard rendered to /dev/fb0 successfully.\n");
            sys_close(g_fb_fd);
            sys_exit(0);
        }
    } else {
        render_tcl_dashboard(0);
        print_str("[+] TCL Smart TV Dashboard rendered to /dev/fb0 successfully.\n");
        sys_close(g_fb_fd);
        sys_exit(0);
    }

    if (interactive) {
        render_tcl_dashboard(selected);
        print_str("\n[*] Interactive TV Remote Control Active.\n");
        print_str("[*] Press '1'-'6' to jump cards, 'h'/'l' (or Left/Right) to navigate, 'q' to quit:\n");

        char buf[8];
        while (1) {
            long n = sys_read(0, buf, sizeof(buf));
            if (n <= 0) break;

            char ch = buf[0];
            if (ch == 'q' || ch == 'Q') {
                print_str("[*] Exiting interactive TV remote session.\n");
                break;
            } else if (ch >= '1' && ch <= '6') {
                selected = ch - '1';
                render_tcl_dashboard(selected);
                print_str("[+] Selected Input: ");
                print_str(cards[selected].title);
                print_str("\n");
            } else if (ch == 'h' || (n >= 3 && buf[0] == '\x1b' && buf[1] == '[' && buf[2] == 'D')) {
                selected = (selected > 0) ? selected - 1 : 5;
                render_tcl_dashboard(selected);
                print_str("[+] Left -> ");
                print_str(cards[selected].title);
                print_str("\n");
            } else if (ch == 'l' || (n >= 3 && buf[0] == '\x1b' && buf[1] == '[' && buf[2] == 'C')) {
                selected = (selected < 5) ? selected + 1 : 0;
                render_tcl_dashboard(selected);
                print_str("[+] Right -> ");
                print_str(cards[selected].title);
                print_str("\n");
            }
        }
    }

    sys_close(g_fb_fd);
    sys_exit(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov r0, sp\n"
        "b main_entry\n"
    );
}
