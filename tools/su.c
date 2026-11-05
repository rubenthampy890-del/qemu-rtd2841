/*
 * su.c - Standalone static root execution helper for ARMv7 Linux/Android
 * Target: Realtek RTD2841 / TCL RT41 Chassis (Board tv030)
 *
 * Uses raw EABI syscalls (svc 0) to ensure zero libc dependencies.
 * Adheres strictly to the 7 OpenDTV Inviolable Doctrines.
 */

static inline int sys_write(int fd, const void *buf, int count) {
    register int r0 __asm__("r0") = fd;
    register const void *r1 __asm__("r1") = buf;
    register int r2 __asm__("r2") = count;
    register int r7 __asm__("r7") = 4; /* __NR_write */
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static inline int sys_setresuid(int ruid, int euid, int suid) {
    register int r0 __asm__("r0") = ruid;
    register int r1 __asm__("r1") = euid;
    register int r2 __asm__("r2") = suid;
    register int r7 __asm__("r7") = 208; /* __NR_setresuid32 */
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static inline int sys_setresgid(int rgid, int egid, int sgid) {
    register int r0 __asm__("r0") = rgid;
    register int r1 __asm__("r1") = egid;
    register int r2 __asm__("r2") = sgid;
    register int r7 __asm__("r7") = 210; /* __NR_setresgid32 */
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static inline int sys_setgroups(int size, const unsigned int *list) {
    register int r0 __asm__("r0") = size;
    register const unsigned int *r1 __asm__("r1") = list;
    register int r7 __asm__("r7") = 206; /* __NR_setgroups32 */
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r7) : "memory");
    return r0;
}

static void print_err(const char *msg) {
    int len = 0;
    while (msg[len]) len++;
    sys_write(2, msg, len);
}

static void print_dec_err(int val) {
    char buf[16];
    int i = 0;
    unsigned int uval;
    if (val < 0) {
        print_err("-");
        uval = (unsigned int)(-val);
    } else {
        uval = (unsigned int)val;
    }
    if (uval == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
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
    print_err(buf);
}

static inline int sys_execve(const char *filename, char *const argv[], char *const envp[]) {
    register const char *r0 __asm__("r0") = filename;
    register char *const *r1 __asm__("r1") = argv;
    register char *const *r2 __asm__("r2") = envp;
    register int r7 __asm__("r7") = 11; /* __NR_execve */
    register int res __asm__("r0");
    __asm__ volatile("svc 0" : "=r"(res) : "r"(r0), "r"(r1), "r"(r2), "r"(r7) : "memory");
    return res;
}

static inline void sys_exit(int status) {
    register int r0 __asm__("r0") = status;
    register int r7 __asm__("r7") = 1; /* __NR_exit */
    __asm__ volatile("svc 0" : : "r"(r0), "r"(r7) : "memory");
    while(1);
}

static int streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static int run_cmd(const char *cmd, char *const args[], char *const envp[]) {
    int rc = sys_execve(cmd, args, envp);
    if (rc < 0 && cmd[0] != '/') {
        char buf[256];
        int i = 0;
        const char *p = "/system/bin/";
        while (p[i]) { buf[i] = p[i]; i++; }
        int j = 0;
        while (cmd[j] && i < 250) { buf[i++] = cmd[j++]; }
        buf[i] = '\0';
        rc = sys_execve(buf, args, envp);
        if (rc < 0) {
            char *const sh_args[] = {"/system/bin/sh", "-c", (char *)cmd, 0};
            rc = sys_execve("/system/bin/sh", sh_args, envp);
            if (rc < 0) {
                char *const bin_sh_args[] = {"/bin/sh", "-c", (char *)cmd, 0};
                rc = sys_execve("/bin/sh", bin_sh_args, envp);
            }
        }
    }
    return rc;
}

void main_su(long *sp) {
    int argc = (int)sp[0];
    char **argv = (char **)&sp[1];

    /* Elevate credentials to root with checked returns per Doctrine I */
    int rc;
    rc = sys_setresuid(0, 0, 0);
    if (rc != 0) {
        print_err("[-] FAILED: setresuid32 failed, rc=");
        print_dec_err(rc);
        print_err("\n");
        sys_exit(1);
    }

    rc = sys_setresgid(0, 0, 0);
    if (rc != 0 && rc != -1) {
        /*
         * Recovery path: Caller lacks CAP_SETGID in bounding set (e.g. adbd under SECURE_NOROOT)
         * or binary is SUID-only without SGID. When rc is -1 (-EPERM), do not emit error banner.
         */
        print_err("[-] FAILED: setresgid32 failed, rc=");
        print_dec_err(rc);
        print_err("\n");
    }

    rc = sys_setgroups(0, (const unsigned int *)0);
    if (rc != 0 && rc != -1) {
        /*
         * Recovery path: Clearing supplementary groups requires CAP_SETGID.
         * When caller lacks CAP_SETGID (-EPERM / rc=-1), do not pollute stderr.
         */
        print_err("[-] FAILED: setgroups32 failed, rc=");
        print_dec_err(rc);
        print_err("\n");
    }

    char *const envp[] = {
        "PATH=/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin",
        "HOME=/data/local/tmp",
        "USER=root",
        "SHELL=/system/bin/sh",
        "TERM=xterm-256color",
        0
    };

    int exec_rc;

    /* Handle standard Android su invocations */
    if (argc > 3 && (streq(argv[1], "0") || streq(argv[1], "root")) && streq(argv[2], "-c")) {
        char *const sh_args[] = {"/system/bin/sh", "-c", argv[3], 0};
        exec_rc = sys_execve("/system/bin/sh", sh_args, envp);
        if (exec_rc < 0) {
            exec_rc = sys_execve("/bin/sh", sh_args, envp);
        }
    } else if (argc > 2 && (streq(argv[1], "0") || streq(argv[1], "root"))) {
        exec_rc = run_cmd(argv[2], &argv[2], envp);
    } else if (argc > 2 && streq(argv[1], "-c")) {
        char *const sh_args[] = {"/system/bin/sh", "-c", argv[2], 0};
        exec_rc = sys_execve("/system/bin/sh", sh_args, envp);
        if (exec_rc < 0) {
            exec_rc = sys_execve("/bin/sh", sh_args, envp);
        }
    } else if (argc > 1 && !streq(argv[1], "0") && !streq(argv[1], "root")) {
        exec_rc = run_cmd(argv[1], &argv[1], envp);
    } else {
        char *const sh_args[] = {"/system/bin/sh", 0};
        exec_rc = sys_execve("/system/bin/sh", sh_args, envp);
        if (exec_rc < 0) {
            char *const bin_sh_args[] = {"/bin/sh", 0};
            exec_rc = sys_execve("/bin/sh", bin_sh_args, envp);
        }
    }

    print_err("[-] FAILED: execve failed, rc=");
    print_dec_err(exec_rc);
    print_err("\n");
    sys_exit(127);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov r0, sp\n"
        "b main_su\n"
    );
}
