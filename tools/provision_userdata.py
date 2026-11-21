#!/usr/bin/env python3
"""
tools/provision_userdata.py - Android 9 Pie Userdata Pre-seeding & Provisioning Engine
Target: Realtek RTD2841 / TCL RT41 tv030 Android 9 Pie (API 28, "UnionTV")

Doctrine of Zero-Assumption Engineering:
- Inspects all return codes and outputs from mke2fs, debugfs, and e2fsck.
- Generates pre-seeded /data/system/users/0/ configuration files:
  1. settings_global.xml: device_provisioned=1, adb_enabled=1, setup_wizard_has_run=1,
     disables all Google/TCL data collection, upload, and verifier telemetry.
  2. settings_secure.xml: user_setup_complete=1, tv_user_setup_complete=1,
     install_non_market_apps=1, all TCL user agreement consent and telemetry flags disabled.
  3. settings_system.xml: persist and app-level telemetry report disable flags set.
  4. package-restrictions.xml: registers ProjectivyLauncher (MainActivity) as default
     preferred HOME activity intent filter, bypassing all setup wizard disambiguation.
  5. preferred-apps.xml: complementary intent resolution mapping for Package Manager.
- Formats ext4 userdata.img (510 MiB) with volume label 'userdata', 4KB block size,
  256-byte inode size, and injects provisioned XML hierarchy with system:system (1000:1000)
  ownership, mode 0600 (files) / 0700 (user 0 directory), and SELinux system_data_file:s0 context.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

AID_SYSTEM = 1000
AID_SHELL = 2000

# Canonical SELinux contexts
SELINUX_SYSTEM_DATA = "u:object_r:system_data_file:s0"
SELINUX_SHELL_DATA = "u:object_r:shell_data_file:s0"
SELINUX_BACKUP_DATA = "u:object_r:backup_data_file:s0"


def find_tool(name: str) -> str:
    """Locates required executable with search fallback."""
    search_paths = [
        "/opt/homebrew/bin",
        "/opt/homebrew/opt/e2fsprogs/bin",
        "/opt/homebrew/opt/e2fsprogs/sbin",
        "/opt/homebrew/Cellar/e2fsprogs/1.47.4/sbin",
        "/usr/local/bin",
        "/usr/bin",
        "/bin",
        "/usr/sbin",
        "/sbin",
    ]
    cand = shutil.which(name)
    if cand and os.access(cand, os.X_OK):
        return cand
    for sp in search_paths:
        p = os.path.join(sp, name)
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return p
    raise FileNotFoundError(f"Required binary '{name}' not found in PATH or standard directories.")


def run_cmd(cmd, cwd=None, check=True):
    """Executes a command enforcing checked return codes."""
    res = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if check and res.returncode != 0:
        cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
        raise subprocess.CalledProcessError(
            res.returncode,
            cmd_str,
            output=res.stdout,
            stderr=res.stderr
        )
    return res


def get_settings_global_xml() -> str:
    """
    Constructs /data/system/users/0/settings_global.xml.
    Pre-provisions the system, enables ADB, and disables all telemetry and tracking.
    """
    return """<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<settings version="170">
  <setting id="1" name="device_provisioned" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="2" name="setup_wizard_has_run" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="3" name="wifi_on" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="4" name="adb_enabled" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="5" name="auto_time" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="6" name="auto_time_zone" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="7" name="package_verifier_enable" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="8" name="package_verifier_user_consent" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="9" name="upload_apk_enable" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="10" name="send_action_app_error" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="11" name="allow_user_data_collection" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="12" name="tcl_data_collection_enabled" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="13" name="tcl_user_agreement_agreed" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="14" name="user_experience_program" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="15" name="user_experience_involved" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="16" name="personalized_ads" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="17" name="advertising_id_opt_out" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="18" name="usage_stats_enabled" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="19" name="netstats_enabled" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="20" name="hdmi_control_enabled" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="21" name="heads_up_notifications_enabled" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="22" name="development_settings_enabled" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="23" name="device_name" value="UnionTV" package="android" defaultValue="UnionTV" defaultSysSet="true" />
  <setting id="24" name="bluetooth_on" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="25" name="bluetooth_disabled" value="1" package="android" defaultValue="1" defaultSysSet="true" />
