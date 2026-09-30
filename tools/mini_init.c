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

static inline long my_syscall3(long n, long a1, long a2, long a3) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    __asm__ volatile("swi #0" : "=r"(r0) : "r"(r7), "r"(r0), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static const char msg[] = "\n========================================\n[+] RTD2841 USERSPACE BOOTED SUCCESSFULLY!\n[+] PID 1 INIT IS ACTIVE!\n========================================\n\n";
static const char msg2[] = "[+] Mounted /proc and /sys!\n[+] Executing /bin/sh...\n";
static const char err[] = "[-] execve /bin/sh failed\n";
static const char str_proc[] = "proc";
static const char str_sysfs[] = "sysfs";
static const char str_proc_path[] = "/proc";
static const char str_sys_path[] = "/sys";
static const char str_sh[] = "/bin/sh";
static const char str_path[] = "PATH=/bin:/system/bin:/vendor/bin";
static const char str_term[] = "TERM=vt100";

static char *argv[] = {(char*)str_sh, 0};
static char *envp[] = {(char*)str_path, (char*)str_term, 0};

void _start(void) {
    my_syscall3(4 /* __NR_write */, 1, (long)msg, sizeof(msg) - 1);

    // mount("proc", "/proc", "proc", 0, 0)
    my_syscall5(21 /* __NR_mount */, (long)str_proc, (long)str_proc_path, (long)str_proc, 0, 0);

    // mount("sysfs", "/sys", "sysfs", 0, 0)
    my_syscall5(21 /* __NR_mount */, (long)str_sysfs, (long)str_sys_path, (long)str_sysfs, 0, 0);

    my_syscall3(4, 1, (long)msg2, sizeof(msg2) - 1);

    my_syscall3(11 /* __NR_execve */, (long)str_sh, (long)argv, (long)envp);

    my_syscall3(4, 1, (long)err, sizeof(err) - 1);

    while(1) {
        my_syscall3(29 /* __NR_pause */, 0, 0, 0);
    }
}
