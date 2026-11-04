#!/usr/bin/env python3
"""
Probe Zygote and System Server execution on QEMU RTD2841.
"""

import http.server
import json
import os
import re
import selectors
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path("/Users/basilthampy/Documents/Qemu rtd")
REF = Path("/Users/basilthampy/.gemini/antigravity/scratch/tcl-firmware-re")

DISK = (ROOT / "staging/emmc.raw").resolve(strict=True)
KERNEL = (ROOT / "build/phase3/kernel-smp-sdhci-20261026T141826Z/zImage").resolve(strict=True)
DTB = (ROOT / "dts/rtd284x-tv030.dtb").resolve(strict=True)
QEMU = (REF / "qemu/build/qemu-system-aarch64").resolve(strict=True)
QEMU_IMG = (REF / "qemu/build/qemu-img").resolve(strict=True)

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
    print("[*] Compiling Zygote launcher binary...")
    bin_path = ROOT / "build/phase3/zygote_launcher"
    tmp_c = "/tmp/zygote_launcher.c"
    with open(tmp_c, "w") as f:
        f.write(ZYGOTE_LAUNCHER_C)
    subprocess.run([
        "/opt/homebrew/bin/arm-none-eabi-gcc",
        "-march=armv7-a", "-nostdlib", "-static", "-fno-builtin", "-O2",
        tmp_c, "-o", str(bin_path)
    ], check=True)
    bin_bytes = bin_path.read_bytes()
    print(f"[+] Compiled {bin_path} ({len(bin_bytes)} bytes)")

    class ZygoteServer(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/zygote_launcher":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(bin_bytes)))
                self.end_headers()
                self.wfile.write(bin_bytes)
            else:
                self.send_error(404)
        def log_message(self, *args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), ZygoteServer)
    port = server.server_address[1]
    threading.Thread(target=server.serve_forever, daemon=True).start()
    print(f"[*] Serving zygote_launcher on port {port}")

    with tempfile.TemporaryDirectory() as td:
        overlay = Path(td) / "overlay.qcow2"
        subprocess.run([str(QEMU_IMG), "create", "-f", "qcow2", "-b", str(DISK), "-F", "raw", str(overlay)],
                       check=True, capture_output=True)

        cmd = [
            str(QEMU), "-M", "rtd2841-tv030", "-m", "1024M", "-smp", "4",
            "-dtb", str(DTB), "-display", "none", "-serial", "stdio",
            "-monitor", "none", "-kernel", str(KERNEL), "-no-reboot",
            "-drive", f"file={overlay},format=qcow2,if=sd",
            "-append", "console=tty0 console=ttyS0,115200 earlyprintk "
                       "root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait "
                       "init=/init consoleblank=0",
            "-nic", "user,model=rtd2841-eth"
        ]

        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, preexec_fn=os.setsid, bufsize=0)

        sel = selectors.DefaultSelector()
        os.set_blocking(proc.stdout.fileno(), False)
        sel.register(proc.stdout, selectors.EVENT_READ)

        buf = ""
        sent_q = False

        def read_until_prompt(target_prompt=":/ # ", wait_timeout=15.0):
            nonlocal buf
            start_t = time.time()
            collected = ""
            while time.time() - start_t < wait_timeout:
                events = sel.select(timeout=0.1)
                for key, _ in events:
                    try:
                        chunk = os.read(key.fileobj.fileno(), 4096).decode("utf-8", "replace")
                        if chunk:
                            buf += chunk
                            collected += chunk
                            sys.stdout.write(chunk)
                            sys.stdout.flush()
                    except Exception:
                        pass
                if target_prompt in collected:
                    return collected
                if proc.poll() is not None:
                    break
            return collected

        commands = [
            "setprop ro.product.cpu.abi armeabi-v7a",
            "setprop ro.product.cpu.abilist armeabi-v7a,armeabi",
            "setprop ro.product.cpu.abilist32 armeabi-v7a,armeabi",
            "setprop ro.zygote zygote32",
            "setprop dalvik.vm.boot-image /system/framework/arm/boot.art",
            "setprop dalvik.vm.usejit true",
            "setprop dalvik.vm.isa.arm.variant cortex-a7",
            "setprop dalvik.vm.isa.arm.features default",
            "setprop vold.has_adoptable 0",
            "mkdir -p /data/dalvik-cache/arm /data/system/users/0 /data/data /data/user /data/app /dev/cpuset/foreground",
            "chmod 0771 /data/dalvik-cache /data/dalvik-cache/arm /data/data /data/app",
            "chown 1000:1000 /data/dalvik-cache /data/dalvik-cache/arm /data/data /data/app /data/system /data/system/users /data/system/users/0",
            "ln -sf /data/data /data/user/0",
            "touch /dev/cpuset/foreground/tasks",
            "chmod 0666 /dev/cpuset/foreground/tasks",
            f"LD_LIBRARY_PATH=/vendor/am_apps/tplayer/lib:/vendor/lib:/system/lib /vendor/am_apps/tplayer/bin/curl -s http://10.0.2.2:{port}/zygote_launcher -o /data/zygote_launcher",
            "chmod 0755 /data/zygote_launcher",
            "/vendor/bin/hw/android.hardware.configstore@1.1-service &",
            "/vendor/bin/hw/android.hardware.graphics.allocator@2.0-service &",
            "/vendor/bin/hw/android.hardware.graphics.composer@2.1-service &",
            "sleep 1",
            "/system/bin/surfaceflinger &",
            "sleep 2",
            "/data/zygote_launcher > /data/zygote.log 2>&1 &",
            "sleep 10",
            "ps",
            "cat /data/zygote.log",
            "dmesg | tail -n 30"
        ]

        try:
            # 1. Wait for remote control banner and send 'q'
            t0 = time.time()
            while time.time() - t0 < 45.0:
                events = sel.select(timeout=0.1)
                for key, _ in events:
                    try:
                        chunk = os.read(key.fileobj.fileno(), 4096).decode("utf-8", "replace")
                        if chunk:
                            buf += chunk
                    except Exception:
                        pass
                if not sent_q and "Interactive TV Remote Control Active" in buf:
                    proc.stdin.write(b"q\n")
                    proc.stdin.flush()
                    sent_q = True
                    break

            # 2. Wait for root shell prompt
            print("[+] Waiting for initial root prompt...")
            read_until_prompt(":/ # ", wait_timeout=10.0)

            # 3. Execute each command synchronously
            for idx, c in enumerate(commands):
                print(f"\n[>>> EXEC {idx+1}/{len(commands)}: {c} <<<]")
                proc.stdin.write(f"{c}\n".encode())
                proc.stdin.flush()
                res = read_until_prompt(":/ # ", wait_timeout=15.0)
                time.sleep(0.2)

            print("\n[+] All Zygote probe commands executed.")
        finally:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception:
                pass

if __name__ == "__main__":
    main()
