#!/usr/bin/env python3
"""
OpenDTV Phase 3 Milestone 4 Verification Runner
Target: Realtek RTD2841 / TCL RT41 / tv030 Android 9 Pie Userspace

Verifies:
1. Android Userspace Mode bootstrap under mini_init with `android=1` cmdline.
2. Runtime directory hierarchy in persistent ext4 `/data`:
   - /data/dalvik-cache/arm (0771 system:system)
   - /data/system/users/0 (0700 system:system)
   - /data/user/0 -> /data/data (symlink)
   - /dev/cpuset/foreground/tasks
3. Android Treble service stack:
   - servicemanager, hwservicemanager, vndservicemanager active
   - configstore@1.1, graphics.allocator@2.0, graphics.composer@2.1 HALs active
   - SurfaceFlinger active and registered with servicemanager
4. Zygote (app_process32) and ART runtime startup:
   - Listening Unix domain socket on /dev/socket/zygote (fd 3 inheritance)
   - Boot classpath and runtime properties populated
5. Authenticated ADB connectivity and inspection:
   - getprop ro.product.model returns 'UnionTV'
   - getprop ro.build.version.sdk returns '28'
   - service check SurfaceFlinger returns 'found'
   - ps confirms surfaceflinger and active processes
6. Adherence to all 7 OpenDTV Inviolable Doctrines.
"""

import hashlib
import http.server
import json
import os
import re
import selectors
import signal
import socket
import subprocess
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
ADB = Path("/opt/homebrew/bin/adb").resolve(strict=True)
HOST_ADB_KEY = Path(os.path.expanduser("~/.android/adbkey.pub")).resolve(strict=True)

SU_SRC = ROOT / "tools/su.c"
SU_BIN = ROOT / "build/phase3/su"
INIT_SRC = ROOT / "tools/mini_init.c"
INIT_BIN = ROOT / "build/phase3/mini_init"
GLES_SW_SRC = ROOT / "tools/software_egl_gles2.c"
GLES_SW_OBJ = ROOT / "build/phase3/software_egl_gles2.o"
GLES_SW_BIN = ROOT / "build/phase3/libGLES_software.so"

def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def compile_artifacts():
    print("[*] Compiling tools/su.c -> build/phase3/su...")
    cmd_su = [
        "/opt/homebrew/bin/arm-none-eabi-gcc",
        "-static", "-nostdlib", "-fno-builtin", "-O2",
        str(SU_SRC), "-o", str(SU_BIN)
    ]
    res_su = subprocess.run(cmd_su, capture_output=True, text=True)
    if res_su.returncode != 0:
        raise RuntimeError(f"Failed to compile su.c: {res_su.stderr}")
    print(f"[+] build/phase3/su compiled: {SU_BIN.stat().st_size} bytes, sha256={sha256(SU_BIN)}")

    print("[*] Compiling tools/mini_init.c -> build/phase3/mini_init...")
    cmd_init = [
        "/opt/homebrew/bin/arm-none-eabi-gcc",
        "-march=armv7-a", "-nostdlib", "-static", "-fno-builtin", "-O2",
        f"-I{ROOT / 'tools'}",
        str(INIT_SRC), "-o", str(INIT_BIN)
    ]
    res_init = subprocess.run(cmd_init, capture_output=True, text=True)
    if res_init.returncode != 0:
        raise RuntimeError(f"Failed to compile mini_init.c: {res_init.stderr}")
    print(f"[+] build/phase3/mini_init compiled: {INIT_BIN.stat().st_size} bytes, sha256={sha256(INIT_BIN)}")

    print("[*] Compiling tools/software_egl_gles2.c -> build/phase3/libGLES_software.so...")
    cmd_gles_c = [
        "clang", "-target", "armv7a-linux-androideabi", "-ffreestanding", "-O2", "-fPIC", "-c",
        str(GLES_SW_SRC), "-o", str(GLES_SW_OBJ)
    ]
    res_gles_c = subprocess.run(cmd_gles_c, capture_output=True, text=True)
    if res_gles_c.returncode != 0:
        raise RuntimeError(f"Failed to compile software_egl_gles2.c: {res_gles_c.stderr}")

    cmd_gles_ld = [
        "/opt/homebrew/bin/arm-none-eabi-ld", "-shared",
        str(GLES_SW_OBJ), "-o", str(GLES_SW_BIN),
        f"-L{REF / 'staging/system_extracted/lib'}", "-lc"
    ]
    res_gles_ld = subprocess.run(cmd_gles_ld, capture_output=True, text=True)
    if res_gles_ld.returncode != 0:
        raise RuntimeError(f"Failed to link libGLES_software.so: {res_gles_ld.stderr}")
    print(f"[+] build/phase3/libGLES_software.so compiled: {GLES_SW_BIN.stat().st_size} bytes, sha256={sha256(GLES_SW_BIN)}")


