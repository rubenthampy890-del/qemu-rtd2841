/* Minimal ARM32 syscall tracer for logd */
typedef unsigned long U;

static long my_syscall0(long n) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0");
    __asm__ volatile("swi 0" : "=r"(r0) : "r"(r7) : "memory", "cc");
    return r0;
}

static long my_syscall1(long n, long a) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a;
    __asm__ volatile("swi 0" : "+r"(r0) : "r"(r7) : "memory", "cc");
    return r0;
}

static long my_syscall3(long n, long a, long b, long c) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    __asm__ volatile("swi 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
    return r0;
}

static long my_syscall4(long n, long a, long b, long c, long d) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r3 __asm__("r3") = d;
    __asm__ volatile("swi 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory", "cc");
    return r0;
}

static void print_str(const char *s) {
    U len = 0;
    while (s[len]) len++;
    my_syscall3(4 /* __NR_write */, 1, (long)s, len);
}

static void print_dec(long val) {
    char buf[16];
    int i = 0;
    if (val < 0) {
        print_str("-");
        val = -val;
    }
    if (val == 0) {
        print_str("0");
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        char c = buf[--i];
        my_syscall3(4, 1, (long)&c, 1);
    }
}

static void print_hex(U val) {
    char buf[16];
    static const char hex[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) {
        buf[i] = hex[val & 0xf];
        val >>= 4;
    }
    buf[8] = 0;
    print_str("0x");
    print_str(buf);
}

struct pt_regs {
    long uregs[18];
};

void _start(void) {
    print_str("[*] trace_logd starting\n");
    long pid = my_syscall0(2 /* __NR_fork */);
    if (pid < 0) {
        print_str("[-] fork failed\n");
        my_syscall1(1, 1);
    }
    if (pid == 0) {
        my_syscall4(26 /* __NR_ptrace */, 0 /* PTRACE_TRACEME */, 0, 0, 0);
        static const char bin[] = "/system/bin/logd";
        char *const argv[] = {(char *)bin, 0};
        static const char *const envp[] = {
            "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin",
            "ANDROID_ROOT=/system",
            "ANDROID_DATA=/data",
            "ANDROID_STORAGE=/storage",
            "ANDROID_SOCKET_logd=3",
            "ANDROID_SOCKET_logdr=4",
            "ANDROID_SOCKET_logdw=5",
            "ANDROID_FILE__proc_kmsg=6",
            "ANDROID_FILE__dev_kmsg=7",
            0
        };
        my_syscall3(11 /* __NR_execve */, (long)bin, (long)argv, (long)envp);
        my_syscall1(1, 127);
    }

    int status = 0;
    my_syscall4(114 /* __NR_wait4 */, pid, (long)&status, 0, 0);
    print_str("[*] child initial stop, tracing syscalls...\n");

    int in_syscall = 0;
    long last_sys = 0;

    while (1) {
        long r = my_syscall4(26 /* __NR_ptrace */, 24 /* PTRACE_SYSCALL */, pid, 0, 0);
        if (r < 0) break;
        long w = my_syscall4(114 /* __NR_wait4 */, pid, (long)&status, 0, 0);
        if (w <= 0) break;

        /* Check if child exited */
        if ((status & 0x7f) == 0) {
            print_str("[+] child exited with status=");
            print_dec((status >> 8) & 0xff);
            print_str("\n");
            break;
        }
        if ((status & 0x7f) != 0x7f) {
            print_str("[-] child signaled: ");
            print_dec(status & 0x7f);
            print_str("\n");
            break;
        }

        struct pt_regs regs;
        my_syscall4(26 /* __NR_ptrace */, 12 /* PTRACE_GETREGS */, pid, 0, (long)&regs);

        if (!in_syscall) {
            last_sys = regs.uregs[7];
            in_syscall = 1;
            /* Only print if it is related to exit, cap, sched, open, or prctl */
            if (last_sys == 1 /* exit */ || last_sys == 248 /* exit_group */ ||
                last_sys == 123 /* fchown32/cap */ || last_sys == 156 /* sched_setscheduler */ ||
                last_sys == 172 /* prctl */ || last_sys == 184 /* capget */ ||
                last_sys == 185 /* capset */ || last_sys == 208 /* setresuid */ ||
                last_sys == 210 /* setresgid */ || last_sys == 213 /* setuid */ ||
                last_sys == 214 /* setgid */ || last_sys == 205 /* setgroups */ ||
                last_sys == 96 /* setpriority */) {
                print_str("SYS_ENTER: nr=");
                print_dec(last_sys);
                print_str(" r0=");
                print_hex(regs.uregs[0]);
                print_str(" r1=");
                print_hex(regs.uregs[1]);
                print_str(" r2=");
                print_hex(regs.uregs[2]);
                print_str("\n");
            }
        } else {
            in_syscall = 0;
            if (last_sys == 1 || last_sys == 248 || last_sys == 156 ||
                last_sys == 172 || last_sys == 185 || last_sys == 213 ||
                last_sys == 214 || last_sys == 205 || last_sys == 96) {
                print_str("SYS_EXIT:  nr=");
                print_dec(last_sys);
                print_str(" ret=");
                print_dec(regs.uregs[0]);
                print_str("\n");
            }
        }
    }
    my_syscall1(1, 0);
}
