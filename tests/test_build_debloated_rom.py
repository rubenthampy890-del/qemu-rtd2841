#!/usr/bin/env python3
"""
tests/test_build_debloated_rom.py - Phase 4 ROM Builder Unit & Validation Test Suite
Target: Realtek RTD2841 / TCL RT41 / tv030 Android 9 Pie

Tests:
1. Tier 1 Bloatware Purging & Idempotency.
2. ProjectivyLauncher Injection & Attributes (0644, root:root, system_file:s0).
3. Pre-seeded Provisioning XMLs Structure & Parsing (v170, device_provisioned=1, user_setup_complete=1).
4. Provisioned data.tar.bz2 Packaging & Android AID Ownership (1000:1000).
5. GPT Storage Disk Geometry & CRC Integrity.
6. End-to-End Build Execution & Deep Verification of Output Artifacts:
   - output/emmc.raw (4096 MiB composite disk)
   - output/Update_RT41_Debloated.img (POSIX TAR package)
7. Second-Run Idempotency Verification.
"""

import os
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
import uuid
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

# Add tools directory to module search path
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

import build_debloated_rom as bdr


class TestTier1BloatwarePurge(unittest.TestCase):
    """Verifies that Tier 1 telemetry and bloatware packages are cleanly purged."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_debloat_")
        self.sys_dir = os.path.join(self.test_dir, "system")
        self.ven_dir = os.path.join(self.test_dir, "vendor")
        os.makedirs(self.sys_dir, exist_ok=True)
        os.makedirs(self.ven_dir, exist_ok=True)

        # Plant sample system bloatware
        for rel in bdr.TIER1_SYSTEM_REMOVALS:
            p = os.path.join(self.sys_dir, rel)
            os.makedirs(p, exist_ok=True)
            with open(os.path.join(p, "dummy.apk"), "w") as f:
                f.write("bloatware")

        # Plant sample vendor bloatware
        for rel in bdr.TIER1_VENDOR_REMOVALS:
            p = os.path.join(self.ven_dir, rel)
            if rel.endswith("netinfocap") or rel.endswith("log_out"):
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, "w") as f:
                    f.write("#!/bin/sh\n")
            else:
                os.makedirs(p, exist_ok=True)
                with open(os.path.join(p, "dummy.apk"), "w") as f:
                    f.write("bloatware")

        # Plant essential services that MUST NOT be removed
        self.essential_system = os.path.join(self.sys_dir, "product", "app", "TCLBTRcuService")
        os.makedirs(self.essential_system, exist_ok=True)
        with open(os.path.join(self.essential_system, "TCLBTRcuService.apk"), "w") as f:
            f.write("remote_service")

        self.essential_vendor = os.path.join(self.ven_dir, "app", "T_IME")
        os.makedirs(self.essential_vendor, exist_ok=True)
        with open(os.path.join(self.essential_vendor, "T_IME.apk"), "w") as f:
            f.write("keyboard")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_purge_and_idempotency(self):
        # 1. First run: all targets must be purged
        res1 = bdr.purge_tier1_bloatware(self.sys_dir, self.ven_dir)
        self.assertEqual(len(res1["purged"]), len(bdr.TIER1_SYSTEM_REMOVALS) + len(bdr.TIER1_VENDOR_REMOVALS))
        self.assertEqual(len(res1["skipped"]), 0)

        # Verify bloat is removed
        for rel in bdr.TIER1_SYSTEM_REMOVALS:
            self.assertFalse(os.path.exists(os.path.join(self.sys_dir, rel)))
        for rel in bdr.TIER1_VENDOR_REMOVALS:
            self.assertFalse(os.path.exists(os.path.join(self.ven_dir, rel)))

        # Verify essential services preserved
        self.assertTrue(os.path.exists(os.path.join(self.essential_system, "TCLBTRcuService.apk")))
        self.assertTrue(os.path.exists(os.path.join(self.essential_vendor, "T_IME.apk")))

        # 2. Second run: idempotency test (all targets must be skipped cleanly)
        res2 = bdr.purge_tier1_bloatware(self.sys_dir, self.ven_dir)
        self.assertEqual(len(res2["purged"]), 0)
        self.assertEqual(len(res2["skipped"]), len(bdr.TIER1_SYSTEM_REMOVALS) + len(bdr.TIER1_VENDOR_REMOVALS))


class TestProjectivyLauncherInjection(unittest.TestCase):
    """Verifies that ProjectivyLauncher is injected with 0644 attributes."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_launcher_")
        self.sys_dir = os.path.join(self.test_dir, "system")
        os.makedirs(self.sys_dir, exist_ok=True)

        self.mock_apk = os.path.join(self.test_dir, "mock_Projectivy.apk")
        with open(self.mock_apk, "wb") as f:
            f.write(b"PK\x03\x04mock_apk_content")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_injection(self):
        apk_path = bdr.inject_projectivy_launcher(self.sys_dir, launcher_apk_path=self.mock_apk)
        self.assertTrue(os.path.isfile(apk_path))
        self.assertEqual(apk_path, os.path.join(self.sys_dir, "app", "ProjectivyLauncher", "ProjectivyLauncher.apk"))

        # Verify file mode (0644)
        st = os.stat(apk_path)
        self.assertEqual(oct(st.st_mode & 0o777), oct(0o644))

        # Verify parent directory mode (0755)
        p_st = os.stat(os.path.dirname(apk_path))
        self.assertEqual(oct(p_st.st_mode & 0o777), oct(0o755))