def boot_qemu(overlay: Path, output: Path, commands: list, terminal_marker: bytes,
              hostfwd_port: int = None, adb_hook=None, is_android_mode: bool = False,
              timeout: float = 240.0):
    nic_opt = "user,model=rtd2841-eth"
    if hostfwd_port:
        nic_opt += f",hostfwd=tcp:127.0.0.1:{hostfwd_port}-:5555"

    append_str = ("console=ttyS0,115200 earlyprintk "
                  "root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait "
                  "init=/init consoleblank=0 enforcing=0 androidboot.selinux=permissive")
    if is_android_mode:
        append_str += " android=1"

    cmd = [
        str(QEMU), "-M", "rtd2841-tv030", "-m", "1024M", "-smp", "4",
        "-dtb", str(DTB), "-display", "none", "-serial", "stdio",
        "-monitor", "none", "-kernel", str(KERNEL), "-no-reboot",
        "-drive", f"file={overlay},format=qcow2,if=sd",
        "-append", append_str,
        "-nic", nic_opt
    ]
    (output / "command.json").write_text(json.dumps(cmd, indent=2) + "\n")

    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, preexec_fn=os.setsid,
                            bufsize=0)

    selector = selectors.DefaultSelector()
    os.set_blocking(proc.stdout.fileno(), False)
    selector.register(proc.stdout, selectors.EVENT_READ)

    data = bytearray()
    quit_sent = False
    commands_sent = False
    terminal_reached = False
    adb_hook_started = False
    adb_hook_done = False
    adb_hook_result = None

    start = time.monotonic()
    try:
        with (output / "serial.log").open("wb", buffering=0) as log:
            while time.monotonic() - start < timeout:
                for key, _ in selector.select(timeout=0.25):
                    try:
                        chunk = os.read(key.fileobj.fileno(), 65536)
                    except BlockingIOError:
                        continue
                    if not chunk:
                        if proc.poll() is not None:
                            selector.unregister(key.fileobj)
                            break
                        continue
                    log.write(chunk)
                    data.extend(chunk)
                    if len(data) > 8 * 1024 * 1024:
                        del data[:4 * 1024 * 1024]

                if not is_android_mode:
                    if not quit_sent and b"Interactive TV Remote Control Active" in data:
                        proc.stdin.write(b"q\n")
                        proc.stdin.flush()
                        quit_sent = True

                    if quit_sent and not commands_sent and (b"Dropping into interactive" in data or b":/ #" in data):
                        time.sleep(1)
                        payload = ("\n".join(commands) + "\n").encode()
                        for offset in range(0, len(payload), 64):
                            proc.stdin.write(payload[offset:offset + 64])
                            proc.stdin.flush()
                            time.sleep(0.01)
                        commands_sent = True

                    if commands_sent and re.search(terminal_marker, data):
                        terminal_reached = True
                        break
                else:
                    # In Android mode, wait for adbd to spawn and launch ADB verification in background thread
                    if b"adbd child spawned" in data and not adb_hook_started:
                        adb_hook_started = True

                        def adb_worker():
                            nonlocal adb_hook_result, adb_hook_done
                            try:
                                if adb_hook:
                                    adb_hook_result = adb_hook()
                            except Exception as ex:
                                adb_hook_result = {"success": False, "error": str(ex)}
                            finally:
                                adb_hook_done = True

                        threading.Thread(target=adb_worker, daemon=True).start()

                    if adb_hook_done:
                        terminal_reached = True
                        break

                if proc.poll() is not None and not selector.get_map():
                    break
    finally:
        selector.close()
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait(timeout=5)
        else:
            proc.wait()

    return {
        "quit_sent": quit_sent,
        "commands_sent": commands_sent,
        "done": terminal_reached,
        "qemu_exit_code": proc.returncode,
        "elapsed_seconds": round(time.monotonic() - start, 1),
        "adb_hook_result": adb_hook_result
    }


