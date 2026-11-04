#!/usr/bin/env python3
"""
Test Zygote startup directly using a helper shell script in QEMU.
"""

import os
import sys
import time
import subprocess
import tempfile
import selectors
import signal
from pathlib import Path

ROOT = Path("/Users/basilthampy/Documents/Qemu rtd")
REF = Path("/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re")

DISK = (ROOT / "staging/emmc.raw").resolve(strict=True)
KERNEL = (ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage").resolve(strict=True)
DTB = (ROOT / "dts/rtd284x-tv030.dtb").resolve(strict=True)
QEMU = (REF / "qemu/build/qemu-system-aarch64").resolve(strict=True)
QEMU_IMG = (REF / "qemu/build/qemu-img").resolve(strict=True)

# A small standalone C program with raw EABI syscalls to create the socket and exec app_process with ANDROID_SOCKET_zygote
ZYGOTE_LAUNCHER_C = """
struct my_sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};

static inline long sys_write(long fd, const void *buf, unsigned long count) {
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = count;
    register long r7 __asm__("r7") = 4;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static inline long sys_unlink(const char *path) {
    register long r0 __asm__("r0") = (long)path;
    register long r7 __asm__("r7") = 10;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r7) : "memory");
    return r0;
}
static inline long sys_mkdir(const char *path, long mode) {
    register long r0 __asm__("r0") = (long)path;
    register long r1 __asm__("r1") = mode;
    register long r7 __asm__("r7") = 39;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r7) : "memory");
    return r0;
}
static inline long sys_socket(long domain, long type, long protocol) {
    register long r0 __asm__("r0") = domain;
    register long r1 __asm__("r1") = type;
    register long r2 __asm__("r2") = protocol;
    register long r7 __asm__("r7") = 281;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static inline long sys_dup2(long oldfd, long newfd) {
    register long r0 __asm__("r0") = oldfd;
    register long r1 __asm__("r1") = newfd;
    register long r7 __asm__("r7") = 63;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r7) : "memory");
    return r0;
}
static inline long sys_close(long fd) {
    register long r0 __asm__("r0") = fd;
    register long r7 __asm__("r7") = 6;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r7) : "memory");
    return r0;
}
static inline long sys_bind(long fd, const void *addr, long addrlen) {
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)addr;
    register long r2 __asm__("r2") = addrlen;
    register long r7 __asm__("r7") = 282;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static inline long sys_chmod(const char *path, long mode) {
    register long r0 __asm__("r0") = (long)path;
    register long r1 __asm__("r1") = mode;
    register long r7 __asm__("r7") = 15;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r7) : "memory");
    return r0;
}
static inline long sys_chown32(const char *path, long owner, long group) {
    register long r0 __asm__("r0") = (long)path;
    register long r1 __asm__("r1") = owner;
    register long r2 __asm__("r2") = group;
    register long r7 __asm__("r7") = 212;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static inline long sys_listen(long fd, long backlog) {
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = backlog;
    register long r7 __asm__("r7") = 284;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r7) : "memory");
    return r0;
}
static inline long sys_execve(const char *filename, char *const argv[], char *const envp[]) {
    register long r0 __asm__("r0") = (long)filename;
    register long r1 __asm__("r1") = (long)argv;
    register long r2 __asm__("r2") = (long)envp;
    register long r7 __asm__("r7") = 11;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static inline void sys_exit(long status) {
    register long r0 __asm__("r0") = status;
    register long r7 __asm__("r7") = 1;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r7) : "memory");
}

static void print(const char *s) {
    int len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

void _start(void) {
    sys_unlink("/dev/socket/zygote");
    sys_mkdir("/dev/socket", 0755);

    long s = sys_socket(1 /* AF_UNIX */, 1 /* SOCK_STREAM */, 0);
    if (s < 0) {
        print("[-] socket failed\\n");
        sys_exit(1);
    }

    if (sys_dup2(s, 3) < 0) {
        print("[-] dup2 failed\\n");
        sys_exit(2);
    }
    if (s != 3) sys_close(s);

    struct my_sockaddr_un sun;
    for (int i = 0; i < sizeof(sun); i++) ((char*)&sun)[i] = 0;
    sun.sun_family = 1; /* AF_UNIX */
    const char path[] = "/dev/socket/zygote";
    for (int i = 0; i < sizeof(path); i++) sun.sun_path[i] = path[i];

    if (sys_bind(3, &sun, sizeof(sun)) < 0) {
        print("[-] bind failed\\n");
        sys_exit(3);
    }
    sys_chmod(path, 0660);
    sys_chown32(path, 0, 1000);
    if (sys_listen(3, 16) < 0) {
        print("[-] listen failed\\n");
        sys_exit(4);
    }

    print("[+] Zygote socket bound to fd 3\\n");

    char *const argv[] = {
        "/system/bin/app_process",
        "-Xzygote",
        "/system/bin",
        "--zygote",
        "--start-system-server",
        0
    };

    char *const envp[] = {
        "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin",
        "ANDROID_ROOT=/system",
        "ANDROID_DATA=/data",
        "ANDROID_STORAGE=/storage",
        "ANDROID_ASSETS=/system/app",
        "BOOTCLASSPATH=/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/bouncycastle.jar:/system/framework/apache-xml.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/android.hidl.base-V1.0-java.jar:/system/framework/android.hidl.manager-V1.0-java.jar:/system/framework/framework-oahl-backward-compatibility.jar:/system/framework/android.test.base.jar:/system/framework/tcl.resource.jar",
        "SYSTEMSERVERCLASSPATH=/system/framework/services.jar:/system/framework/ethernet-service.jar:/system/framework/wifi-service.jar",
        "LD_LIBRARY_PATH=/system/lib:/vendor/lib",
        "ANDROID_SOCKET_zygote=3",
        0
    };

    print("[+] Executing app_process with socket on fd 3...\\n");
    sys_execve(argv[0], argv, envp);
    print("[-] execve app_process failed!\\n");
    sys_exit(127);
}
"""

def main():
    print("[*] Compiling Zygote test launcher for ARM32...")
    tmp_c = "/tmp/zygote_test.c"
    tmp_bin = "/tmp/zygote_test"
    with open(tmp_c, "w") as f:
        f.write(ZYGOTE_LAUNCHER_C)

    # Compile with arm-none-eabi-gcc
    subprocess.run(["/opt/homebrew/bin/arm-none-eabi-gcc", "-march=armv7-a", "-nostdlib", "-static", "-fno-builtin", "-O2", tmp_c, "-o", tmp_bin], check=True)
    print(f"[+] Compiled {tmp_bin} ({os.path.getsize(tmp_bin)} bytes)")

if __name__ == "__main__":
    main()
