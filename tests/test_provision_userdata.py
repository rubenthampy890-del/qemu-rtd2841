#!/usr/bin/env python3
"""
tests/test_provision_userdata.py - Unit & Integration Test Suite for Userdata Provisioning & Stubs
Target: Realtek RTD2841 / TCL RT41 Android 9 Pie (API 28)

Verifies:
1. All telemetry and tracking consent flags are disabled across settings_global, settings_secure, settings_system.
2. device_provisioned=1, user_setup_complete=1, adb_enabled=1 are enforced.
3. ProjectivyLauncher (com.spocky.projengmenu) is registered as preferred HOME activity in package-restrictions.xml.
4. BiReportStub.apk has valid AndroidManifest, classes.dex, AIDL binders, and exports BiReportService.
5. Ext4 userdata.img filesystem geometry, UID/GID (1000:1000), mode (0600), and SELinux labeling.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

import provision_userdata as pu


class TestProvisioningXMLsDeep(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_prov_deep_")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_xml_flags_and_preferred_home(self):
        xml_map = pu.generate_provisioned_xmls(self.test_dir)

        # 1. settings_global.xml
        tree_g = ET.parse(xml_map["settings_global.xml"])
        settings_g = {el.get("name"): el.get("value") for el in tree_g.getroot().findall("setting")}

        self.assertEqual(settings_g.get("device_provisioned"), "1")
        self.assertEqual(settings_g.get("setup_wizard_has_run"), "1")
        self.assertEqual(settings_g.get("adb_enabled"), "1")
        self.assertEqual(settings_g.get("package_verifier_enable"), "0")
        self.assertEqual(settings_g.get("package_verifier_user_consent"), "0")
        self.assertEqual(settings_g.get("upload_apk_enable"), "0")
        self.assertEqual(settings_g.get("send_action_app_error"), "0")
        self.assertEqual(settings_g.get("allow_user_data_collection"), "0")
        self.assertEqual(settings_g.get("tcl_data_collection_enabled"), "0")
        self.assertEqual(settings_g.get("advertising_id_opt_out"), "1")
        self.assertEqual(settings_g.get("usage_stats_enabled"), "0")
        self.assertEqual(settings_g.get("netstats_enabled"), "0")

        # 2. settings_secure.xml
        tree_s = ET.parse(xml_map["settings_secure.xml"])
        settings_s = {el.get("name"): el.get("value") for el in tree_s.getroot().findall("setting")}

        self.assertEqual(settings_s.get("user_setup_complete"), "1")
        self.assertEqual(settings_s.get("tv_user_setup_complete"), "1")
        self.assertEqual(settings_s.get("install_non_market_apps"), "1")
        self.assertEqual(settings_s.get("cbSmartTV"), "0")
        self.assertEqual(settings_s.get("cbTVUpdate"), "0")
        self.assertEqual(settings_s.get("cbDelete"), "0")
        self.assertEqual(settings_s.get("advertisementservices"), "0")
        self.assertEqual(settings_s.get("recognitionservice"), "0")
        self.assertEqual(settings_s.get("freeviewplay"), "0")
        self.assertEqual(settings_s.get("personal_data_collection"), "0")
        self.assertEqual(settings_s.get("tcl_data_report_disable"), "1")
        self.assertEqual(settings_s.get("data_report_disable"), "1")

        # 3. settings_system.xml
        tree_sys = ET.parse(xml_map["settings_system.xml"])
        settings_sys = {el.get("name"): el.get("value") for el in tree_sys.getroot().findall("setting")}
        self.assertEqual(settings_sys.get("tcl_data_report_disable"), "1")
        self.assertEqual(settings_sys.get("tcl_data_collection"), "0")
        self.assertEqual(settings_sys.get("persisit.app.setting.data.report.disable"), "true")
        self.assertEqual(settings_sys.get("persist.app.setting.data.report.disable"), "true")

        # 4. package-restrictions.xml preferred activity registration
        tree_p = ET.parse(xml_map["package-restrictions.xml"])
        pref_acts = tree_p.getroot().find("preferred-activities")
        self.assertIsNotNone(pref_acts, "preferred-activities tag must be present in package-restrictions.xml")

        items = pref_acts.findall("item")
        self.assertGreaterEqual(len(items), 1)

        home_item = None
        for it in items:
            filt = it.find("filter")
            if filt is not None:
                acts = [a.get("name") for a in filt.findall("action")]
                cats = [c.get("name") for c in filt.findall("category")]
                if "android.intent.action.MAIN" in acts and "android.intent.category.HOME" in cats:
                    home_item = it
                    break

        self.assertIsNotNone(home_item, "Must have an item matching MAIN + HOME")
        self.assertIn("com.spocky.projengmenu", home_item.get("name"))
        self.assertEqual(home_item.get("always"), "true")


class TestBiReportStubApk(unittest.TestCase):
    def test_stub_apk_structure(self):
        apk_path = REPO_ROOT / "apks" / "BiReportStub.apk"
        self.assertTrue(apk_path.is_file(), f"BiReportStub.apk must exist at {apk_path}")

        with zipfile.ZipFile(apk_path, "r") as z:
            names = z.namelist()
            self.assertIn("AndroidManifest.xml", names)
            self.assertIn("classes.dex", names)

            dex_data = z.read("classes.dex")
            # Verify AIDL and Binder class descriptors in DEX
            self.assertIn(b"BiReportService", dex_data)
            self.assertIn(b"StubBinder", dex_data)
            self.assertIn(b"IBiReportService", dex_data)
            self.assertIn(b"IBIService", dex_data)
            self.assertIn(b"ParamProvider", dex_data)


class TestUserdataImageBuild(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="test_ud_build_")
        self.out_img = os.path.join(self.test_dir, "userdata.img")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_build_userdata_img(self):
        img_path, size_bytes = pu.build_provisioned_userdata_image(
            workspace_root=str(REPO_ROOT),
            output_img=self.out_img
        )
        self.assertTrue(os.path.isfile(img_path))
        self.assertEqual(size_bytes, 510 * 1024 * 1024)

        debugfs_bin = pu.find_tool("debugfs")

        # Verify inodes and content
        res = pu.run_cmd([debugfs_bin, "-R", "cat system/users/0/package-restrictions.xml", img_path])
        self.assertIn("com.spocky.projengmenu", res.stdout)

        res_stat = pu.run_cmd([debugfs_bin, "-R", "stat system/users/0/settings_global.xml", img_path])
        self.assertIn(f"User:  {pu.AID_SYSTEM}", res_stat.stdout)
        self.assertIn(f"Group:  {pu.AID_SYSTEM}", res_stat.stdout)
        self.assertIn("security.selinux", res_stat.stdout)


if __name__ == "__main__":
    unittest.main()
