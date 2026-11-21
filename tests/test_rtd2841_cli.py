#!/usr/bin/env python3
"""
tests/test_rtd2841_cli.py - Unit & Functional Test Suite for Turnkey CLI Runner
Target: OpenDTV RTD2841 / TCL RT41 Chassis Emulation (Profile tv030)

Tests:
1. Dependency Resolution & Environment Diagnostics (find_qemu, find_dtb, find_kernel, find_base_disk)
2. Port Auto-detection & Socket Hygiene (find_available_port, clean_stale_sockets)
3. Non-Destructive Copy-on-Write (QCOW2) Overlay Lifecycle (create, backing validation, force_reset)
4. Custom Firmware Ingestion & GPT Partition Assembly
5. Physical USB Export Engine & SHA-256 Checksum Verification
6. CLI Flags & Subprocess Execution (--check, --help, --reset, --headless)
7. Adherence to RTD2841 Project Inviolable Doctrines
"""

import hashlib
import json
import os
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

import rtd2841_cli as cli


class TestEnvironmentDiagnostics(unittest.TestCase):
    """Verifies that system dependencies, binaries, and kernels are accurately located."""

    def test_01_find_qemu(self):
        qemu_bin = cli.find_qemu()
        self.assertTrue(bool(qemu_bin), "qemu-system-aarch64 must be resolved")
        self.assertTrue(os.path.isfile(qemu_bin), f"Resolved QEMU binary must exist: {qemu_bin}")
        self.assertTrue(os.access(qemu_bin, os.X_OK), "QEMU binary must be executable")

        # Verify rtd2841 machine model is recognized
        res = subprocess.run([qemu_bin, "-M", "help"], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("rtd2841", res.stdout, "RTD2841 machine model must be supported by QEMU")

    def test_02_find_qemu_img(self):
        qemu_img = cli.find_qemu_img()
        self.assertTrue(bool(qemu_img), "qemu-img must be resolved")
        self.assertTrue(os.path.isfile(qemu_img), f"qemu-img must exist: {qemu_img}")
        self.assertTrue(os.access(qemu_img, os.X_OK), "qemu-img must be executable")

    def test_03_find_dtb(self):
        dtb_path = cli.find_dtb()
        self.assertTrue(bool(dtb_path), "Device tree blob must be resolved or compiled")
        self.assertTrue(os.path.isfile(dtb_path), f"DTB must exist: {dtb_path}")
        self.assertGreater(os.path.getsize(dtb_path), 1000, "DTB file size must be valid (>1KB)")

    def test_04_find_kernel(self):
        kernel_path = cli.find_kernel("android")
        self.assertTrue(bool(kernel_path), "Kernel zImage must be resolved")
        self.assertTrue(os.path.isfile(kernel_path), f"Kernel must exist: {kernel_path}")
        self.assertGreater(os.path.getsize(kernel_path), 1000000, "Kernel zImage must be >1MB")

    def test_05_find_base_disk(self):
        base_disk, is_deb = cli.find_base_disk("debloated")
        self.assertTrue(bool(base_disk), "A base eMMC disk (debloated or stock) must be resolved")
        self.assertTrue(os.path.isfile(base_disk), f"Base disk must exist: {base_disk}")
        self.assertGreaterEqual(os.path.getsize(base_disk), 1024 * 1024 * 1024, "Base disk must be >= 1GB")

    def test_06_check_environment_report(self):
        status = cli.check_environment()
        self.assertIn("qemu", status)
        self.assertIn("qemu_img", status)
        self.assertIn("dtb", status)
        self.assertIn("kernel", status)
        self.assertIn("base_disk", status)
        self.assertTrue(status["qemu"]["ok"])
        self.assertTrue(status["qemu_img"]["ok"])
        self.assertTrue(status["dtb"]["ok"])
        self.assertTrue(status["kernel"]["ok"])
        self.assertTrue(status["base_disk"]["ok"])


class TestPortAndSocketHygiene(unittest.TestCase):
    """Verifies socket port binding, auto-increment fallback, and stale socket cleanup."""

    def test_01_find_available_port(self):
        # Preferred port 5555 should be bound or fallback to an available port
        port = cli.find_available_port(5555)
        self.assertGreaterEqual(port, 5555)
        self.assertLess(port, 5580)

        # Occupy port and ensure next port is chosen
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(("127.0.0.1", port))
            s.listen(1)
            next_port = cli.find_available_port(port)
            self.assertNotEqual(port, next_port)
            self.assertEqual(next_port, port + 1)

    def test_02_clean_stale_sockets(self):
        tmp_dir = tempfile.mkdtemp(prefix="test_sock_")
        stale_sock = os.path.join(tmp_dir, "stale.sock")
        # Create dummy file as stale socket
        with open(stale_sock, "w") as f:
            f.write("dead socket")
        self.assertTrue(os.path.exists(stale_sock))

        cli.clean_stale_sockets([stale_sock])
        self.assertFalse(os.path.exists(stale_sock), "Stale socket must be unlinked")
        shutil.rmtree(tmp_dir, ignore_errors=True)


class TestCopyOnWriteOverlay(unittest.TestCase):
    """Verifies that QCOW2 COW overlays isolate guest writes and preserve base disk immutability."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_cow_")
        # Create a small dummy base raw disk (10 MiB)
        self.base_raw = os.path.join(self.test_dir, "test_base.raw")
        with open(self.base_raw, "wb") as f:
            f.truncate(10 * 1024 * 1024)
            f.seek(0)
            f.write(b"ORIGINAL_BASE_DATA")

        self.overlay_qcow2 = os.path.join(self.test_dir, "test_overlay.qcow2")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_01_create_cow_overlay(self):
        out_overlay = cli.ensure_cow_overlay(
            base_disk=self.base_raw,
            overlay_path=self.overlay_qcow2,
            force_reset=False
        )
        self.assertEqual(out_overlay, self.overlay_qcow2)
        self.assertTrue(os.path.isfile(self.overlay_qcow2))

        # Inspect overlay with qemu-img
        qemu_img = cli.find_qemu_img()
        res = subprocess.run([qemu_img, "info", "--output=json", self.overlay_qcow2],
                             capture_output=True, text=True, check=True)
        info = json.loads(res.stdout)
        self.assertEqual(info["format"], "qcow2")
        self.assertTrue(os.path.samefile(info["backing-filename"], self.base_raw))

    def test_refuse_unrelated_file_and_base_alias(self):
        path = pathlib.Path(self.overlay_qcow2)
        path.write_bytes(b"existing user data")
        for reset in (False, True):
            with self.assertRaises(ValueError):
                cli.ensure_cow_overlay(self.base_raw, str(path), force_reset=reset)
            self.assertEqual(path.read_bytes(), b"existing user data")
        with self.assertRaises(ValueError):
            cli.ensure_cow_overlay(self.base_raw, self.base_raw, force_reset=True)

    def test_prepared_image_must_match_base_and_remain_read_only(self):
        root = pathlib.Path(self.test_dir)
        output = root / "output"
        output.mkdir()
        raw = output / "emmc.raw"
        raw.write_bytes(b"expected disk")
        prepared = output / "emmc-projectivy-usb.qcow2"
        qemu_img = cli.find_qemu_img()
        cli.ensure_cow_overlay(self.base_raw, str(prepared))
        with patch.object(cli, "REPO_ROOT", root), patch.object(cli, "find_qemu_img", return_value=qemu_img):
            with self.assertRaisesRegex(ValueError, "different disk"):
                cli.ensure_cow_overlay(str(raw))
            for reset in (False, True):
                with self.assertRaisesRegex(ValueError, "prepared image"):
                    cli.ensure_cow_overlay(self.base_raw, str(prepared), force_reset=reset)
            self.assertFalse(prepared.with_name("emmc-projectivy-usb-session.qcow2").exists())

    def test_missing_explicit_partition_is_an_error(self):
        with self.assertRaises(FileNotFoundError):
            cli.mount_custom_firmware(custom_vendor=os.path.join(self.test_dir, "missing.img"))

    def test_02_force_reset_overlay(self):
        # 1. Create overlay
        cli.ensure_cow_overlay(self.base_raw, self.overlay_qcow2)

        # 2. Simulate guest writing dirty data into overlay
        with open(self.overlay_qcow2, "ab") as f:
            f.write(b"DIRTY_TRANSIENT_MODIFICATION")

        dirty_size = os.path.getsize(self.overlay_qcow2)

        # 3. Force reset
        cli.ensure_cow_overlay(self.base_raw, self.overlay_qcow2, force_reset=True)
        clean_size = os.path.getsize(self.overlay_qcow2)

        # Verify base raw is completely untouched (Doctrine of Immutability)
        with open(self.base_raw, "rb") as f:
            header = f.read(18)
            self.assertEqual(header, b"ORIGINAL_BASE_DATA")

        self.assertTrue(os.path.isfile(self.overlay_qcow2))
        self.assertLess(clean_size, dirty_size)


class TestPhysicalUSBExport(unittest.TestCase):
    """Verifies the USB export workflow, checksum generation, and post-write verification."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_usb_")
        self.usb_dest = os.path.join(self.test_dir, "USB_MNT")
        os.makedirs(self.usb_dest, exist_ok=True)

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_01_export_usb_success(self):
        # Run export_usb with target directory
        success = cli.export_usb(dest_dir=self.usb_dest)
        self.assertTrue(success, "export_usb must return True on valid destination")

        exported_img = os.path.join(self.usb_dest, "Update.img")
        exported_sha = os.path.join(self.usb_dest, "Update.img.sha256")

        self.assertTrue(os.path.isfile(exported_img), "Update.img must be present on USB destination")
        self.assertTrue(os.path.isfile(exported_sha), "Update.img.sha256 must be present on USB destination")

        # Verify SHA256 integrity matches
        with open(exported_sha, "r") as f:
            expected_hash = f.read().strip().split()[0]

        hasher = hashlib.sha256()
        with open(exported_img, "rb") as f:
            while chunk := f.read(1024 * 1024):
                hasher.update(chunk)
        actual_hash = hasher.hexdigest()

        self.assertEqual(expected_hash, actual_hash, "Exported USB image checksum must verify clean")


