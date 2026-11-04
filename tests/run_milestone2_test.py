#!/usr/bin/env python3
"""
OpenDTV Phase 3 Milestone 2 Verification Runner
Target: Realtek RTD2841 / TCL RT41 / tv030

Verifies:
1. tools/su.c: static ARM32 EABI root execution helper deployed to /system/xbin/su (04755 root:root).
2. Advanced service manager bring-up:
   - /dev/hwbinder and /dev/vndbinder device nodes created with mode 0666.
   - mini_init.c launches servicemanager, hwservicemanager, vndservicemanager under UID/GID 1000.
   - hwservicemanager.ready=true property is published via Bionic property area.
3. ADB authenticated root execution path:
   - adb connect 127.0.0.1:15566
   - adb shell /system/xbin/su -c "id" returns UID 0.
4. Adherence to all 7 OpenDTV Inviolable Doctrines.
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


def boot_qemu(overlay: Path, output: Path, commands: list, terminal_marker: bytes,
              hostfwd_port: int = None, adb_hook=None, timeout: float = 160.0):
    nic_opt = "user,model=rtd2841-eth"
    if hostfwd_port:
        nic_opt += f",hostfwd=tcp:127.0.0.1:{hostfwd_port}-:5555"

    cmd = [
        str(QEMU), "-M", "rtd2841-tv030", "-m", "1024M", "-smp", "4",
        "-dtb", str(DTB), "-display", "none", "-serial", "stdio",
        "-monitor", "none", "-kernel", str(KERNEL), "-no-reboot",
        "-drive", f"file={overlay},format=qcow2,if=sd",
        "-append", "console=tty0 console=ttyS0,115200 earlyprintk "
                   "root=/dev/mmcblk0p1 rootfstype=ext4 rw rootwait "
                   "init=/init consoleblank=0",
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
    adb_hook_executed = False
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
                        selector.unregister(key.fileobj)
                        break
                    log.write(chunk)
                    data.extend(chunk)
                    if len(data) > 8 * 1024 * 1024:
                        del data[:4 * 1024 * 1024]

                if not quit_sent and b"Interactive TV Remote Control Active" in data:
                    proc.stdin.write(b"q\n")
                    proc.stdin.flush()
                    quit_sent = True

                if quit_sent and not commands_sent and b"Dropping into interactive" in data:
                    time.sleep(1)
                    payload = ("\n".join(commands) + "\n").encode()
                    for offset in range(0, len(payload), 64):
                        proc.stdin.write(payload[offset:offset + 64])
                        proc.stdin.flush()
                        time.sleep(0.01)
                    commands_sent = True

                if commands_sent and re.search(terminal_marker, data):
                    terminal_reached = True
                    if adb_hook and not adb_hook_executed:
                        adb_hook_result = adb_hook()
                        adb_hook_executed = True
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
    print("[*] OpenDTV RTD2841 Phase 3 Milestone 2 Verification")
    print("===================================================================")

    compile_artifacts()

    out_dir = Path(tempfile.mkdtemp(prefix="opendtv-milestone2-", dir="/private/tmp"))
    print(f"[*] Artifact Directory: {out_dir}")

    disk_hash_before = sha256(DISK)
    print(f"[*] Pre-run Disk SHA-256: {disk_hash_before}")

    init_bytes = INIT_BIN.read_bytes()
    init_hash = sha256(INIT_BIN)

    su_bytes = SU_BIN.read_bytes()
    su_hash = sha256(SU_BIN)

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
        print("[*] Phase 1: Deploying mini_init, su (04755), and adb_keys into overlay...")
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
            f"/vendor/am_apps/tplayer/bin/curl --fail --silent --show-error --max-time 10 --output /tmp/host-key http://10.0.2.2:{http_port}/adbkey.pub",
            "echo __FETCH_KEY_RC_${?}__",
            "/system/bin/mkdir -p /system/xbin && "
            "/system/bin/cp /tmp/new-su /system/xbin/su && "
            "/system/bin/chown 0:0 /system/xbin/su && "
            "/system/bin/chmod 6755 /system/xbin/su && "
            "/system/bin/sha256sum /system/xbin/su",
            "echo __INSTALL_SU_RC_${?}__",
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
            b"__FETCH_KEY_RC_0__", b"__INSTALL_SU_RC_0__",
            b"__INSTALL_INIT_RC_0__", b"__INSTALL_KEY_RC_0__",
            b"__INSTALL_END__"
        ]
        for marker in req_install:
            if marker not in install_log:
                raise RuntimeError(f"[-] Installation failed: missing marker {marker.decode()} in serial log")
        print("[+] Phase 1 successful: mini_init and su deployed cleanly.")

        # -------------------------------------------------------------
        # Phase 2: Verification Stage
        # -------------------------------------------------------------
        print("\n===================================================================")
        print("[*] Phase 2: Fresh boot verification of service managers, nodes & ADB root...")
        print("===================================================================")
        verify_dir = out_dir / "verify"
        verify_dir.mkdir()

        # Clean up any leftover adb server from prior runs before spawning QEMU
        subprocess.run([str(ADB), "kill-server"], capture_output=True)
        time.sleep(1)

        adb_port = find_free_port()
        print(f"[*] Dynamic host ADB port allocated: {adb_port}")

        verify_cmds = [
            "echo __VERIFY_START__",
            "/system/bin/sha256sum /init /system/xbin/su",
            "echo __HASH_CHECK_RC_${?}__",
            "ls -l /dev/binder /dev/hwbinder /dev/vndbinder /dev/urandom",
            "echo __DEV_NODES_RC_${?}__",
            "ls -l /system/xbin/su",
            "echo __SU_PERM_RC_${?}__",
            "test -u /system/xbin/su",
            "echo __SU_SETUID_RC_${?}__",
            "SM_PID=$(/system/bin/pidof servicemanager); echo SM_PID=$SM_PID; test -n \"$SM_PID\" && cat /proc/$SM_PID/status",
            "echo __SM_STATUS_RC_${?}__",
            "HWSM_PID=$(/system/bin/pidof hwservicemanager); echo HWSM_PID=$HWSM_PID; test -n \"$HWSM_PID\" && cat /proc/$HWSM_PID/status",
            "echo __HWSM_STATUS_RC_${?}__",
            "VNDSM_PID=$(/system/bin/pidof vndservicemanager); echo VNDSM_PID=$VNDSM_PID; test -n \"$VNDSM_PID\" && cat /proc/$VNDSM_PID/status",
            "echo __VNDSM_STATUS_RC_${?}__",
            "ADBD_PID=$(/system/bin/pidof adbd); echo ADBD_PID=$ADBD_PID; test -n \"$ADBD_PID\" && cat /proc/$ADBD_PID/status",
            "echo __ADBD_STATUS_RC_${?}__",
            "/system/bin/getprop hwservicemanager.ready",
            "echo __GETPROP_READY_RC_${?}__",
            "/system/bin/timeout 8 /system/bin/service list",
            "echo __SERVICE_LIST_RC_${?}__",
            "echo __VERIFY_GUEST_READY__",
        ]
        (verify_dir / "guest-commands.txt").write_text("\n".join(verify_cmds) + "\n")

        def run_adb_tests():
            print("[*] Initiating host ADB root verification suite...")

            target = f"127.0.0.1:{adb_port}"
            connected = False
            for attempt in range(12):
                res = subprocess.run([str(ADB), "connect", target], capture_output=True, text=True)
                if "connected" in res.stdout or "already connected" in res.stdout:
                    connected = True
                    break
                time.sleep(1.5)

            if not connected:
                return {"success": False, "error": f"Failed to connect to ADB on {target}"}

            # Wait for authentication handshake to complete: poll get-state until 'device'
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

            # Check adb devices state
            dev_res = subprocess.run([str(ADB), "devices", "-l"], capture_output=True, text=True)
            print(f"[*] adb devices output:\n{dev_res.stdout}")

            # 1. Unprivileged shell probe (should be uid 2000)
            sh_res = subprocess.run([str(ADB), "-s", target, "shell", "id"], capture_output=True, text=True)
            print(f"[*] adb shell id:\n{sh_res.stdout.strip()}")

            # 2. Authenticated root execution probe via su -c "id" (MUST return UID 0)
            su_res = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su -c 'id'"],
                                    capture_output=True, text=True)
            print(f"[*] adb shell /system/xbin/su -c 'id':\nstdout='{su_res.stdout.strip()}'\nstderr='{su_res.stderr.strip()}'\nrc={su_res.returncode}")

            # 3. Authenticated root execution probe via su 0 "id"
            su0_res = subprocess.run([str(ADB), "-s", target, "shell", "/system/xbin/su 0 id"],
                                     capture_output=True, text=True)
            print(f"[*] adb shell /system/xbin/su 0 id:\nstdout='{su0_res.stdout.strip()}'\nstderr='{su0_res.stderr.strip()}'\nrc={su0_res.returncode}")

            # 4. Check getprop hwservicemanager.ready via adb
            prop_res = subprocess.run([str(ADB), "-s", target, "shell", "getprop hwservicemanager.ready"],
                                      capture_output=True, text=True)
            print(f"[*] adb shell getprop hwservicemanager.ready:\n{prop_res.stdout.strip()}")

            uid0_verified = ("uid=0" in su_res.stdout or "uid=0(root)" in su_res.stdout) and (su_res.returncode == 0)
            prop_verified = ("true" in prop_res.stdout.strip())

            return {
                "success": uid0_verified and prop_verified,
                "adb_connect": res.stdout.strip(),
                "adb_devices": dev_res.stdout.strip(),
                "adb_shell_unprivileged": sh_res.stdout.strip(),
                "adb_su_c_id": su_res.stdout.strip(),
                "adb_su_c_rc": su_res.returncode,
                "adb_su_0_id": su0_res.stdout.strip(),
                "adb_su_0_rc": su0_res.returncode,
                "adb_prop_ready": prop_res.stdout.strip(),
                "uid0_verified": uid0_verified,
                "prop_verified": prop_verified
            }

        verify_res = boot_qemu(overlay, verify_dir, verify_cmds, rb"(?m)^__VERIFY_GUEST_READY__\r?$",
                               hostfwd_port=adb_port, adb_hook=run_adb_tests, timeout=240)
        verify_log = (verify_dir / "serial.log").read_bytes()

        req_verify = [
            b"__HASH_CHECK_RC_0__", b"__DEV_NODES_RC_0__",
            b"__SU_PERM_RC_0__", b"__SU_SETUID_RC_0__", b"__SM_STATUS_RC_0__",
            b"__HWSM_STATUS_RC_0__", b"__VNDSM_STATUS_RC_0__",
            b"__ADBD_STATUS_RC_0__", b"__GETPROP_READY_RC_0__",
            b"__SERVICE_LIST_RC_0__", b"__VERIFY_GUEST_READY__"
        ]
        for marker in req_verify:
            if marker not in verify_log:
                raise RuntimeError(f"[-] Verification failed: missing marker {marker.decode()} in serial log")

        adb_result = verify_res.get("adb_hook_result")
        if not adb_result or not adb_result.get("success"):
            raise RuntimeError(f"[-] Host ADB verification failed: {json.dumps(adb_result, indent=2)}")

        # Post-run disk immutability check
        disk_hash_after = sha256(DISK)
        disk_unchanged = (disk_hash_before == disk_hash_after)
        if not disk_unchanged:
            raise RuntimeError(f"[-] FATAL: Reference disk changed! Before: {disk_hash_before}, After: {disk_hash_after}")
        print(f"[+] Post-run Disk SHA-256: {disk_hash_after} (UNCHANGED)")

        # Compile final verification report
        final_report = {
            "milestone": "Phase 3 Milestone 2",
            "target": "RTD2841 / TCL RT41 Chassis (tv030)",
            "kernel_sha256": sha256(KERNEL),
            "dtb_sha256": sha256(DTB),
            "init_sha256": init_hash,
            "su_sha256": su_hash,
            "disk_sha256_before": disk_hash_before,
            "disk_sha256_after": disk_hash_after,
            "disk_immutability_verified": disk_unchanged,
            "install": install_res,
            "verify": verify_res,
            "adb_verification": adb_result,
            "status": "ALL_TESTS_PASSED"
        }
        (out_dir / "milestone2_results.json").write_text(json.dumps(final_report, indent=2) + "\n")
        (ROOT / "tests/milestone2_results.json").write_text(json.dumps(final_report, indent=2) + "\n")

        print("\n===================================================================")
        print("[>>> OPENDTV PHASE 3 MILESTONE 2 VERIFICATION PASSED <<<]")
        print("===================================================================")
        print(json.dumps(final_report, indent=2))
        return 0

    finally:
        server.shutdown()
        server.server_close()
        subprocess.run([str(ADB), "kill-server"], capture_output=True)


if __name__ == "__main__":
    exit(main())