class TestFirmwareInputs(unittest.TestCase):
    def test_failed_build_preserves_extracted_system(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            system = root / "original"
            app = system / "app" / "Pumpkin"
            app.mkdir(parents=True)
            vendor = root / "vendor.tar"
            vendor.touch()
            apk = root / "launcher.apk"
            apk.write_bytes(b"local test input")
            with patch.object(bdr, "build_debloated_vendor_image", side_effect=RuntimeError("stop at vendor")):
                with self.assertRaisesRegex(RuntimeError, "stop at vendor"):
                    bdr.build_debloated_rom(workspace_root=tmp, system_dir=str(system),
                                            vendor_tar_path=str(vendor), launcher_apk=str(apk),
                                            skip_emmc=True)
            self.assertTrue(app.is_dir())
            self.assertFalse((system / "app" / "ProjectivyLauncher").exists())
            self.assertFalse(list((root / "staging").glob("system-work-*")))

    def test_empty_verification_fails(self):
        with self.assertRaises(FileNotFoundError):
            bdr.verify_debloated_artifacts(None, None)


class TestProvisioningXMLs(unittest.TestCase):
    """Verifies generation and validity of Android 9 Pie SettingsProvider XMLs."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_xmls_")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_xml_generation_and_content(self):
        xml_map = bdr.generate_provisioned_xmls(self.test_dir)
        self.assertIn("settings_global.xml", xml_map)
        self.assertIn("settings_secure.xml", xml_map)
        self.assertIn("settings_system.xml", xml_map)
        self.assertIn("package-restrictions.xml", xml_map)

        # Parse settings_global.xml
        tree_g = ET.parse(xml_map["settings_global.xml"])
        root_g = tree_g.getroot()
        self.assertEqual(root_g.tag, "settings")
        self.assertEqual(root_g.get("version"), "170")

        settings_g = {el.get("name"): el.get("value") for el in root_g.findall("setting")}
        self.assertEqual(settings_g.get("device_provisioned"), "1")
        self.assertEqual(settings_g.get("setup_wizard_has_run"), "1")
        self.assertEqual(settings_g.get("adb_enabled"), "1")

        # Parse settings_secure.xml
        tree_s = ET.parse(xml_map["settings_secure.xml"])
        root_s = tree_s.getroot()
        self.assertEqual(root_s.tag, "settings")
        self.assertEqual(root_s.get("version"), "170")

        settings_s = {el.get("name"): el.get("value") for el in root_s.findall("setting")}
        self.assertEqual(settings_s.get("user_setup_complete"), "1")
        self.assertEqual(settings_s.get("tv_user_setup_complete"), "1")
        self.assertEqual(settings_s.get("install_non_market_apps"), "1")

        # Verify telemetry disable flags
        self.assertEqual(settings_g.get("package_verifier_enable"), "0")
        self.assertEqual(settings_g.get("allow_user_data_collection"), "0")
        self.assertEqual(settings_g.get("tcl_data_collection_enabled"), "0")
        self.assertEqual(settings_g.get("advertising_id_opt_out"), "1")
        self.assertEqual(settings_s.get("tcl_data_report_disable"), "1")
        self.assertEqual(settings_s.get("cbSmartTV"), "0")

        # Verify ProjectivyLauncher registered as preferred HOME
        tree_p = ET.parse(xml_map["package-restrictions.xml"])
        pref = tree_p.getroot().find("preferred-activities")
        self.assertIsNotNone(pref)
        self.assertTrue(any("com.spocky.projengmenu" in it.get("name") for it in pref.findall("item")))

        # Verify file modes are 0600
        for fpath in xml_map.values():
            st = os.stat(fpath)
            self.assertEqual(oct(st.st_mode & 0o777), oct(0o600))


class TestBiReportStubInjection(unittest.TestCase):
    """Verifies that BiReportStub is injected with 0644 attributes."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_bistub_")
        self.sys_dir = os.path.join(self.test_dir, "system")
        os.makedirs(self.sys_dir, exist_ok=True)

        self.mock_apk = os.path.join(self.test_dir, "mock_BiReportStub.apk")
        with open(self.mock_apk, "wb") as f:
            f.write(b"PK\x03\x04mock_stub_content")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_injection(self):
        apk_path = bdr.inject_bireport_stub(self.sys_dir, stub_apk_path=self.mock_apk)
        self.assertTrue(os.path.isfile(apk_path))
        self.assertEqual(apk_path, os.path.join(self.sys_dir, "app", "BiReportStub", "BiReportStub.apk"))

        # Verify file mode (0644)
        st = os.stat(apk_path)
        self.assertEqual(oct(st.st_mode & 0o777), oct(0o644))

        # Verify parent directory mode (0755)
        p_st = os.stat(os.path.dirname(apk_path))
        self.assertEqual(oct(p_st.st_mode & 0o777), oct(0o755))


