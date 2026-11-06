/*
 * test_fbdev_hwc.c - Low-level fbdev and HWC hardware surface validation
 * Target: Realtek RTD2841 / TCL RT41 Chassis (tv030)
 *
 * Implements strict checked ioctls for FBIOGET_FSCREENINFO, FBIOGET_VSCREENINFO,
 * memory mapping, pattern write, and bitwise readback verification matching
 * the contract in hwcomposer / HWC2OnFbAdapter and simplefb.
 *
 * Zero libc dependencies. Complies with the 7 OpenDTV Inviolable Doctrines.
 */

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

struct fb_fix_screeninfo {
    char id[16];
    unsigned long smem_start;
    unsigned int smem_len;
    unsigned int type;
    unsigned int type_aux;
    unsigned int visual;
    unsigned short xpanstep;
    unsigned short ypanstep;
    unsigned short ywrapstep;
    unsigned int line_length;
    unsigned long mmio_start;
    unsigned int mmio_len;
    unsigned int accel;
    unsigned short capabilities;
    unsigned short reserved[2];
};

struct fb_bitfield {
    unsigned int offset;
    unsigned int length;
    unsigned int msb_right;
};

struct fb_var_screeninfo {
    unsigned int xres;
    unsigned int yres;
    unsigned int xres_virtual;
    unsigned int yres_virtual;
    unsigned int xoffset;
    unsigned int yoffset;
    unsigned int bits_per_pixel;
    unsigned int grayscale;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;
    unsigned int nonstd;
    unsigned int activate;
    unsigned int height;
    unsigned int width;
    unsigned int accel_flags;
    unsigned int pixclock;
    unsigned int left_margin;
    unsigned int right_margin;
    unsigned int upper_margin;
    unsigned int lower_margin;
    unsigned int hsync_len;
    unsigned int vsync_len;
    unsigned int sync;
    unsigned int vmode;
    unsigned int rotate;
    unsigned int colorspace;
    unsigned int reserved[4];
};

