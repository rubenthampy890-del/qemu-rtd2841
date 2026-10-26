#!/usr/bin/env python3
"""Check per-CPU mailbox release twice, including a QMP system reset."""
import argparse
import json
import os
from pathlib import Path
import selectors
import signal
import socket
import subprocess
import tempfile
import time


def run_smp_reset(qemu, firmware, dtb, log_path, timeout=20):
    deadline = time.monotonic() + timeout
    output = bytearray()
    reset_marker = b"[SMP] All secondary mailboxes clear on reset"
    pass_marker = b"[SMP] Separate CPU1 CPU2 CPU3 releases verified"

    def remaining():
        value = deadline - time.monotonic()
        if value <= 0:
            raise TimeoutError("SMP/reset test deadline exceeded")
        return value

    with tempfile.TemporaryDirectory(prefix="rtd-smp-", dir="/tmp") as temp:
        qmp_path = str(Path(temp) / "qmp")
        cmd = [qemu, "-M", "rtd2841-tv030", "-m", "1024M", "-smp", "4",
               "-dtb", str(dtb), "-kernel", str(firmware), "-display", "none",
               "-serial", "stdio", "-monitor", "none", "-nic", "none",
               "-qmp", f"unix:{qmp_path},server=on,wait=off", "-S"]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT,
                                start_new_session=True)
        os.set_blocking(proc.stdout.fileno(), False)
        selector = selectors.DefaultSelector()
        selector.register(proc.stdout, selectors.EVENT_READ)
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        qmp_buffer = bytearray()
        passed = False
        error = None
        planned = False

        def qmp_read():
            while b"\n" not in qmp_buffer:
                sock.settimeout(remaining())
                chunk = sock.recv(4096)
                if not chunk:
                    raise RuntimeError("QMP closed unexpectedly")
                qmp_buffer.extend(chunk)
                if len(qmp_buffer) > 65536:
                    raise RuntimeError("QMP response exceeds bound")
            line, _, rest = qmp_buffer.partition(b"\n")
            qmp_buffer[:] = rest
            return json.loads(line)

        def qmp(command):
            sock.settimeout(remaining())
            sock.sendall((json.dumps({"execute": command,
                                     "id": command}) + "\n").encode())
            while True:
                reply = qmp_read()
                if reply.get("id") == command:
                    if "error" in reply or "return" not in reply:
                        raise RuntimeError(f"QMP {command}: {reply}")
                    return

        def collect_boot(number):
            while True:
                for key, _ in selector.select(min(0.1, remaining())):
                    try:
                        chunk = os.read(key.fd, 16384)
                    except BlockingIOError:
                        continue
                    if not chunk:
                        raise RuntimeError("serial EOF before SMP completion")
                    output.extend(chunk)
                    if len(output) > 1024 * 1024:
                        raise RuntimeError("serial output exceeds bound")
                if b"[FAIL]" in output:
                    raise RuntimeError("guest reported SMP failure")
                if proc.poll() is not None:
                    raise RuntimeError(f"unexpected QEMU exit {proc.returncode}")
                lines = bytes(output).replace(b"\r", b"").split(b"\n")
                if (lines.count(reset_marker) == number and
                        lines.count(pass_marker) == number):
                    return

        try:
            while True:
                remaining()
                try:
                    sock.connect(qmp_path)
                    break
                except FileNotFoundError:
                    if proc.poll() is not None:
                        raise RuntimeError(f"QEMU startup exit {proc.returncode}")
                    time.sleep(min(0.02, remaining()))
            if "QMP" not in qmp_read():
                raise RuntimeError("missing QMP greeting")
            qmp("qmp_capabilities")
            qmp("cont")
            collect_boot(1)
            qmp("system_reset")
            collect_boot(2)
            passed = True
        except (OSError, RuntimeError, ValueError) as exc:
            error = str(exc)
        finally:
            planned = proc.poll() is None
            if planned:
                try:
                    os.killpg(proc.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
            try:
                rc = proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                rc = proc.wait(timeout=2)
            selector.close()
            sock.close()
            proc.stdout.close()
            Path(log_path).write_bytes(output)
        result = {"passed": passed and planned and rc == 0,
                  "error": error, "qemu_exit_code": rc,
                  "planned_termination": planned, "command": cmd}
        Path(str(log_path) + ".json").write_text(
            json.dumps(result, indent=2) + "\n")
        return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("qemu", "firmware", "dtb", "log"):
        parser.add_argument("--" + arg, required=True)
    args = parser.parse_args()
    result = run_smp_reset(args.qemu, args.firmware, args.dtb, args.log)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["passed"] else 1)