</settings>
"""


def get_settings_secure_xml() -> str:
    """
    Constructs /data/system/users/0/settings_secure.xml.
    Marks user setup complete and disables all telemetry and consent checkboxes.
    """
    return """<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<settings version="170">
  <setting id="1" name="user_setup_complete" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="2" name="tv_user_setup_complete" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="3" name="install_non_market_apps" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="4" name="lockscreen.disabled" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="5" name="screensaver_enabled" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="6" name="wake_gesture_enabled" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="7" name="default_input_method" value="com.tcl.inputmethod.international/.T_IME" package="android" defaultValue="com.tcl.inputmethod.international/.T_IME" defaultSysSet="true" />
  <setting id="8" name="user_agreement_agreed" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="9" name="user_agreement_accepted" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="10" name="tcl_user_agreement" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="11" name="cbSmartTV" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="12" name="cbTVUpdate" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="13" name="cbDelete" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="14" name="advertisementservices" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="15" name="recognitionservice" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="16" name="freeviewplay" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="17" name="personal_data_collection" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="18" name="tcl_data_report_disable" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="19" name="data_report_disable" value="1" package="android" defaultValue="1" defaultSysSet="true" />
</settings>
"""


def get_settings_system_xml() -> str:
    """
    Constructs /data/system/users/0/settings_system.xml.
    Disables system-level telemetry and analytics collection.
    """
    return """<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<settings version="170">
  <setting id="1" name="tcl_data_report_disable" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="2" name="tcl_data_collection" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="3" name="persisit.app.setting.data.report.disable" value="true" package="android" defaultValue="true" defaultSysSet="true" />
  <setting id="4" name="persist.app.setting.data.report.disable" value="true" package="android" defaultValue="true" defaultSysSet="true" />
</settings>
"""


def get_package_restrictions_xml() -> str:
    """
    Constructs /data/system/users/0/package-restrictions.xml.
    Pre-registers ProjectivyLauncher as the default preferred HOME activity intent handler.
    """
    return """<?xml version='1.0' encoding='utf-8' standalone='yes' ?>
<package-restrictions>
  <preferred-activities>
    <item name="com.spocky.projengmenu/com.spocky.projengmenu.ui.home.MainActivity" match="100000" always="true" set="1">
      <set name="com.spocky.projengmenu/com.spocky.projengmenu.ui.home.MainActivity" />
      <filter>
        <action name="android.intent.action.MAIN" />
        <category name="android.intent.category.HOME" />
        <category name="android.intent.category.DEFAULT" />
      </filter>
    </item>
  </preferred-activities>
</package-restrictions>
"""


def get_preferred_apps_xml() -> str:
    """
    Constructs /data/system/users/0/preferred-apps.xml as complementary intent resolution mapping.
    """
    return """<?xml version='1.0' encoding='utf-8' standalone='yes' ?>
<preferred-activities>
  <item name="com.spocky.projengmenu/com.spocky.projengmenu.ui.home.MainActivity" match="100000" always="true" set="1">
    <set name="com.spocky.projengmenu/com.spocky.projengmenu.ui.home.MainActivity" />
    <filter>
      <action name="android.intent.action.MAIN" />
      <category name="android.intent.category.HOME" />
      <category name="android.intent.category.DEFAULT" />
    </filter>
  </item>