/* Raw ARM Linux Syscalls */
static inline long sys_open(const char *path, long flags, long mode) {
    register long r7 __asm__("r7") = 5; /* __NR_open */
    register long r0 __asm__("r0") = (long)path;
    register long r1 __asm__("r1") = flags;
    register long r2 __asm__("r2") = mode;
    __asm__ volatile("swi #0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline long sys_close(long fd) {
    register long r7 __asm__("r7") = 6; /* __NR_close */
    register long r0 __asm__("r0") = fd;
    __asm__ volatile("swi #0" : "+r"(r0) : "r"(r7) : "memory");
    return r0;
}

static inline long sys_write(long fd, const void *buf, unsigned long count) {
    register long r7 __asm__("r7") = 4; /* __NR_write */
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = count;
    __asm__ volatile("swi #0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline long sys_ioctl(long fd, unsigned long req, void *arg) {
    register long r7 __asm__("r7") = 54; /* __NR_ioctl */
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)req;
    register long r2 __asm__("r2") = (long)arg;
    __asm__ volatile("swi #0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static inline void *sys_mmap2(void *addr, unsigned long length, long prot,
                             long flags, long fd, unsigned long pgoff) {
    register long r7 __asm__("r7") = 192; /* __NR_mmap2 */
    register long r0 __asm__("r0") = (long)addr;
    register long r1 __asm__("r1") = (long)length;
    register long r2 __asm__("r2") = prot;
    register long r3 __asm__("r3") = flags;
    register long r4 __asm__("r4") = fd;
    register long r5 __asm__("r5") = (long)pgoff;
    __asm__ volatile("swi #0" : "+r"(r0)
                     : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
                     : "memory");
    return (void *)r0;
}

static inline long sys_munmap(void *addr, unsigned long length) {
    register long r7 __asm__("r7") = 91; /* __NR_munmap */
    register long r0 __asm__("r0") = (long)addr;
    register long r1 __asm__("r1") = (long)length;
    __asm__ volatile("swi #0" : "+r"(r0) : "r"(r7), "r"(r1) : "memory");
    return r0;
}

static inline void sys_exit(long status) {
    register long r7 __asm__("r7") = 1; /* __NR_exit */
    register long r0 __asm__("r0") = status;
    __asm__ volatile("swi #0" : : "r"(r7), "r"(r0));
    while (1) {}
}

static void print_s(const char *s) {
    if (!s) return;
    int len = 0;
    while (s[len]) len++;
    int off = 0;
    while (off < len) {
        long w = sys_write(1, s + off, (unsigned long)(len - off));
        if (w <= 0) break;
        off += (int)w;
    }
}

static void print_d(long val) {
    char buf[32];
    int i = 0;
    if (val < 0) {
        print_s("-");
        val = -val;
    }
    if (val == 0) {
        print_s("0");
        return;
    }
    while (val > 0) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    char rev[32];
    for (int j = 0; j < i; j++) {
        rev[j] = buf[i - 1 - j];
    }
    rev[i] = '\0';
    print_s(rev);
}

static void print_x(unsigned long val) {
    char buf[16];
    int i = 0;
    if (val == 0) {
        print_s("0x0");
        return;
    }
    print_s("0x");
    while (val > 0) {
        int d = (int)(val & 0xF);
        buf[i++] = (char)((d < 10) ? ('0' + d) : ('a' + d - 10));
        val >>= 4;
    }
    char rev[16];
    for (int j = 0; j < i; j++) {
        rev[j] = buf[i - 1 - j];
    }
    rev[i] = '\0';
    print_s(rev);
}

static void my_memset(void *dst, int val, unsigned long n) {
    unsigned char *d = (unsigned char *)dst;
    for (unsigned long i = 0; i < n; i++) d[i] = (unsigned char)val;
}

static inline void memory_barrier(void) {
    __asm__ volatile("dmb sy" ::: "memory");
}

/* Deterministic test pattern pixel calculation (ARGB8888) */
static inline unsigned int calc_test_pixel(unsigned int x, unsigned int y) {
    unsigned char r = (unsigned char)((x * 255) / 1920);
    unsigned char g = (unsigned char)((y * 255) / 1080);
    unsigned char b = 128;
    unsigned char a = 255;
    return ((unsigned int)a << 24) | ((unsigned int)r << 16) | ((unsigned int)g << 8) | b;
}

void _start(void) {
    print_s("[*] OpenDTV Phase 3: Hardware Framebuffer & Composer HAL Probe\n");

    const char *dev_path = "/dev/graphics/fb0";
    long fd = sys_open(dev_path, 2 /* O_RDWR */, 0);
    if (fd < 0) {
        print_s("[!] /dev/graphics/fb0 not found, trying /dev/fb0...\n");
        dev_path = "/dev/fb0";
        fd = sys_open(dev_path, 2 /* O_RDWR */, 0);
        if (fd < 0) {
            print_s("[-] FAILED: open framebuffer device, rc=");
            print_d(fd);
            print_s("\n");
            sys_exit(1);
        }
    }
    print_s("[+] Successfully opened framebuffer device: ");
    print_s(dev_path);
    print_s(" (fd=");
    print_d(fd);
    print_s(")\n");

    /* 1. Query Fixed Screen Information */
    struct fb_fix_screeninfo fix;
    my_memset(&fix, 0, sizeof(fix));
    long rc_fix = sys_ioctl(fd, FBIOGET_FSCREENINFO, &fix);
    if (rc_fix != 0) {
        print_s("[-] FAILED: FBIOGET_FSCREENINFO, rc=");
        print_d(rc_fix);
        print_s("\n");
        sys_close(fd);
        sys_exit(2);
    }
    print_s("[+] FBIOGET_FSCREENINFO query successful:\n");
    print_s("    ID:          "); print_s(fix.id); print_s("\n");
    print_s("    smem_start:  "); print_x(fix.smem_start); print_s("\n");
    print_s("    smem_len:    "); print_d((long)fix.smem_len); print_s(" bytes\n");
    print_s("    line_length: "); print_d((long)fix.line_length); print_s(" bytes\n");

    /* Strict assertions on fixed screen info */
    if (fix.smem_start != 0x3E000000UL) {
        print_s("[-] FAILED: smem_start mismatch (expected 0x3E000000, got ");
        print_x(fix.smem_start);
        print_s(")\n");
        sys_close(fd);
        sys_exit(10);
    }
    if (fix.line_length != 7680) {
        print_s("[-] FAILED: line_length mismatch (expected 7680, got ");
        print_d((long)fix.line_length);
        print_s(")\n");
        sys_close(fd);
        sys_exit(11);
    }
    if (fix.smem_len < 8294400UL) {
        print_s("[-] FAILED: smem_len insufficient for 1080p frame (got ");
        print_d((long)fix.smem_len);
        print_s(")\n");
        sys_close(fd);
        sys_exit(12);
    }

    /* 2. Query Variable Screen Information */
    struct fb_var_screeninfo var;
    my_memset(&var, 0, sizeof(var));
    long rc_var = sys_ioctl(fd, FBIOGET_VSCREENINFO, &var);
    if (rc_var != 0) {
        print_s("[-] FAILED: FBIOGET_VSCREENINFO, rc=");
        print_d(rc_var);
        print_s("\n");
        sys_close(fd);
        sys_exit(3);
    }
    print_s("[+] FBIOGET_VSCREENINFO query successful:\n");
    print_s("    Resolution:  "); print_d((long)var.xres); print_s("x"); print_d((long)var.yres); print_s("\n");
    print_s("    BPP:         "); print_d((long)var.bits_per_pixel); print_s(" bpp\n");
    print_s("    Red:   offset="); print_d((long)var.red.offset);   print_s(", length="); print_d((long)var.red.length);   print_s("\n");
    print_s("    Green: offset="); print_d((long)var.green.offset); print_s(", length="); print_d((long)var.green.length); print_s("\n");
    print_s("    Blue:  offset="); print_d((long)var.blue.offset);  print_s(", length="); print_d((long)var.blue.length);  print_s("\n");
    print_s("    Alpha: offset="); print_d((long)var.transp.offset);print_s(", length="); print_d((long)var.transp.length);print_s("\n");

    /* Strict assertions on variable screen geometry */
    if (var.xres != 1920) {
        print_s("[-] FAILED: xres mismatch (expected 1920, got ");
        print_d((long)var.xres);
        print_s(")\n");
        sys_close(fd);
        sys_exit(13);
    }
    if (var.yres != 1080) {
        print_s("[-] FAILED: yres mismatch (expected 1080, got ");
        print_d((long)var.yres);
        print_s(")\n");
        sys_close(fd);
        sys_exit(14);
    }
    if (var.bits_per_pixel != 32) {
        print_s("[-] FAILED: bits_per_pixel mismatch (expected 32, got ");
        print_d((long)var.bits_per_pixel);
        print_s(")\n");
        sys_close(fd);
        sys_exit(15);
    }
    if (var.red.offset != 16 || var.red.length != 8 ||
        var.green.offset != 8 || var.green.length != 8 ||
        var.blue.offset != 0 || var.blue.length != 8) {
        print_s("[-] FAILED: pixel channel offset/length mismatch for ARGB32\n");
        sys_close(fd);
        sys_exit(16);
    }

    /* 3. Validate Framebuffer Geometry & Map */
    unsigned long frame_bytes = (unsigned long)var.yres * fix.line_length;
    if (frame_bytes != 8294400UL) {
        print_s("[-] FAILED: expected frame_bytes 8294400, got ");
        print_d((long)frame_bytes);
        print_s("\n");
        sys_close(fd);
        sys_exit(17);
    }
    if (frame_bytes > fix.smem_len) {
        print_s("[-] FAILED: frame exceeds carveout capacity\n");
        sys_close(fd);
        sys_exit(18);
    }

    print_s("[*] Testing mmap of framebuffer (8294400 bytes)...\n");
    void *fb_ptr = sys_mmap2(0, frame_bytes, 3 /* PROT_READ|PROT_WRITE */,
                             1 /* MAP_SHARED */, fd, 0);
    if ((unsigned long)fb_ptr >= 0xFFFFF000UL) {
        print_s("[-] FAILED: mmap framebuffer, rc=");
        print_x((unsigned long)fb_ptr);
        print_s("\n");
        sys_close(fd);
        sys_exit(4);
    }
    print_s("[+] mmap successful: mapped at virtual address ");
    print_x((unsigned long)fb_ptr);
    print_s("\n");

    /* 4. Write Verified Test Pattern Directly to Mapped simplefb RAM */
    print_s("[*] Writing deterministic test pattern to mapped framebuffer...\n");
    volatile unsigned int *pixels = (volatile unsigned int *)fb_ptr;

    for (unsigned int y = 0; y < 1080; y++) {
        unsigned int row_start = y * 1920;
        for (unsigned int x = 0; x < 1920; x++) {
            pixels[row_start + x] = calc_test_pixel(x, y);
        }
    }
    memory_barrier();

    /* 5. Memory Readback & Bitwise Verification */
    print_s("[*] Executing pixel readback bitwise verification...\n");
    unsigned long non_zero_count = 0;
    for (unsigned int y = 0; y < 1080; y++) {
        unsigned int row_start = y * 1920;
        for (unsigned int x = 0; x < 1920; x++) {
            unsigned int expected = calc_test_pixel(x, y);
            unsigned int actual = pixels[row_start + x];
            if (actual != expected) {
                print_s("[-] FAILED: bitwise readback mismatch at (");
                print_d((long)x);
                print_s(",");
                print_d((long)y);
                print_s("): expected ");
                print_x(expected);
                print_s(", got ");
                print_x(actual);
                print_s("\n");
                sys_munmap(fb_ptr, frame_bytes);
                sys_close(fd);
                sys_exit(20);
            }
            if (actual != 0) {
                non_zero_count += 4;
            }
        }
    }
    print_s("[+] Readback verified: all 2,073,600 pixels match bitwise ground truth.\n");
    print_s("[+] Non-zero framebuffer bytes: ");
    print_d((long)non_zero_count);
    print_s("\n");

    /* 6. Clean Unmap & Close with Explicit Checked Returns */
    long rc_unmap = sys_munmap(fb_ptr, frame_bytes);
    if (rc_unmap != 0) {
        print_s("[-] FAILED: munmap framebuffer, rc=");
        print_d(rc_unmap);
        print_s("\n");
        sys_close(fd);
        sys_exit(21);
    }
    print_s("[+] munmap cleanly completed.\n");

    long rc_close = sys_close(fd);
    if (rc_close != 0) {
        print_s("[-] FAILED: close framebuffer, rc=");
        print_d(rc_close);
        print_s("\n");
        sys_exit(22);
    }
    print_s("[+] Framebuffer device closed cleanly.\n");

    print_s("[>>> SURFACE PROBE VERIFICATION PASSED <<<\n");
    sys_exit(0);
}