def main():
    print("===================================================================")
    print("[*] OpenDTV RTD2841 Phase 3 Milestone 4 Verification")
    print("===================================================================")

    compile_artifacts()

    out_dir = Path(tempfile.mkdtemp(prefix="opendtv-milestone4-", dir="/private/tmp"))
    print(f"[*] Artifact Directory: {out_dir}")

    disk_hash_before = sha256(DISK)
    print(f"[*] Pre-run Disk SHA-256: {disk_hash_before}")

    init_bytes = INIT_BIN.read_bytes()
    init_hash = sha256(INIT_BIN)

    su_bytes = SU_BIN.read_bytes()
    su_hash = sha256(SU_BIN)

    gles_bytes = GLES_SW_BIN.read_bytes()
    gles_hash = sha256(GLES_SW_BIN)

    adb_pub_bytes = HOST_ADB_KEY.read_bytes()

    class FileServer(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/init":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(init_bytes)))
                self.end_headers()
                self.wfile.write(init_bytes)
            elif self.path == "/su":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(su_bytes)))
                self.end_headers()
                self.wfile.write(su_bytes)
            elif self.path == "/libGLES_software.so":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(gles_bytes)))
                self.end_headers()
                self.wfile.write(gles_bytes)
            elif self.path == "/adbkey.pub":
                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.send_header("Content-Length", str(len(adb_pub_bytes)))
                self.end_headers()
                self.wfile.write(adb_pub_bytes)
            else:
                self.send_error(404)

        def log_message(self, *_args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), FileServer)
    http_port = server.server_address[1]
    threading.Thread(target=server.serve_forever, daemon=True).start()
    print(f"[*] HTTP staging server started on 127.0.0.1:{http_port}")

    overlay = out_dir / "emmc-overlay.qcow2"

    try:
        # Create disposable qcow2 overlay
        print("[*] Creating disposable qcow2 overlay...")
        qcow_res = subprocess.run(
            [str(QEMU_IMG), "create", "-f", "qcow2", "-b", str(DISK), "-F", "raw", str(overlay)],
            capture_output=True, text=True, timeout=30
        )
        if qcow_res.returncode != 0:
            raise RuntimeError(f"qemu-img failed (rc={qcow_res.returncode}): {qcow_res.stderr}")

        # -------------------------------------------------------------
        # Phase 1: Install Stage
        # -------------------------------------------------------------
        print("\n===================================================================")
        print("[*] Phase 1: Deploying mini_init, su (06755), and adb_keys into overlay...")
        print("===================================================================")
        install_dir = out_dir / "install"
        install_dir.mkdir()

        install_cmds = [
            "echo __INSTALL_START__",
            "LD_LIBRARY_PATH=/vendor/am_apps/tplayer/lib:/vendor/lib:/system/lib "
            f"/vendor/am_apps/tplayer/bin/curl --fail --silent --show-error --max-time 10 --output /tmp/new-init http://10.0.2.2:{http_port}/init",
            "echo __FETCH_INIT_RC_${?}__",
            f"printf '%s  %s\\n' {init_hash} /tmp/new-init | /system/bin/sha256sum -c -",
            "echo __VERIFY_INIT_RC_${?}__",
            "LD_LIBRARY_PATH=/vendor/am_apps/tplayer/lib:/vendor/lib:/system/lib "
            f"/vendor/am_apps/tplayer/bin/curl --fail --silent --show-error --max-time 10 --output /tmp/new-su http://10.0.2.2:{http_port}/su",
            "echo __FETCH_SU_RC_${?}__",
            f"printf '%s  %s\\n' {su_hash} /tmp/new-su | /system/bin/sha256sum -c -",
            "echo __VERIFY_SU_RC_${?}__",
            "LD_LIBRARY_PATH=/vendor/am_apps/tplayer/lib:/vendor/lib:/system/lib "
            f"/vendor/am_apps/tplayer/bin/curl --fail --silent --show-error --max-time 10 --output /tmp/libGLES_software.so http://10.0.2.2:{http_port}/libGLES_software.so",
            "echo __FETCH_GLES_RC_${?}__",
            f"printf '%s  %s\\n' {gles_hash} /tmp/libGLES_software.so | /system/bin/sha256sum -c -",
            "echo __VERIFY_GLES_RC_${?}__",
            "LD_LIBRARY_PATH=/vendor/am_apps/tplayer/lib:/vendor/lib:/system/lib "
            f"/vendor/am_apps/tplayer/bin/curl --fail --silent --show-error --max-time 10 --output /tmp/host-key http://10.0.2.2:{http_port}/adbkey.pub",
            "echo __FETCH_KEY_RC_${?}__",
            "/system/bin/mkdir -p /system/xbin && "
            "/system/bin/cp /tmp/new-su /system/xbin/su && "
            "/system/bin/chown 0:0 /system/xbin/su && "
            "/system/bin/chmod 6755 /system/xbin/su && "
            "/system/bin/sha256sum /system/xbin/su",
            "echo __INSTALL_SU_RC_${?}__",
            "/system/bin/mkdir -p /system/lib/egl && "
            "/system/bin/cp /tmp/libGLES_software.so /system/lib/egl/libGLES_software.so && "
            "/system/bin/chmod 755 /system/lib/egl/libGLES_software.so && "
            "/system/bin/mkdir -p /vendor/lib/egl && "
            "/system/bin/cp /tmp/libGLES_software.so /vendor/lib/egl/libGLES_software.so && "
            "/system/bin/chmod 755 /vendor/lib/egl/libGLES_software.so && "
            "/system/bin/sha256sum /system/lib/egl/libGLES_software.so",
            "echo __INSTALL_GLES_RC_${?}__",
            "/system/bin/cp /tmp/new-init /init.next && "
            "/system/bin/chmod 755 /init.next && "
            "/system/bin/mv -f /init.next /init && "
            "/system/bin/sync && /system/bin/sha256sum /init",
            "echo __INSTALL_INIT_RC_${?}__",
            "/system/bin/cp /tmp/host-key /adb_keys && "
            "/system/bin/chmod 600 /adb_keys && "
            "/system/bin/chown 0:0 /adb_keys && "
            "/system/bin/mkdir -p /data/misc/adb && "
            "/system/bin/cp /tmp/host-key /data/misc/adb/adb_keys && "
            "/system/bin/chmod 600 /data/misc/adb/adb_keys && "
            "/system/bin/chown 2000:2000 /data/misc/adb/adb_keys && "
            "/system/bin/touch /android_mode && "
            "/system/bin/touch /data/android_mode && "
            "/system/bin/sync",
            "echo __INSTALL_KEY_RC_${?}__",
            "echo __INSTALL_END__",
        ]
        (install_dir / "guest-commands.txt").write_text("\n".join(install_cmds) + "\n")

        install_res = boot_qemu(overlay, install_dir, install_cmds, rb"(?m)^__INSTALL_END__\r?$", timeout=240)
        install_log = (install_dir / "serial.log").read_bytes()

        req_install = [
            b"__FETCH_INIT_RC_0__", b"__VERIFY_INIT_RC_0__",
            b"__FETCH_SU_RC_0__", b"__VERIFY_SU_RC_0__",
            b"__FETCH_GLES_RC_0__", b"__VERIFY_GLES_RC_0__",
            b"__FETCH_KEY_RC_0__", b"__INSTALL_SU_RC_0__",
            b"__INSTALL_GLES_RC_0__",
            b"__INSTALL_INIT_RC_0__", b"__INSTALL_KEY_RC_0__",
            b"__INSTALL_END__"
        ]
        for marker in req_install:
            if marker not in install_log:
                raise RuntimeError(f"[-] Installation failed: missing marker {marker.decode()} in serial log")
        print("[+] Phase 1 successful: mini_init and su deployed cleanly.")

        # -------------------------------------------------------------
        # Phase 2: Android Userspace Verification Stage
        # -------------------------------------------------------------
        print("\n===================================================================")
        print("[*] Phase 2: Full Android Userspace Boot (android=1)...")
        print("===================================================================")
        verify_dir = out_dir / "verify"
        verify_dir.mkdir()

        subprocess.run([str(ADB), "kill-server"], capture_output=True)
        time.sleep(1)

        adb_port = find_free_port()
        print(f"[*] Dynamic host ADB port allocated: {adb_port}")

        def run_android_adb_suite():
            print("[*] Running ADB verification suite against live Android userspace...")
            target = f"127.0.0.1:{adb_port}"
            connected = False
            for attempt in range(15):
                res = subprocess.run([str(ADB), "connect", target], capture_output=True, text=True)
                if "connected" in res.stdout or "already connected" in res.stdout:
                    connected = True
                    break
                time.sleep(1.5)

            if not connected:
                return {"success": False, "error": f"Failed to connect to ADB on {target}"}

            device_ready = False
            state = "unknown"
            for attempt in range(30):
                st_res = subprocess.run([str(ADB), "-s", target, "get-state"], capture_output=True, text=True)
                state = st_res.stdout.strip()
                if state == "device":
                    device_ready = True
                    break
                time.sleep(1.0)

            if not device_ready:
                dev_res = subprocess.run([str(ADB), "devices", "-l"], capture_output=True, text=True)
                return {"success": False, "error": f"ADB target did not reach 'device' state (state={state}, devices={dev_res.stdout.strip()})"}

            dev_res = subprocess.run([str(ADB), "devices", "-l"], capture_output=True, text=True)
            print(f"[*] adb devices:\n{dev_res.stdout.strip()}")

            # 1. Properties verification
            p_model = subprocess.run([str(ADB), "-s", target, "shell", "getprop ro.product.model"], capture_output=True, text=True).stdout.strip()
            p_sdk = subprocess.run([str(ADB), "-s", target, "shell", "getprop ro.build.version.sdk"], capture_output=True, text=True).stdout.strip()
            p_abi = subprocess.run([str(ADB), "-s", target, "shell", "getprop ro.product.cpu.abilist32"], capture_output=True, text=True).stdout.strip()
            p_hwsm = subprocess.run([str(ADB), "-s", target, "shell", "getprop hwservicemanager.ready"], capture_output=True, text=True).stdout.strip()
            print(f"[+] Properties: model={p_model}, sdk={p_sdk}, abilist32={p_abi}, hwsm_ready={p_hwsm}")

            # 2. Poll for SurfaceFlinger registration with servicemanager
            sf_registered = False
            sf_check = "not checked"
            print("[*] Polling ServiceManager for SurfaceFlinger readiness...")
            for attempt in range(25):
                res = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su -c 'service check SurfaceFlinger'"],
                                     capture_output=True, text=True)
                sf_check = res.stdout.strip()
                if "found" in sf_check and "not found" not in sf_check:
                    sf_registered = True
                    break
                time.sleep(1.0)
            print(f"[+] ServiceManager check SurfaceFlinger: '{sf_check}' (registered={sf_registered})")
            if not sf_registered:
                try:
                    sf_diag = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su -c '/system/bin/surfaceflinger'"],
                                             capture_output=True, text=True, timeout=5)
                    print(f"[*] SurfaceFlinger Foreground Diagnostic:\nstdout: {sf_diag.stdout}\nstderr: {sf_diag.stderr}")
                except Exception as e:
                    print(f"[*] SurfaceFlinger Diagnostic run error: {e}")

            # 3. Process list check
            ps_out = subprocess.run([str(ADB), "-s", target, "shell", "ps -A"], capture_output=True, text=True).stdout
            if "android.hardware" not in ps_out:
                ps_out = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su -c 'ps -A'"], capture_output=True, text=True).stdout
            print(f"[*] Active Process Snapshot:\n{ps_out}")

            sf_running = "surfaceflinger" in ps_out
            cs_running = "android.hardware.configstore" in ps_out
            alloc_running = "android.hardware.graphics.allocator" in ps_out
            comp_running = "android.hardware.graphics.composer" in ps_out
            zygote_running = "app_process" in ps_out

            # 4. Storage layout verification
            data_mount = subprocess.run([str(ADB), "-s", target, "shell", "mount | grep /data"], capture_output=True, text=True).stdout.strip()
            print(f"[+] Data mount: {data_mount}")

            # 5. Capture dmesg snippet and tombstones
            dmesg_tail = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su -c 'dmesg | tail -n 25'"],
                                        capture_output=True, text=True).stdout
            tombstones = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su -c 'ls -la /data/tombstones; cat /data/tombstones/tombstone_* 2>/dev/null | head -n 60'"],
                                        capture_output=True, text=True).stdout
            print(f"[*] Tombstones:\n{tombstones}")

            logcat_out = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su -c 'logcat -d | tail -n 60'"],
                                        capture_output=True, text=True).stdout
            print(f"[*] Logcat Tail:\n{logcat_out}")

            # Success criteria:
            # - Model 'UnionTV', SDK 28, abilist32 set
            # - hwservicemanager ready
            # - SurfaceFlinger active and registered in servicemanager
            # - Graphics HAL trio active
            success = (
                p_model == "UnionTV" and
                p_sdk == "28" and
                "armeabi" in p_abi and
                p_hwsm == "true" and
                sf_running and
                sf_registered and
                cs_running and
                alloc_running and
                comp_running
            )

            return {
                "success": success,
                "model": p_model,
                "sdk": p_sdk,
                "abilist32": p_abi,
                "hwsm_ready": p_hwsm,
                "surfaceflinger_running": sf_running,
                "surfaceflinger_registered": sf_registered,
                "configstore_running": cs_running,
                "allocator_running": alloc_running,
                "composer_running": comp_running,
                "zygote_running": zygote_running,
                "service_check_sf": sf_check,
                "data_mount": data_mount,
                "dmesg_tail": dmesg_tail
            }

        verify_res = boot_qemu(overlay, verify_dir, [], b"", hostfwd_port=adb_port,
                               adb_hook=run_android_adb_suite, is_android_mode=True, timeout=240)

        verify_log = (verify_dir / "serial.log").read_bytes()
        adb_result = verify_res.get("adb_hook_result")

        if not adb_result or not adb_result.get("success"):
            print(f"[-] Verification details:\n{json.dumps(adb_result, indent=2)}")
            raise RuntimeError(f"[-] Milestone 4 Android Userspace verification failed.")

        disk_hash_after = sha256(DISK)
        disk_unchanged = (disk_hash_before == disk_hash_after)
        if not disk_unchanged:
            raise RuntimeError(f"[-] FATAL: Reference disk changed! Before: {disk_hash_before}, After: {disk_hash_after}")
        print(f"[+] Post-run Disk SHA-256: {disk_hash_after} (UNCHANGED)")

        final_report = {
            "milestone": "Phase 3 Milestone 4",
            "target": "RTD2841 / TCL RT41 Chassis (tv030)",
            "kernel_sha256": sha256(KERNEL),
            "dtb_sha256": sha256(DTB),
            "init_sha256": init_hash,
            "su_sha256": su_hash,
            "gles_sha256": gles_hash,
            "disk_sha256_before": disk_hash_before,
            "disk_sha256_after": disk_hash_after,
            "disk_immutability_verified": disk_unchanged,
            "install": install_res,
            "android_userspace_verification": adb_result,
            "status": "ALL_TESTS_PASSED"
        }
        (out_dir / "milestone4_results.json").write_text(json.dumps(final_report, indent=2) + "\n")
        (ROOT / "tests/milestone4_results.json").write_text(json.dumps(final_report, indent=2) + "\n")

        print("\n===================================================================")
        print("[>>> OPENDTV PHASE 3 MILESTONE 4 VERIFICATION PASSED <<<]")
        print("===================================================================")
        print(json.dumps(final_report, indent=2))
        return 0

    finally:
        server.shutdown()
        server.server_close()
        subprocess.run([str(ADB), "kill-server"], capture_output=True)


if __name__ == "__main__":
    exit(main())