class TestCLIExecutionInterface(unittest.TestCase):
    """Verifies that ./run.sh and tools/rtd2841_cli.py execute cleanly with CLI flags."""

    def test_01_run_sh_executable(self):
        run_sh = REPO_ROOT / "run.sh"
        self.assertTrue(run_sh.is_file(), "run.sh must exist in repository root")
        self.assertTrue(os.access(run_sh, os.X_OK), "run.sh must have executable permissions (+x)")

    def test_02_run_sh_help(self):
        res = subprocess.run([str(REPO_ROOT / "run.sh"), "--help"], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("OpenDTV RTD2841 / TCL RT41 Unified Turnkey Runner", res.stdout)
        self.assertIn("--debloat", res.stdout)
        self.assertIn("--export-usb", res.stdout)
        self.assertIn("--system", res.stdout)
        self.assertIn("--reset", res.stdout)

    def test_03_run_sh_check(self):
        res = subprocess.run([str(REPO_ROOT / "run.sh"), "--check"], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("QEMU Engine", res.stdout)
        self.assertIn("Kernel zImage", res.stdout)
        self.assertIn("Device Tree Blob", res.stdout)
        self.assertIn("[OK]", res.stdout)

    def test_04_adb_timeout_on_inactive_port(self):
        # Using an unmapped port should timeout and exit cleanly without hanging indefinitely
        with self.assertRaises(SystemExit) as cm:
            cli.connect_adb(port=59999)
        self.assertNotEqual(cm.exception.code, 0)

    def test_05_mount_custom_firmware_missing_fallback(self):
        # Pointing to non-existent custom images must raise FileNotFoundError
        with self.assertRaises(FileNotFoundError):
            cli.mount_custom_firmware(custom_update_img="/tmp/non_existent_update.img")

    def test_06_mem_flag_and_default_ram(self):
        # Verify --mem defaults to 2048M per coordinator requirement #23
        parser = cli.parse_cli_args
        import sys
        orig_argv = sys.argv
        try:
            sys.argv = ["rtd2841_cli.py"]
            args = cli.parse_cli_args()
            self.assertEqual(args.mem, "2048M", "Default QEMU memory must be 2048M to prevent dex2oat OOM")

            sys.argv = ["rtd2841_cli.py", "--mem", "4096M"]
            args = cli.parse_cli_args()
            self.assertEqual(args.mem, "4096M", "--mem CLI flag must correctly override guest RAM size")
        finally:
            sys.argv = orig_argv

        # Check --mem in run.sh --help
        res = subprocess.run([str(REPO_ROOT / "run.sh"), "--help"], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("--mem", res.stdout)

    def test_07_universality_matrix(self):
        # 1. Debloated ROM resolution
        deb_disk, is_deb = cli.find_base_disk("debloated")
        self.assertTrue(bool(deb_disk) and os.path.isfile(deb_disk))
        self.assertTrue(is_deb)

        # 2. Stock Firmware resolution
        stock_disk, is_deb_stock = cli.find_base_disk("stock")
        self.assertTrue(bool(stock_disk) and os.path.isfile(stock_disk))
        self.assertFalse(is_deb_stock)

        # 3. Kernel resolutions for both Android and Diagnostic modes
        android_k = cli.find_kernel("android")
        diag_k = cli.find_kernel("diag")
        self.assertTrue(os.path.isfile(android_k))
        self.assertTrue(os.path.isfile(diag_k))

        # 4. Universal non-destructive overlay lifecycle for both stock and debloated bases
        tmp_dir = tempfile.mkdtemp(prefix="test_univ_")
        try:
            overlay_deb = os.path.join(tmp_dir, "overlay_deb.qcow2")
            cli.ensure_cow_overlay(deb_disk, overlay_deb, force_reset=True)
            self.assertTrue(os.path.isfile(overlay_deb))

            overlay_stock = os.path.join(tmp_dir, "overlay_stock.qcow2")
            cli.ensure_cow_overlay(stock_disk, overlay_stock, force_reset=True)
            self.assertTrue(os.path.isfile(overlay_stock))
        finally:
            shutil.rmtree(tmp_dir, ignore_errors=True)

    def test_08_overlay_flag_and_isolation(self):
        tmp_dir = tempfile.mkdtemp(prefix="test_cli_overlay_")
        try:
            custom_overlay = os.path.join(tmp_dir, "custom.qcow2")
            res = subprocess.run([
                str(REPO_ROOT / "run.sh"), "--reset", "--overlay", custom_overlay
            ], capture_output=True, text=True)
            self.assertEqual(res.returncode, 0)
            self.assertTrue(os.path.isfile(custom_overlay), "Explicit --overlay file must be created")
            default_overlay = REPO_ROOT / "build/phase3/emmc-android-overlay.qcow2"
            if default_overlay.exists():
                self.assertNotIn(str(default_overlay), res.stdout)
        finally:
            shutil.rmtree(tmp_dir, ignore_errors=True)

    def test_09_overlay_in_use_protection(self):
        tmp_dir = tempfile.mkdtemp(prefix="test_in_use_")
        try:
            dummy_overlay = os.path.join(tmp_dir, "in_use.qcow2")
            deb_disk, _ = cli.find_base_disk("debloated")
            cli.ensure_cow_overlay(deb_disk, dummy_overlay)
            self.assertTrue(os.path.isfile(dummy_overlay))

            orig_check = cli.is_file_in_use_by_qemu
            cli.is_file_in_use_by_qemu = lambda path: True
            try:
                with self.assertRaises(RuntimeError) as cm:
                    cli.ensure_cow_overlay(deb_disk, dummy_overlay, force_reset=True)
                self.assertIn("active use by running QEMU process", str(cm.exception))
            finally:
                cli.is_file_in_use_by_qemu = orig_check
        finally:
            shutil.rmtree(tmp_dir, ignore_errors=True)

    def test_10_socket_active_check(self):
        tmp_dir = tempfile.mkdtemp(prefix="test_sock_act_")
        try:
            test_sock = os.path.join(tmp_dir, "server.sock")
            self.assertFalse(cli.is_socket_active(test_sock))

            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.bind(test_sock)
            s.listen(1)
            try:
                self.assertTrue(cli.is_socket_active(test_sock))
            finally:
                s.close()

            self.assertFalse(cli.is_socket_active(test_sock))
        finally:
            shutil.rmtree(tmp_dir, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
