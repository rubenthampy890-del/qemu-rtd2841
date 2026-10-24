#!/usr/bin/env python3
"""
tests/test_send_key.py - Verification suite for scripts/send_key.py
Tests QMP protocol negotiation, HMP command fallback, UNIX domain socket
and TCP socket communication.
"""

import os
import sys
import time
import socket
import threading
import json
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO_ROOT, "scripts"))
import send_key

class TestSendKeyProtocols(unittest.TestCase):
    def setUp(self):
        self.test_sock_path = f"/tmp/test_qemu_mon_{os.getpid()}.sock"
        if os.path.exists(self.test_sock_path):
            os.unlink(self.test_sock_path)

    def tearDown(self):
        if os.path.exists(self.test_sock_path):
            try:
                os.unlink(self.test_sock_path)
            except OSError:
                pass

    def test_unix_qmp_negotiation(self):
        """Verify QMP greeting detection, capabilities negotiation, and human-monitor-command."""
        received_commands = []
        stop_server = threading.Event()

        def qmp_server():
            srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            srv.bind(self.test_sock_path)
            srv.listen(1)
            srv.settimeout(2.0)
            try:
                conn, _ = srv.accept()
                greeting = json.dumps({
                    "QMP": {
                        "version": {"qemu": {"micro": 0, "minor": 2, "major": 8}, "package": "v8.2.0"},
                        "capabilities": []
                    }
                }) + "\r\n"
                conn.sendall(greeting.encode("utf-8"))

                f = conn.makefile("r", encoding="utf-8")
                # Expect qmp_capabilities
                line1 = f.readline()
                cmd1 = json.loads(line1)
                received_commands.append(cmd1)
                conn.sendall(b'{"return": {}}\r\n')

                # Expect human-monitor-command
                line2 = f.readline()
                cmd2 = json.loads(line2)
                received_commands.append(cmd2)
                conn.sendall(b'{"return": ""}\r\n')

                conn.close()
            except Exception as e:
                pass
            finally:
                srv.close()

        th = threading.Thread(target=qmp_server)
        th.start()
        time.sleep(0.1)

        os.environ["QEMU_MONITOR_SOCK"] = self.test_sock_path
        try:
            ok, target = send_key.send_qemu_key("ret")
            self.assertTrue(ok, f"send_qemu_key failed: {target}")
            self.assertIn("QMP", target)
            self.assertIn(self.test_sock_path, target)
        finally:
            os.environ.pop("QEMU_MONITOR_SOCK", None)
            th.join()

        self.assertEqual(len(received_commands), 2)
        self.assertEqual(received_commands[0].get("execute"), "qmp_capabilities")
        self.assertEqual(received_commands[1].get("execute"), "human-monitor-command")
        self.assertEqual(received_commands[1]["arguments"]["command-line"], "sendkey ret")

    def test_unix_hmp_plain(self):
        """Verify plaintext HMP banner and direct sendkey execution."""
        received_lines = []

        def hmp_server():
            srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            srv.bind(self.test_sock_path)
            srv.listen(1)
            srv.settimeout(2.0)
            try:
                conn, _ = srv.accept()
                conn.sendall(b"QEMU 8.2.0 monitor - type 'help' for more information\r\n(qemu) ")
                f = conn.makefile("r", encoding="utf-8")
                line = f.readline()
                received_lines.append(line.strip())
                conn.sendall(b"(qemu) ")
                conn.close()
            except Exception:
                pass
            finally:
                srv.close()

        th = threading.Thread(target=hmp_server)
        th.start()
        time.sleep(0.1)

        os.environ["QEMU_MONITOR_SOCK"] = self.test_sock_path
        try:
            ok, target = send_key.send_qemu_key("up")
            self.assertTrue(ok, f"send_qemu_key failed: {target}")
            self.assertIn("HMP", target)
            self.assertIn(self.test_sock_path, target)
        finally:
            os.environ.pop("QEMU_MONITOR_SOCK", None)
            th.join()

        self.assertEqual(len(received_lines), 1)
        self.assertEqual(received_lines[0], "sendkey up")

    def test_tcp_qmp_negotiation(self):
        """Verify QMP over TCP socket."""
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.bind(("127.0.0.1", 0))
        tcp_port = srv.getsockname()[1]
        srv.listen(1)
        srv.settimeout(2.0)

        received_commands = []
        def tcp_server():
            try:
                conn, _ = srv.accept()
                greeting = json.dumps({
                    "QMP": {
                        "version": {"qemu": {"micro": 0, "minor": 2, "major": 8}, "package": "v8.2.0"},
                        "capabilities": []
                    }
                }) + "\r\n"
                conn.sendall(greeting.encode("utf-8"))

                f = conn.makefile("r", encoding="utf-8")
                line1 = f.readline()
                received_commands.append(json.loads(line1))
                conn.sendall(b'{"return": {}}\r\n')

                line2 = f.readline()
                received_commands.append(json.loads(line2))
                conn.sendall(b'{"return": ""}\r\n')
                conn.close()
            except Exception:
                pass
            finally:
                srv.close()

        th = threading.Thread(target=tcp_server)
        th.start()
        time.sleep(0.1)

        # Set fake sock path so unix check falls back to TCP
        os.environ["QEMU_MONITOR_SOCK"] = "/tmp/non_existent_sock_12345.sock"
        os.environ["QEMU_MONITOR_PORT"] = str(tcp_port)
        try:
            ok, target = send_key.send_qemu_key("volumedown")
            self.assertTrue(ok, f"send_qemu_key failed: {target}")
            self.assertIn("QMP", target)
            self.assertIn(str(tcp_port), target)
        finally:
            os.environ.pop("QEMU_MONITOR_SOCK", None)
            os.environ.pop("QEMU_MONITOR_PORT", None)
            th.join()

        self.assertEqual(received_commands[1]["arguments"]["command-line"], "sendkey volumedown")

    def test_offline_socket_handling(self):
        """Verify graceful error reporting when sockets are offline."""
        os.environ["QEMU_MONITOR_SOCK"] = "/tmp/non_existent_sock_99999.sock"
        os.environ["QEMU_MONITOR_PORT"] = "59999"  # Unlikely to be listening
        try:
            ok, msg = send_key.send_qemu_key("p")
            self.assertFalse(ok)
            self.assertTrue(isinstance(msg, str))
            self.assertGreater(len(msg), 0)
        finally:
            os.environ.pop("QEMU_MONITOR_SOCK", None)
            os.environ.pop("QEMU_MONITOR_PORT", None)

if __name__ == "__main__":
    unittest.main()