class TestProvisionedDataTar(unittest.TestCase):
    """Verifies injection of XMLs into package7/data.tar.bz2 with AID_SYSTEM ownership."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_datatar_")
        self.orig_tar = os.path.join(self.test_dir, "orig_data.tar.bz2")
        self.out_tar = os.path.join(self.test_dir, "out_data.tar.bz2")

        # Create dummy original data.tar.bz2
        with tempfile.TemporaryDirectory() as td:
            app_dir = os.path.join(td, "app")
            os.makedirs(app_dir, exist_ok=True)
            with open(os.path.join(app_dir, "test.txt"), "w") as f:
                f.write("test")
            with tarfile.open(self.orig_tar, "w:bz2") as t:
                t.add(app_dir, arcname="app")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_data_tar_creation(self):
        bdr.build_provisioned_data_tar(self.orig_tar, self.out_tar)
        self.assertTrue(os.path.isfile(self.out_tar))

        with tarfile.open(self.out_tar, "r:bz2") as t:
            members = {m.name: m for m in t.getmembers()}
            self.assertIn("system/users/0/settings_global.xml", members)
            self.assertIn("system/users/0/settings_secure.xml", members)

            m_glob = members["system/users/0/settings_global.xml"]
            self.assertEqual(m_glob.uid, bdr.AID_SYSTEM)
            self.assertEqual(m_glob.gid, bdr.AID_SYSTEM)
            self.assertEqual(m_glob.uname, "system")
            self.assertEqual(m_glob.gname, "system")
            self.assertEqual(oct(m_glob.mode & 0o777), oct(0o600))


class TestGptDiskGeometry(unittest.TestCase):
    """Verifies Protective MBR and GPT calculation logic."""

    def test_gpt_structure_calculation(self):
        total_sectors = 8388608  # 4096 MiB
        partitions = [
            {
                "name": "system",
                "type_guid": bdr.LINUX_FS_DATA_GUID,
                "unique_guid": uuid.uuid4(),
                "start_lba": 2048,
                "end_lba": 4196351,
                "attributes": 0
            },
            {
                "name": "vendor",
                "type_guid": bdr.LINUX_FS_DATA_GUID,
                "unique_guid": uuid.uuid4(),
                "start_lba": 4196352,
                "end_lba": 7342079,
                "attributes": 0
            },
            {
                "name": "userdata",
                "type_guid": bdr.LINUX_FS_DATA_GUID,
                "unique_guid": uuid.uuid4(),
                "start_lba": 7342080,
                "end_lba": 8386559,
                "attributes": 0
            }
        ]

        gpt = bdr.create_gpt_structures(total_sectors, partitions)
        self.assertEqual(len(gpt["pmbr"]), 512)
        self.assertEqual(len(gpt["primary_hdr"]), 512)
        self.assertEqual(len(gpt["entries"]), 128 * 128)
        self.assertEqual(len(gpt["backup_hdr"]), 512)

        # MBR signature
        mbr_sig = struct.unpack_from("<H", gpt["pmbr"], 510)[0]
        self.assertEqual(mbr_sig, bdr.MBR_SIGNATURE)
        self.assertEqual(gpt["pmbr"][450], 0xEE)

        # Primary GPT Header signature & CRC32
        self.assertEqual(gpt["primary_hdr"][0:8], bdr.GPT_SIGNATURE)
        prim_crc = struct.unpack_from("<I", gpt["primary_hdr"], 16)[0]
        hdr_copy = bytearray(gpt["primary_hdr"][:bdr.GPT_HEADER_SIZE])
        struct.pack_into("<I", hdr_copy, 16, 0)
        self.assertEqual(zlib.crc32(hdr_copy) & 0xFFFFFFFF, prim_crc)


class TestInitDisplayHandshakeAndUniversality(unittest.TestCase):
    """Verifies that mini_init binary and system init staging implement the universal display handshake."""

    def test_mini_init_arm32_elf_and_display_strings(self):
        mini_init_path = REPO_ROOT / "staging" / "mini_init"
        self.assertTrue(mini_init_path.is_file(), f"staging/mini_init missing at {mini_init_path}")

        data = mini_init_path.read_bytes()
        # Verify ARM32 ELF header (Magic: 7f 45 4c 46, Class: 1, Data: 1, Machine: 40 / 0x28 ARM)
        self.assertEqual(data[:4], b"\x7fELF", "Invalid ELF magic for mini_init")
        self.assertEqual(data[4], 1, "mini_init must be 32-bit ELF (ELFCLASS32)")
        self.assertEqual(data[5], 1, "mini_init must be Little Endian (ELFDATA2LSB)")
        machine = struct.unpack_from("<H", data, 18)[0]
        self.assertEqual(machine, 40, "mini_init must target ARM architecture (EM_ARM = 40)")

        # Verify universal display handshake, bootanimation suppression, and software keymaster/gatekeeper markers
        self.assertIn(b"/dev/fb0", data)
        self.assertIn(b"TCL SMART TV FULL-SYSTEM FIRMWARE EMULATION", data)
        self.assertIn(b"bootanimation", data)
        self.assertIn(b"/system/bin/surfaceflinger", data)
        self.assertIn(b"keystore.rtd284x.so", data)
        self.assertIn(b"gatekeeper.rtd284x.so", data)
        self.assertIn(b"/dev/socket/lmkd", data)
        self.assertIn(b"/system/bin/lmkd", data)
        self.assertIn(b"/fstab.rtd284x", data)
        self.assertIn(b"init.svc.bootanim", data)

    def test_system_img_init_synchronization(self):
        system_img = REPO_ROOT / "staging" / "system.img"
        mini_init = REPO_ROOT / "staging" / "mini_init"
        self.assertTrue(system_img.is_file(), f"staging/system.img missing at {system_img}")
        self.assertTrue(mini_init.is_file(), f"staging/mini_init missing at {mini_init}")

        # Verify /init in system.img via debugfs
        cmd = ["/opt/homebrew/opt/e2fsprogs/sbin/debugfs", "-R", "stat init", str(system_img)]
        proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
        self.assertIn("Mode:  0755", proc.stdout)
        self.assertIn(f"Size: {mini_init.stat().st_size}", proc.stdout)
        self.assertIn('security.selinux (23) = "u:object_r:init_exec:s0"', proc.stdout)



class TestFullPipelineIntegration(unittest.TestCase):
    """Executes the full builder on firmware staging and validates all output artifacts."""

    @classmethod
    def setUpClass(cls):
        # These tests rebuild every partition twice. Keep outputs and
        # intermediate images separate from any running developer machine.
        tmp = tempfile.TemporaryDirectory(prefix="rt41-rom-test-")
        cls.addClassCleanup(tmp.cleanup)
        root = Path(tmp.name)
        staging = root / "staging"
        staging.mkdir()
        for name in ("system_extracted", "update_root"):
            source = REPO_ROOT / "staging" / name
            if not source.is_dir():
                raise unittest.SkipTest(f"Private firmware input unavailable: {source}")
            (staging / name).symlink_to(source.resolve(), target_is_directory=True)
        (root / "build").mkdir()
        (root / "build/swiftshader-android").symlink_to(
            REPO_ROOT / "build/swiftshader-android", target_is_directory=True)
        cls.workspace_root = str(root)
        cls.output_dir = str(root / "output")
        cls.emmc_path = os.path.join(cls.output_dir, "emmc.raw")
        cls.update_img_path = os.path.join(cls.output_dir, "Update_RT41_Debloated.img")

    def test_01_build_and_verify(self):
        print("\n[TEST] Running full build_debloated_rom pipeline...")
        bdr.build_debloated_rom(
            workspace_root=self.workspace_root,
            output_dir=self.output_dir
        )

        # Assert outputs exist
        self.assertTrue(os.path.isfile(self.emmc_path), f"emmc.raw not created at {self.emmc_path}")
        self.assertTrue(os.path.isfile(self.update_img_path), f"Update_RT41_Debloated.img not created at {self.update_img_path}")
        self.assertTrue(os.path.isfile(f"{self.update_img_path}.sha256"))

        # Assert sizes
        self.assertEqual(os.path.getsize(self.emmc_path), 4096 * 1024 * 1024)
        self.assertGreater(os.path.getsize(self.update_img_path), 1_500_000_000)

        # Run deep verification
        bdr.verify_debloated_artifacts(
            emmc_path=self.emmc_path,
            update_img_path=self.update_img_path,
            workspace_root=self.workspace_root
        )

    def test_02_idempotency_second_run(self):
        print("\n[TEST] Verifying idempotency with secondary build execution...")
        # A second run must succeed cleanly with zero exceptions
        bdr.build_debloated_rom(
            workspace_root=self.workspace_root,
            output_dir=self.output_dir
        )
        self.assertTrue(os.path.isfile(self.emmc_path))
        self.assertTrue(os.path.isfile(self.update_img_path))


if __name__ == "__main__":
    unittest.main()