</preferred-activities>
"""


def generate_provisioned_xmls(output_dir: str) -> dict:
    """
    Generates all Android 9 Pie SettingsProvider and PackageManager pre-seeded XMLs.
    Returns mapping of filename -> absolute path.
    """
    os.makedirs(output_dir, exist_ok=True)
    xml_map = {
        "settings_global.xml": get_settings_global_xml(),
        "settings_secure.xml": get_settings_secure_xml(),
        "settings_system.xml": get_settings_system_xml(),
        "package-restrictions.xml": get_package_restrictions_xml(),
        "preferred-apps.xml": get_preferred_apps_xml(),
    }

    created = {}
    for fname, content in xml_map.items():
        fpath = os.path.join(output_dir, fname)
        with open(fpath, "w", encoding="utf-8") as f:
            f.write(content)
        os.chmod(fpath, 0o600)
        created[fname] = fpath

    print(f"[+] Generated {len(created)} provisioned XMLs in {output_dir}")
    return created


def build_provisioned_userdata_image(workspace_root: str, output_img: str = None, xml_dir: str = None) -> tuple:
    """
    Builds ext4 userdata.img (510 MiB) pre-seeded with:
      /data/system/users/0/settings_global.xml
      /data/system/users/0/settings_secure.xml
      /data/system/users/0/settings_system.xml
      /data/system/users/0/package-restrictions.xml
      /data/system/users/0/preferred-apps.xml
    Enforces strict Android UID/GID ownership and SELinux contexts.
    """
    staging_dir = os.path.join(workspace_root, "staging")
    userdata_img = output_img or os.path.join(staging_dir, "userdata.img")

    mke2fs_bin = find_tool("mke2fs")
    e2fsck_bin = find_tool("e2fsck")
    debugfs_bin = find_tool("debugfs")

    size_mb = 510  # 510 MiB, LBA 7342080..8386559
    size_bytes = size_mb * 1024 * 1024

    print("\n========================================================")
    print(f"[*] Stage: Building Pre-seeded ext4 userdata.img ({size_mb} MiB)")
    print("========================================================")

    with tempfile.TemporaryDirectory() as td:
        xml_source_dir = xml_dir or td
        xml_map = generate_provisioned_xmls(xml_source_dir)

        if os.path.dirname(userdata_img):
            os.makedirs(os.path.dirname(userdata_img), exist_ok=True)
        with open(userdata_img, "wb") as f:
            f.truncate(size_bytes)

        cmd = [
            mke2fs_bin,
            "-t", "ext4",
            "-F",
            "-O", "^metadata_csum,^orphan_file,^dir_index",
            "-L", "userdata",
            "-b", "4096",
            "-I", "256",
            userdata_img
        ]
        run_cmd(cmd)

        cmds = [
            "mkdir local",
            "mkdir local/tmp",
            "set_inode_field local/tmp mode 040777",
            f"ea_set local/tmp security.selinux {SELINUX_SHELL_DATA}",
            # Pre-seed /data/backup and /data/system/backup to prevent BackupManagerService NPE
            "mkdir backup",
            f"set_inode_field backup uid {AID_SYSTEM}",
            f"set_inode_field backup gid {AID_SYSTEM}",
            "set_inode_field backup mode 040700",
            f"ea_set backup security.selinux {SELINUX_BACKUP_DATA}",
            "mkdir system",
            f"set_inode_field system uid {AID_SYSTEM}",
            f"set_inode_field system gid {AID_SYSTEM}",
            "set_inode_field system mode 040775",
            f"ea_set system security.selinux {SELINUX_SYSTEM_DATA}",
            "mkdir system/backup",
            f"set_inode_field system/backup uid {AID_SYSTEM}",
            f"set_inode_field system/backup gid {AID_SYSTEM}",
            "set_inode_field system/backup mode 040700",
            f"ea_set system/backup security.selinux {SELINUX_BACKUP_DATA}",
            "mkdir system/users",
            f"set_inode_field system/users uid {AID_SYSTEM}",
            f"set_inode_field system/users gid {AID_SYSTEM}",
            "set_inode_field system/users mode 040775",
            f"ea_set system/users security.selinux {SELINUX_SYSTEM_DATA}",
            "mkdir system/users/0",
            f"set_inode_field system/users/0 uid {AID_SYSTEM}",
            f"set_inode_field system/users/0 gid {AID_SYSTEM}",
            "set_inode_field system/users/0 mode 040700",
            f"ea_set system/users/0 security.selinux {SELINUX_SYSTEM_DATA}",
        ]

        # Use relative basenames with cwd execution to prevent debugfs whitespace splitting bugs
        for fname in xml_map.keys():
            remote_path = f"system/users/0/{fname}"
            cmds.append(f"write {fname} {remote_path}")
            cmds.append(f"set_inode_field {remote_path} uid {AID_SYSTEM}")
            cmds.append(f"set_inode_field {remote_path} gid {AID_SYSTEM}")
            cmds.append(f"set_inode_field {remote_path} mode 0100600")
            cmds.append(f"ea_set {remote_path} security.selinux {SELINUX_SYSTEM_DATA}")

        # Ensure /data/misc/adb/adb_keys is present for instant host adb authentication
        adb_key_paths = [
            os.path.expanduser("~/.android/adbkey.pub"),
            os.path.join(workspace_root, "build/phase3/adbd-probe-20261026T143739Z/test_adb_key.pub")
        ]
        combined_keys = []
        for kp in adb_key_paths:
            if os.path.isfile(kp):
                with open(kp, "r", encoding="utf-8") as kf:
                    content = kf.read().strip()
                    if content and content not in combined_keys:
                        combined_keys.append(content)
        if combined_keys:
            adb_keys_file = os.path.join(xml_source_dir, "adb_keys")
            with open(adb_keys_file, "w", encoding="utf-8") as kf:
                kf.write("\n".join(combined_keys) + "\n")
            cmds.extend([
                "mkdir misc",
                f"set_inode_field misc uid {AID_SYSTEM}",
                f"set_inode_field misc gid {AID_SYSTEM}",
                "set_inode_field misc mode 040775",
                f"ea_set misc security.selinux {SELINUX_SYSTEM_DATA}",
                "mkdir misc/adb",
                f"set_inode_field misc/adb uid {AID_SYSTEM}",
                f"set_inode_field misc/adb gid {AID_SYSTEM}",
                "set_inode_field misc/adb mode 040700",
                f"ea_set misc/adb security.selinux {SELINUX_SYSTEM_DATA}",
                "write adb_keys misc/adb/adb_keys",
                f"set_inode_field misc/adb/adb_keys uid {AID_SYSTEM}",
                f"set_inode_field misc/adb/adb_keys gid {AID_SYSTEM}",
                "set_inode_field misc/adb/adb_keys mode 0100640",
                f"ea_set misc/adb/adb_keys security.selinux {SELINUX_SYSTEM_DATA}",
            ])

        cmd_file = os.path.join(staging_dir, "userdata_debugfs_cmds.tmp")
        with open(cmd_file, "w", encoding="utf-8") as f:
            f.write("\n".join(cmds) + "\n")

        run_cmd([debugfs_bin, "-w", "-f", cmd_file, userdata_img], cwd=xml_source_dir)
        if os.path.exists(cmd_file):
            os.remove(cmd_file)

    print("[*] Verifying userdata.img with e2fsck...")
    res = run_cmd([e2fsck_bin, "-f", "-n", userdata_img], check=False)
    if res.returncode > 1:
        raise RuntimeError(f"e2fsck failed on userdata.img (code {res.returncode}):\n{res.stdout}")

    res_stat = run_cmd([debugfs_bin, "-R", "stat system/users/0/settings_global.xml", userdata_img])
    assert f"User:  {AID_SYSTEM}" in res_stat.stdout and f"Group:  {AID_SYSTEM}" in res_stat.stdout, "XML ownership must be 1000:1000!"
    assert "security.selinux" in res_stat.stdout, "SELinux context missing on provisioned XML!"

    res_backup = run_cmd([debugfs_bin, "-R", "stat backup", userdata_img])
    assert f"User:  {AID_SYSTEM}" in res_backup.stdout and "0700" in res_backup.stdout, "backup directory must be 0700 1000:1000"

    res_sys_backup = run_cmd([debugfs_bin, "-R", "stat system/backup", userdata_img])
    assert f"User:  {AID_SYSTEM}" in res_sys_backup.stdout and "0700" in res_sys_backup.stdout, "system/backup directory must be 0700 1000:1000"

    res_pref = run_cmd([debugfs_bin, "-R", "cat system/users/0/package-restrictions.xml", userdata_img])
    assert "com.spocky.projengmenu" in res_pref.stdout, "ProjectivyLauncher must be registered in package-restrictions.xml!"

    print(f"[+] Verified provisioned XML injection into userdata.img ({size_bytes} bytes).")
    return userdata_img, size_bytes


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    parser = argparse.ArgumentParser(description="Android 9 Userdata Pre-seeding & Provisioning Tool")
    parser.add_argument("--workspace-root", default=repo_root, help="Repository workspace root")
    parser.add_argument("--output-img", default=None, help="Output userdata.img path")
    parser.add_argument("--xml-dir", default=None, help="Output directory for provisioned XMLs")
    parser.add_argument("--verify", action="store_true", help="Perform verification on generated image")
    args = parser.parse_args()

    workspace_root = os.path.abspath(args.workspace_root)
    output_img = os.path.abspath(args.output_img) if args.output_img else None
    xml_dir = os.path.abspath(args.xml_dir) if args.xml_dir else None

    if xml_dir and not output_img:
        generate_provisioned_xmls(xml_dir)
        return 0

    build_provisioned_userdata_image(workspace_root=workspace_root, output_img=output_img, xml_dir=xml_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
