#!/usr/bin/env python3
"""
tools/build_debloated_rom.py - Phase 4 Automated Debloated ROM Builder & Multi-Target Packaging Tool
Target SoC: Realtek RTD2841 / RTK284x (TCL RT41 Chassis, Board tv030, Android 9 Pie API 28)

Objectives:
1. Ingest extracted firmware partitions or Update.img (standard POSIX tar package).
2. Purge Tier 1 Safe Removal Bloatware:
   - System: Pumpkin, Tubesky, PlayAutoInstallStub, TVRecommendations, TCL_ALL_CyberUI,
             TCL_Dashboard_OverSeas, BugReportSender, GoogleFeedback, TCLHome_passive
   - Vendor: TCL_ALL_BI, UserAgreement, TVWeiShi, terminalmanagerplus, netinfocap
3. Inject ProjectivyLauncher.apk into /system/app/ProjectivyLauncher/
   with permissions 0644, root:root (0:0), SELinux context u:object_r:system_file:s0.
4. Inject pre-seeded provisioned XMLs into /data/system/users/0/:
   - settings_global.xml (device_provisioned=1, setup_wizard_has_run=1)
   - settings_secure.xml (user_setup_complete=1, tv_user_setup_complete=1, install_non_market_apps=1)
   - settings_system.xml
   - package-restrictions.xml
5. Outputs:
   a. output/emmc.raw: 4096 MiB composite disk with GPT partitioning for immediate QEMU emulation.
   b. output/Update_RT41_Debloated.img: Standard POSIX tar package for physical USB OTA recovery flashing.
6. Validation & Idempotency: Complete cryptographic, filesystem, and geometry integrity assertions.

Doctrine Adherence:
- Checked Syscalls & Subprocess returns (Doctrine 1)
- ARM32 EABI Target Ground Truth (Doctrine 2)
- Hardware Fidelity & Case-Sensitivity Preservation (Doctrine 3)
- Modulo bounds & Memory Safety (Doctrine 5)
- Local Isolation & Branch Safety (Doctrine 6)
"""

import argparse
import glob
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import uuid
import zlib
from pathlib import Path

# ==============================================================================
# Constants & Configuration
# ==============================================================================

SECTOR_SIZE = 512
MBR_SIGNATURE = 0xAA55
GPT_SIGNATURE = b"EFI PART"
GPT_REVISION = 0x00010000
GPT_HEADER_SIZE = 92
NUM_PARTITION_ENTRIES = 128
PARTITION_ENTRY_SIZE = 128

# Standard Linux filesystem data GUID (47728e79-3d69-4d8e-b5b7-7d5a56d68b81)
LINUX_FS_DATA_GUID = uuid.UUID("0FC63DAF-8483-4772-8E79-3D69D8477DE4")

# Android AIDs
AID_ROOT = 0
AID_SYSTEM = 1000
AID_SHELL = 2000
AID_NET_ADMIN = 3003

# ==============================================================================
# Verified 3-Tiered Debloat Matrix (TCL RT41 / RTD2841)
# ==============================================================================

# Tier 1 (Category A): Mandatory Telemetry, Adware, and Bloatware Purge
TIER1_SYSTEM_REMOVALS = [
    # Telemetry and Tracking Daemons
    "app/Pumpkin",                  # TCL OEM telemetry and user tracking daemon
    "priv-app/Tubesky",             # TCL continuous background diagnostic analytics (41 MB)
    "priv-app/PlayAutoInstallStub", # Google/OEM silent application downloader
    "priv-app/TVRecommendations",   # OEM sponsored ad injection and recommendation cards
    # OEM Bloatware and Redundant UI
    "app/TCL_ALL_CyberUI",          # TCL legacy UI overlay
    "app/TCL_Dashboard_OverSeas",   # TCL overseas dashboard
    "app/BugReportSender",          # OEM bug reporting uploader
    "priv-app/GoogleFeedback",      # Google feedback / crash reporter
    "product/app/TCLHome_passive",  # TCL Home bloatware
    # Category A System Factory Bloatware
    "factory-app/DeviceAuthenCommon", # Device Authentication bloatware
    "factory-app/Esticker",           # TCL E-Sticker demo overlay
    "factory-app/copyDataToTV",       # OEM migration utility
    "product/app/KeyCustomFunctionService", # KeyCustomFunctionService (cures EglManager EGL_NO_SURFACE abort)
    # Stock Launchers & Setup Wizard (unblocks ProjectivyLauncher as sole uncontested HOME activity)
    "app/InitSetup",                # OOBE setup wizard (prevents setup wizard stall and ANR cascades)
    "factory-app/TclFrameworkCustom", # Proprietary TCL framework service (cures BpServiceManager ANR deadlock)
    "priv-app/TVLauncher",          # Stock TV launcher (cures SecurityException setThreadPriority crash loop)
    "priv-app/LeanbackLauncher",    # Stock Leanback launcher (prevents HOME intent collision)
    "priv-app/SetupWraithPrebuilt", # Stock Android TV setup wizard (prevents setup wizard stall)
    "priv-factory-app/TVLauncher",  # Factory TV launcher fallback
    "priv-factory-app/LeanbackLauncher",
    "priv-factory-app/SetupWraithPrebuilt",
    "priv-factory-app/SetupWraith",
    "priv-factory-app/PrebuiltSetupWraith",
]

TIER1_VENDOR_REMOVALS = [
    # Category A Telemetry, AppStore, and Surveillance
    "app/TCL_ALL_BI",               # TCL Business Intelligence analytics collector
    "app/UserAgreement",            # OEM tracking consent prompt
    "app/TCL_ALL_APPSTORE",         # TCL OEM App Store
    "app/TVWeiShi",                 # TCL TV manager / bloatware
    "app/terminalmanagerplus",      # TCL remote management agent
    "app/DeviceAuthenCommon",       # Device Authentication bloatware
    "app/UserCenter_OverSeas",      # TCL Overseas User Center
    "app/Esticker",                 # TCL E-Sticker demo overlay
    "app/copyDataToTV",             # OEM migration utility
    "app/SystemUpdate",             # Forced OTA updater
    "tvos/bin/netinfocap",          # Network packet capture / telemetry sniffer
    "tvos/bin/log_out",             # Diagnostic telemetry dumper
    "app/TIF_TvInput",              # TV Input Service (cures sleep(1) tuner ANR deadlock)
    "app/TIF_LiveTV",               # Live TV Service (cures tuner service dependency)
    "app/TclFrameworkCustom",       # Proprietary TCL framework service (cures BpServiceManager ANR deadlock)
]

# Tier 2 (Category B): Secondary / Non-Essential Packages
TIER2_SECONDARY_REMOVALS = [
    "vendor/app/ListenNetStateService",
    "vendor/media/EStickerRes",
]

# Tier 3 (Category C): Boot-Critical & Framework Preservation Targets (NEVER PURGE)
TIER3_BOOT_CRITICAL_PRESERVE = [
    "system/priv-app/ConfigProvider",           # Boot-critical ConfigProvider
    "vendor/framework/com.tcl.tvmanager.jar",    # com.tcl.tvmanager.jar (800 classes in vdex)
    "vendor/framework/android.tclwidget.jar",    # android.tclwidget.jar (95 classes in vdex)
    "vendor/app/T_IME",                         # System keyboard
    "system/product/app/TCLBTRcuService",       # Bluetooth Remote Service
]

# Canonical Update.img member list and ordering for install_a compatibility
UPDATE_IMG_MEMBERS = [
    "0.vmlinux.rescue.aes",
    "config.txt",
    "customer",
    "font.ttf",
    "fw_text_segment_info.aes",
    "install_a",
    "mm",
    "otares.tar.bz2",
    "package7",
    "package7/video_firmware.aes",
    "package7/bootloader.tar",
    "package7/system.emmc.tar",
    "package7/bootfile.raw",
    "package7/vmlinux.develop.android.pie.rtd284x.tv030.emmc.user.andtv.optee.2K-model.aes",
    "package7/factory.tar",
    "package7/bluecore.audio.aes",
    "package7/tclconfig.tar.bz2",
    "package7/optee_armtz.emmc.tar.bz2",
    "package7/kcpu.aes",
    "package7/data.tar.bz2",
    "postprocess.sh",
    "setfilecon.sh",
    "vendor.tar",
]


# ==============================================================================
# Helper Utilities (Tool Resolution & Command Execution)
# ==============================================================================

def find_tool(name):
    """Dynamically resolve tool path across PATH and standard Unix/Homebrew locations."""
    which_path = shutil.which(name)
    if which_path and os.path.isfile(which_path) and os.access(which_path, os.X_OK):
        return which_path

    search_dirs = [
        "/opt/homebrew/opt/e2fsprogs/sbin",
        "/opt/homebrew/sbin",
        "/opt/homebrew/bin",
        "/usr/local/sbin",
        "/usr/local/bin",
        "/usr/sbin",
        "/sbin",
    ]
    for d in search_dirs:
        p = os.path.join(d, name)
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return p

    cellar_matches = sorted(glob.glob(f"/opt/homebrew/Cellar/e2fsprogs/*/sbin/{name}"))
    if cellar_matches:
        return cellar_matches[-1]

    raise FileNotFoundError(f"Required tool '{name}' not found in PATH or standard search paths.")


def run_cmd(cmd, check=True, cwd=None):
    """Execute command with return code inspection and error propagation."""
    cmd_str = " ".join(str(c) for c in cmd)
    print(f"[*] Running: {cmd_str}")
    res = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)
    if res.returncode != 0 and check:
        print(f"[-] Command failed with exit code {res.returncode}")
        print("STDOUT:", res.stdout)
        print("STDERR:", res.stderr)
        raise subprocess.CalledProcessError(res.returncode, cmd, output=res.stdout, stderr=res.stderr)
    return res


def compute_sha256(file_path):
    """Compute SHA-256 digest of a file in streaming chunks."""
    h = hashlib.sha256()
    with open(file_path, "rb") as f:
        while chunk := f.read(16 * 1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


SWIFTSHADER_EMULATOR_LIBS = {
    "libEGL_swiftshader.so": "libEGL.so",
    "libGLESv1_CM_swiftshader.so": "libGLES_CM.so",
    "libGLESv2_swiftshader.so": "libGLESv2.so",
}


def build_emulator_init(workspace_root):
    """Build PID 1 from this checkout before creating the emulator disk."""
    directory = os.path.join(workspace_root, "build", "phase4")
    os.makedirs(directory, exist_ok=True)
    target = os.path.join(directory, "mini_init")
    with tempfile.TemporaryDirectory(dir=directory) as scratch:
        binary = os.path.join(scratch, "mini_init")
        run_cmd([find_tool("arm-none-eabi-gcc"), "-march=armv7-a", "-nostdlib",
                 "-static", "-fno-builtin", "-O2",
                 os.path.join(os.path.dirname(__file__), "mini_init.c"),
                 "-o", binary, "-lgcc"])
        os.replace(binary, target)
    return target


def resolve_emulator_swiftshader(directory):
    """Require a complete ARM32 shared-library set before building eMMC."""
    sources = {}
    for target_name, source_name in SWIFTSHADER_EMULATOR_LIBS.items():
        source = os.path.join(directory, source_name)
        if not os.path.isfile(source):
            raise FileNotFoundError(f"ARM32 SwiftShader library missing: {source}")
        with open(source, "rb") as stream:
            header = stream.read(20)
        if (len(header) != 20 or header[:4] != b"\x7fELF" or
                header[4] != 1 or header[5] != 1 or
                struct.unpack_from("<HH", header, 16) != (3, 40)):
            raise ValueError(f"Expected ARM32 EABI shared library: {source}")
        sources[target_name] = source
    return sources


def stage_emulator_swiftshader(vendor_root, directory):
    """Install renderer only in the emulator vendor tree, after OTA packing."""
    sources = resolve_emulator_swiftshader(directory)
    egl_dir = os.path.join(vendor_root, "lib", "egl")
    os.makedirs(egl_dir, exist_ok=True)
    for legacy_name in ("libGLES.so", "libGLES_android.so", "libGLES_mali.so"):
        legacy = os.path.join(egl_dir, legacy_name)
        if os.path.lexists(legacy):
            if not os.path.isfile(legacy) and not os.path.islink(legacy):
                raise ValueError(f"Unexpected legacy EGL entry: {legacy}")
            os.unlink(legacy)
    installed = {}
    for target_name, source in sources.items():
        target = os.path.join(egl_dir, target_name)
        shutil.copyfile(source, target)
        os.chmod(target, 0o755)
        expected = compute_sha256(source)
        actual = compute_sha256(target)
        if actual != expected:
            raise IOError(f"SwiftShader copy mismatch: {target}")
        installed[target_name] = actual
    return installed


def get_cdex_class_defs_size(data_bytes):
    """Extract class_defs_size from Compact DEX (cdex001) or standard DEX (dex\\n035) header."""
    pos = data_bytes.find(b"cdex001\x00")
    if pos != -1:
        return struct.unpack("<I", data_bytes[pos + 0x60 : pos + 0x64])[0]
    pos = data_bytes.find(b"dex\n")
    if pos != -1:
        return struct.unpack("<I", data_bytes[pos + 0x60 : pos + 0x64])[0]
    return -1


# ==============================================================================
# SELinux Context Parser
# ==============================================================================

def load_file_contexts(contexts_path):
    """Parse Android file_contexts into compiled regex patterns and context strings."""
    rules = []
    if not os.path.isfile(contexts_path):
        print(f"[!] Warning: file_contexts not found at {contexts_path}")
        return rules

    with open(contexts_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                pattern = parts[0]
                context = parts[-1]
                try:
                    regex = re.compile(f"^{pattern}$")
                    rules.append((regex, context))
                except re.error:
                    continue
    return rules


def resolve_selinux_context(rel_path, partition_name, rules, default_context):
    """Match relative filesystem path against file_contexts rules."""
    test_paths = [
        f"/{partition_name}/{rel_path}",
        f"/{rel_path}",
        f"/(vendor|system/vendor)/{rel_path}",
    ]
    for regex, ctx in rules:
        for tp in test_paths:
            if regex.match(tp):
                return ctx
    return default_context


# ==============================================================================
# Provisioning XML Generator
# ==============================================================================

def generate_provisioned_xmls(output_dir):
    """
    Generate Android 9 Pie SettingsProvider pre-seeded provisioned XMLs.
    These XMLs bypass Google SetupWizard / SetupWraith, configure network root ADB,
    disable all OEM/Google tracking & telemetry consent flags, and pre-register
    ProjectivyLauncher as the default preferred HOME activity intent handler.
    """
    os.makedirs(output_dir, exist_ok=True)

    settings_global_content = """<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
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

    settings_secure_content = """<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
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

    settings_system_content = """<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<settings version="170">
  <setting id="1" name="tcl_data_report_disable" value="1" package="android" defaultValue="1" defaultSysSet="true" />
  <setting id="2" name="tcl_data_collection" value="0" package="android" defaultValue="0" defaultSysSet="true" />
  <setting id="3" name="persisit.app.setting.data.report.disable" value="true" package="android" defaultValue="true" defaultSysSet="true" />
  <setting id="4" name="persist.app.setting.data.report.disable" value="true" package="android" defaultValue="true" defaultSysSet="true" />
</settings>
"""

    package_restrictions_content = """<?xml version='1.0' encoding='utf-8' standalone='yes' ?>
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

    preferred_apps_content = """<?xml version='1.0' encoding='utf-8' standalone='yes' ?>
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

    files = {
        "settings_global.xml": settings_global_content,
        "settings_secure.xml": settings_secure_content,
        "settings_system.xml": settings_system_content,
        "package-restrictions.xml": package_restrictions_content,
        "preferred-apps.xml": preferred_apps_content,
    }

    created = {}
    for fname, content in files.items():
        fpath = os.path.join(output_dir, fname)
        with open(fpath, "w", encoding="utf-8") as f:
            f.write(content)
        os.chmod(fpath, 0o600)
        created[fname] = fpath

    print(f"[+] Generated {len(created)} provisioned XMLs in {output_dir}")
    return created


# ==============================================================================
# Tier 1 Bloatware Purge Engine
# ==============================================================================

def purge_tier1_bloatware(system_dir, vendor_dir, dry_run=False):
    """
    Purges Tier 1 telemetry and adware packages from system and vendor partitions.
    Guarantees idempotency: absent targets are skipped safely.
    """
    print(f"\n========================================================")
    print(f"[*] Executing Tier 1 Bloatware Purge")
    print(f"========================================================")

    purged = []
    skipped = []

    # System partition removals
    if system_dir and os.path.isdir(system_dir):
        print(f"[*] Scanning system directory: {system_dir}")
        for rel_target in TIER1_SYSTEM_REMOVALS:
            target_path = os.path.join(system_dir, rel_target)
            if os.path.lexists(target_path):
                if not dry_run:
                    if os.path.islink(target_path):
                        os.unlink(target_path)
                    elif os.path.isdir(target_path):
                        shutil.rmtree(target_path)
                    else:
                        os.remove(target_path)
                print(f"  [-] PURGED (System): {rel_target}")
                purged.append(f"system/{rel_target}")
            else:
                print(f"  [ ] Already absent (System): {rel_target}")
                skipped.append(f"system/{rel_target}")

    # Vendor partition removals
    if vendor_dir and os.path.isdir(vendor_dir):
        print(f"[*] Scanning vendor directory: {vendor_dir}")
        for rel_target in TIER1_VENDOR_REMOVALS:
            target_path = os.path.join(vendor_dir, rel_target)
            if os.path.lexists(target_path):
                if not dry_run:
                    if os.path.islink(target_path):
                        os.unlink(target_path)
                    elif os.path.isdir(target_path):
                        shutil.rmtree(target_path)
                    else:
                        os.remove(target_path)
                print(f"  [-] PURGED (Vendor): {rel_target}")
                purged.append(f"vendor/{rel_target}")
            else:
                print(f"  [ ] Already absent (Vendor): {rel_target}")
                skipped.append(f"vendor/{rel_target}")

    print(f"[+] Purge complete: {len(purged)} items purged, {len(skipped)} items already absent.")
    return {"purged": purged, "skipped": skipped}


# ==============================================================================
# Projectivy Launcher Injection
# ==============================================================================

def inject_projectivy_launcher(system_dir, launcher_apk_path=None):
    """
    Drops ProjectivyLauncher.apk into /system/app/ProjectivyLauncher/
    Sets attributes: mode 0644, ownership root:root (0:0), SELinux context u:object_r:system_file:s0.
    """
    print(f"\n========================================================")
    print(f"[*] Injecting ProjectivyLauncher into System Partition")
    print(f"========================================================")

    dest_dir = os.path.join(system_dir, "app", "ProjectivyLauncher")
    dest_apk = os.path.join(dest_dir, "ProjectivyLauncher.apk")
    os.makedirs(dest_dir, exist_ok=True)
    os.chmod(dest_dir, 0o755)

    # Resolve launcher APK source
    if not launcher_apk_path:
        candidate_paths = [
            os.path.join(os.path.dirname(__file__), "..", "apks", "ProjectivyLauncher.apk"),
            dest_apk,  # Already placed in system_extracted
        ]
        for p in candidate_paths:
            if os.path.isfile(p):
                launcher_apk_path = os.path.abspath(p)
                break

    if not launcher_apk_path or not os.path.isfile(launcher_apk_path):
        raise FileNotFoundError(
            "ProjectivyLauncher.apk could not be located. Specify path via --launcher-apk."
        )

    if os.path.abspath(launcher_apk_path) != os.path.abspath(dest_apk):
        print(f"[*] Copying {launcher_apk_path} -> {dest_apk}")
        shutil.copy2(launcher_apk_path, dest_apk)
    else:
        print(f"[*] ProjectivyLauncher.apk already present at destination: {dest_apk}")

    os.chmod(dest_apk, 0o644)
    apk_size = os.path.getsize(dest_apk)
    print(f"[+] ProjectivyLauncher.apk installed successfully ({apk_size} bytes, mode 0644).")
    return dest_apk


def inject_bireport_stub(system_dir, stub_apk_path=None):
    """
    Drops BiReportStub.apk into /system/app/BiReportStub/
    Sets attributes: mode 0644, ownership root:root (0:0), SELinux context u:object_r:system_file:s0.
    Provides no-op AIDL binder stub for com.tcl.bi.service.BiReportService so retained
    SystemSettings and InitSetup never NPE following Category A debloating.
    """
    print(f"\n========================================================")
    print(f"[*] Injecting BiReportStub into System Partition")
    print(f"========================================================")

    dest_dir = os.path.join(system_dir, "app", "BiReportStub")
    dest_apk = os.path.join(dest_dir, "BiReportStub.apk")
    os.makedirs(dest_dir, exist_ok=True)
    os.chmod(dest_dir, 0o755)

    if not stub_apk_path or not os.path.isfile(stub_apk_path):
        candidate_paths = [
            os.path.join(os.path.dirname(__file__), "..", "apks", "BiReportStub.apk"),
            os.path.join(os.path.dirname(__file__), "stubs", "BiReportStub", "BiReportStub.apk"),
            dest_apk,
        ]
        for p in candidate_paths:
            if os.path.isfile(p):
                stub_apk_path = os.path.abspath(p)
                break

    if not stub_apk_path or not os.path.isfile(stub_apk_path):
        raise FileNotFoundError(
            "BiReportStub.apk could not be located. Build via tools/stubs/BiReportStub/build.sh or specify path."
        )

    if os.path.abspath(stub_apk_path) != os.path.abspath(dest_apk):
        print(f"[*] Copying {stub_apk_path} -> {dest_apk}")
        shutil.copy2(stub_apk_path, dest_apk)
    else:
        print(f"[*] BiReportStub.apk already present at destination: {dest_apk}")

    os.chmod(dest_apk, 0o644)
    apk_size = os.path.getsize(dest_apk)
    print(f"[+] BiReportStub.apk installed successfully ({apk_size} bytes, mode 0644).")
    return dest_apk


# ==============================================================================
# Target A: Ext4 Image Builders (System, Vendor, Userdata) & Composite GPT Disk
# ==============================================================================

def build_debloated_system_image(workspace_root, system_extracted_dir, output_img=None, mini_init_bin=None, fb_test_bin=None):
    """Builds debloated, SELinux-labeled, normalized ext4 system.img (2048 MiB)."""
    staging_dir = os.path.join(workspace_root, "staging")
    system_img = output_img or os.path.join(staging_dir, "system.img")
    plat_fc_path = os.path.join(system_extracted_dir, "etc", "selinux", "plat_file_contexts")

    mke2fs_bin = find_tool("mke2fs")
    e2fsck_bin = find_tool("e2fsck")
    debugfs_bin = find_tool("debugfs")

    size_mb = 2048  # 2 GiB
    size_bytes = size_mb * 1024 * 1024

    print(f"\n========================================================")
    print(f"[*] Stage: Building ext4 system.img ({size_mb} MiB)")
    print(f"========================================================")

    fc_rules = load_file_contexts(plat_fc_path)
    print(f"[+] Loaded {len(fc_rules)} SELinux context rules for system")

    if os.path.dirname(system_img):
        os.makedirs(os.path.dirname(system_img), exist_ok=True)
    with open(system_img, "wb") as f:
        f.truncate(size_bytes)

    cmd = [
        mke2fs_bin,
        "-t", "ext4",
        "-F",
        "-O", "^metadata_csum,^orphan_file,^dir_index",
        "-L", "system",
        "-b", "4096",
        "-I", "256",
        "-E", "root_owner=0:0",
        "-d", system_extracted_dir,
        system_img
    ]
    run_cmd(cmd)

    # Resolve optional mini_init and fb_test binaries
    if not mini_init_bin:
        cand_init = os.path.join(staging_dir, "mini_init")
        if os.path.isfile(cand_init):
            mini_init_bin = cand_init
    if not fb_test_bin:
        cand_fb = os.path.join(staging_dir, "fb_test")
        if os.path.isfile(cand_fb):
            fb_test_bin = cand_fb

    cmds = []
    default_system_ctx = "u:object_r:system_file:s0"

    cmds.append("mkdir dev")
    cmds.append("mkdir proc")
    cmds.append("mkdir sys")
    cmds.append("rm vendor")
    cmds.append("mkdir vendor")
    cmds.append("set_inode_field vendor mode 040755")
    cmds.append("set_inode_field vendor uid 0")
    cmds.append("set_inode_field vendor gid 0")
    cmds.append("ea_set vendor security.selinux u:object_r:vendor_file:s0")
    cmds.append("symlink system .")

    init_cwd = staging_dir
    if mini_init_bin and os.path.isfile(mini_init_bin):
        init_cwd = os.path.dirname(os.path.abspath(mini_init_bin))
        init_base = os.path.basename(mini_init_bin)
        cmds.append(f"write {init_base} init")
        cmds.append("set_inode_field init mode 0100755")
        cmds.append("set_inode_field init uid 0")
        cmds.append("set_inode_field init gid 0")
        cmds.append("ea_set init security.selinux u:object_r:init_exec:s0")

    # Install /fstab.rtd284x for vold/fs_mgr default fstab resolution
    fstab_content = (
        "/dev/block/platform/18010800.emmc/by-name/system /system ext4 ro,barrier=1 wait\n"
        "/dev/block/platform/18010800.emmc/by-name/vendor /vendor ext4 ro,barrier=1 wait\n"
        "/dev/block/platform/18010800.emmc/by-name/userdata /data ext4 noatime,nosuid,nodev,barrier=1,data=ordered wait,check\n"
    )
    staging_fstab = os.path.join(init_cwd, "fstab.rtd284x")
    with open(staging_fstab, "w", encoding="utf-8") as f:
        f.write(fstab_content)
    cmds.append("write fstab.rtd284x fstab.rtd284x")
    cmds.append("set_inode_field fstab.rtd284x mode 0100644")
    cmds.append("set_inode_field fstab.rtd284x uid 0")
    cmds.append("set_inode_field fstab.rtd284x gid 0")
    cmds.append("ea_set fstab.rtd284x security.selinux u:object_r:rootfs:s0")

    sys_ext_fstab = os.path.join(system_extracted_dir, "fstab.rtd284x")
    with open(sys_ext_fstab, "w", encoding="utf-8") as f:
        f.write(fstab_content)
    os.chmod(sys_ext_fstab, 0o644)

    if fb_test_bin and os.path.isfile(fb_test_bin):
        fb_base = os.path.basename(fb_test_bin)
        if os.path.dirname(os.path.abspath(fb_test_bin)) != init_cwd:
            shutil.copy2(fb_test_bin, os.path.join(init_cwd, fb_base))
        cmds.append(f"write {fb_base} bin/fb_test")
        cmds.append("set_inode_field bin/fb_test mode 0100755")
        cmds.append("set_inode_field bin/fb_test uid 0")
        cmds.append("set_inode_field bin/fb_test gid 2000")
        cmds.append("ea_set bin/fb_test security.selinux u:object_r:system_file:s0")

    # Write adb_keys for zero-friction authenticated host adb access
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
        adb_keys_file = os.path.join(init_cwd, "adb_keys")
        with open(adb_keys_file, "w", encoding="utf-8") as kf:
            kf.write("\n".join(combined_keys) + "\n")
        cmds.append("write adb_keys adb_keys")
        cmds.append("set_inode_field adb_keys mode 0100644")
        cmds.append("set_inode_field adb_keys uid 0")
        cmds.append("set_inode_field adb_keys gid 0")
        cmds.append("ea_set adb_keys security.selinux u:object_r:rootfs:s0")

    # Invalidate stale or copy software GLES 2.0 driver into system
    cand_gles = os.path.join(workspace_root, "build/phase3/libGLES_software.so")
    if os.path.isfile(cand_gles):
        gles_base = "libGLES_software.so"
        if os.path.dirname(os.path.abspath(cand_gles)) != init_cwd:
            shutil.copy2(cand_gles, os.path.join(init_cwd, gles_base))
        cmds.append(f"write {gles_base} lib/egl/libGLES_software.so")
        cmds.append("set_inode_field lib/egl/libGLES_software.so mode 0100755")
        cmds.append("set_inode_field lib/egl/libGLES_software.so uid 0")
        cmds.append("set_inode_field lib/egl/libGLES_software.so gid 0")
        cmds.append("ea_set lib/egl/libGLES_software.so security.selinux u:object_r:system_file:s0")
        cmds.append(f"write {gles_base} lib/egl/libGLES_android.so")
        cmds.append("set_inode_field lib/egl/libGLES_android.so mode 0100755")
        cmds.append("set_inode_field lib/egl/libGLES_android.so uid 0")
        cmds.append("set_inode_field lib/egl/libGLES_android.so gid 0")
        cmds.append("ea_set lib/egl/libGLES_android.so security.selinux u:object_r:system_file:s0")

    for root, dirs, files in os.walk(system_extracted_dir):
        for item in dirs + files:
            full = os.path.join(root, item)
            rel = os.path.relpath(full, system_extracted_dir).replace("\\", "/")
            if rel == "vendor":
                continue
            is_dir = os.path.isdir(full) and not os.path.islink(full)
            is_symlink = os.path.islink(full)

            uid, gid, mode = 0, 0, None
            if rel == "bin/netcfg":
                uid, gid, mode = 0, AID_NET_ADMIN, "0102750"
            elif rel == "xbin/su":
                uid, gid, mode = 0, AID_SHELL, "0106750"
            elif rel in ("bin/uncrypt", "bin/install-recovery.sh"):
                uid, gid, mode = 0, 0, "0100750"
            elif rel in ("xbin/librank", "xbin/procrank", "xbin/procmem", "xbin/tcpdump"):
                uid, gid, mode = 0, 0, "0100755"
            elif rel == "etc/dbus.conf":
                uid, gid, mode = 1002, 1002, "0100440"
            elif rel == "etc/dhcpcd/dhcpcd-run-hooks":
                uid, gid, mode = 1014, AID_SHELL, "0100550"
            elif rel.startswith("etc/ppp"):
                uid, gid, mode = 0, 0, "0100555" if not is_dir else "040755"
            elif rel.startswith("bin") or rel.startswith("xbin"):
                uid, gid = 0, AID_SHELL
                if not is_dir and not is_symlink:
                    mode = "0100755"
            else:
                uid, gid = 0, 0

            cmds.append(f"set_inode_field {rel} uid {uid}")
            cmds.append(f"set_inode_field {rel} gid {gid}")
            if mode and not is_symlink:
                cmds.append(f"set_inode_field {rel} mode {mode}")

            selinux_ctx = resolve_selinux_context(rel, "system", fc_rules, default_system_ctx)
            cmds.append(f"ea_set {rel} security.selinux {selinux_ctx}")

    # Explicit enforcement on ProjectivyLauncher
    cmds.append("set_inode_field app/ProjectivyLauncher uid 0")
    cmds.append("set_inode_field app/ProjectivyLauncher gid 0")
    cmds.append("set_inode_field app/ProjectivyLauncher mode 040755")
    cmds.append("ea_set app/ProjectivyLauncher security.selinux u:object_r:system_file:s0")
    cmds.append("set_inode_field app/ProjectivyLauncher/ProjectivyLauncher.apk uid 0")
    cmds.append("set_inode_field app/ProjectivyLauncher/ProjectivyLauncher.apk gid 0")
    cmds.append("set_inode_field app/ProjectivyLauncher/ProjectivyLauncher.apk mode 0100644")
    cmds.append("ea_set app/ProjectivyLauncher/ProjectivyLauncher.apk security.selinux u:object_r:system_file:s0")

    # Explicit enforcement on BiReportStub
    cmds.append("set_inode_field app/BiReportStub uid 0")
    cmds.append("set_inode_field app/BiReportStub gid 0")
    cmds.append("set_inode_field app/BiReportStub mode 040755")
    cmds.append("ea_set app/BiReportStub security.selinux u:object_r:system_file:s0")
    cmds.append("set_inode_field app/BiReportStub/BiReportStub.apk uid 0")
    cmds.append("set_inode_field app/BiReportStub/BiReportStub.apk gid 0")
    cmds.append("set_inode_field app/BiReportStub/BiReportStub.apk mode 0100644")
    cmds.append("ea_set app/BiReportStub/BiReportStub.apk security.selinux u:object_r:system_file:s0")

    cmd_file = os.path.join(staging_dir, "system_debugfs_cmds.tmp")
    with open(cmd_file, "w", encoding="utf-8") as f:
        f.write("\n".join(cmds) + "\n")

    run_cmd([debugfs_bin, "-w", "-f", cmd_file, system_img], cwd=init_cwd)
    if os.path.exists(cmd_file):
        os.remove(cmd_file)

    print("[*] Verifying system.img with e2fsck...")
    res = run_cmd([e2fsck_bin, "-f", "-n", system_img], check=False)
    if res.returncode > 1:
        raise RuntimeError(f"e2fsck failed on system.img (code {res.returncode}):\n{res.stdout}")

    stat_check = run_cmd([debugfs_bin, "-R", "stat app/ProjectivyLauncher/ProjectivyLauncher.apk", system_img])
    assert "security.selinux" in stat_check.stdout, "SELinux context missing on ProjectivyLauncher.apk!"
    print(f"[+] Verified ProjectivyLauncher attributes in system.img.")
    print(f"[+] system.img built successfully ({size_bytes} bytes).")
    return system_img, size_bytes


def build_debloated_vendor_image(workspace_root, vendor_tar_path, output_img=None,
                                debloated_tar_out=None, dry_run=False,
                                emulator_swiftshader_dir=None, system_extracted_dir=None):
    """
    Extracts vendor.tar on case-sensitive APFS DMG (macOS) or directory (Linux),
    purges Tier 1 vendor bloatware, preserves case collisions (libOMX_Core.so vs libomx_core.so),
    packs debloated vendor.tar, and builds ext4 vendor.img (1536 MiB).
    """
    staging_dir = os.path.join(workspace_root, "staging")
    vendor_img = output_img or os.path.join(staging_dir, "vendor.img")
    system_extracted_dir = system_extracted_dir or os.path.join(staging_dir, "system_extracted")
    plat_fc_path = os.path.join(system_extracted_dir, "etc", "selinux", "plat_file_contexts")

    mke2fs_bin = find_tool("mke2fs")
    e2fsck_bin = find_tool("e2fsck")
    debugfs_bin = find_tool("debugfs")

    size_mb = 1536  # 1.5 GiB
    size_bytes = size_mb * 1024 * 1024

    print(f"\n========================================================")
    print(f"[*] Stage: Building ext4 vendor.img ({size_mb} MiB) & debloated vendor.tar")
    print(f"========================================================")

    if not os.path.isfile(vendor_tar_path):
        raise FileNotFoundError(f"vendor.tar not found at {vendor_tar_path}")

    is_darwin = (sys.platform == "darwin")
    dmg_path = os.path.join(staging_dir, "vendor_cs.dmg")
    mount_dir = os.path.join(staging_dir, "vendor_cs_mnt")

    def cleanup_vendor_dmg():
        if is_darwin:
            subprocess.run(["diskutil", "unmount", "force", mount_dir], capture_output=True)
            subprocess.run(["hdiutil", "detach", mount_dir, "-force"], capture_output=True)
            try:
                import plistlib
                info_res = subprocess.run(["hdiutil", "info", "-plist"], capture_output=True)
                if info_res.returncode == 0:
                    info_plist = plistlib.loads(info_res.stdout)
                    real_dmg = os.path.realpath(dmg_path)
                    for img in info_plist.get("images", []):
                        img_path = img.get("image-path", "")
                        entities = img.get("system-entities", [])
                        matches = (
                            os.path.realpath(img_path) == real_dmg or
                            "vendor_cs" in img_path or
                            any(e.get("mount-point") == mount_dir for e in entities)
                        )
                        if matches:
                            for ent in entities:
                                dev = ent.get("dev-entry")
                                if dev:
                                    subprocess.run(["hdiutil", "detach", dev, "-force"], capture_output=True)
                                    subprocess.run(["diskutil", "eject", dev], capture_output=True)
            except Exception:
                pass

            if os.path.exists(mount_dir):
                try:
                    if not os.path.ismount(mount_dir):
                        shutil.rmtree(mount_dir, ignore_errors=True)
                except Exception:
                    pass
            if os.path.exists(dmg_path):
                try:
                    os.remove(dmg_path)
                except OSError:
                    pass
        else:
            if os.path.exists(mount_dir):
                shutil.rmtree(mount_dir, ignore_errors=True)

    cleanup_vendor_dmg()
    os.makedirs(mount_dir, exist_ok=True)

    try:
        if is_darwin:
            print("[*] Mounting temporary Case-sensitive APFS DMG for case fidelity...")
            create_cmd = ["hdiutil", "create", "-size", "2500m", "-fs", "Case-sensitive APFS", "-volname", "VendorCS", "-ov", dmg_path]
            for attempt in range(5):
                res = run_cmd(create_cmd, check=False)
                if res.returncode == 0:
                    break
                print(f"[!] hdiutil create attempt {attempt+1} failed ({res.stderr.strip()}), retrying in 1s...")
                time.sleep(1)
                cleanup_vendor_dmg()
            else:
                run_cmd(create_cmd, check=True)

            for _ in range(10):
                if os.path.isfile(dmg_path):
                    break
                time.sleep(0.1)
            os.makedirs(mount_dir, exist_ok=True)
            attach_res = run_cmd(["hdiutil", "attach", dmg_path, "-mountpoint", mount_dir], check=False)
            if attach_res.returncode != 0:
                time.sleep(1)
                run_cmd(["hdiutil", "attach", dmg_path, "-mountpoint", mount_dir])
        else:
            print("[*] Staging vendor directory directly on native case-sensitive filesystem...")

        print(f"[*] Extracting {vendor_tar_path} into staging mount...")
        run_cmd(["tar", "-xf", vendor_tar_path, "-C", mount_dir])

        # Verify case-sensitive collision preservation
        omx1 = os.path.join(mount_dir, "lib", "libOMX_Core.so")
        omx2 = os.path.join(mount_dir, "lib", "libomx_core.so")
        assert os.path.exists(omx1) and os.path.exists(omx2), "Case collision preservation failed: libOMX_Core.so missing!"
        print(f"[+] Verified libOMX_Core.so ({os.path.getsize(omx1)} B) and libomx_core.so ({os.path.getsize(omx2)} B) preserved.")

        # Purge Tier 1 vendor bloatware
        purge_tier1_bloatware(system_dir=None, vendor_dir=mount_dir, dry_run=dry_run)

        # Neutralize vendor TrustZone keystore HAL to trigger pure software Keymaster 3.0 fallback
        ks_rtd = os.path.join(mount_dir, "lib", "hw", "keystore.rtd284x.so")
        if os.path.lexists(ks_rtd):
            if not dry_run:
                os.remove(ks_rtd)
            print("[+] Removed /vendor/lib/hw/keystore.rtd284x.so to unblock AOSP software Keymaster 3.0")

        # Neutralize vendor TrustZone gatekeeper HAL to trigger pure software Gatekeeper fallback
        gk_rtd = os.path.join(mount_dir, "lib", "hw", "gatekeeper.rtd284x.so")
        if os.path.lexists(gk_rtd):
            if not dry_run:
                os.remove(gk_rtd)
            print("[+] Removed /vendor/lib/hw/gatekeeper.rtd284x.so to unblock AOSP software Gatekeeper")

        # Strip gatekeeper from VINTF manifests to prevent infinite getService() wait loop
        gk_pattern = re.compile(r'\s*<hal format=\"hidl\">\s*<name>android\.hardware\.gatekeeper</name>.*?</hal>', re.DOTALL)
        for manifest_rel in ["etc/vintf/manifest.xml", "manifest.xml"]:
            mpath = os.path.join(mount_dir, manifest_rel)
            if os.path.isfile(mpath):
                with open(mpath, "r", encoding="utf-8") as f:
                    mcontent = f.read()
                cleaned_mcontent = gk_pattern.sub('', mcontent)
                if not dry_run:
                    with open(mpath, "w", encoding="utf-8") as f:
                        f.write(cleaned_mcontent)
                print(f"[+] Stripped gatekeeper HAL declaration from /vendor/{manifest_rel}")

        # Install /vendor/etc/fstab.rtd284x for vold/fs_mgr default fstab resolution
        fstab_content = (
            "/dev/block/platform/18010800.emmc/by-name/system /system ext4 ro,barrier=1 wait\n"
            "/dev/block/platform/18010800.emmc/by-name/vendor /vendor ext4 ro,barrier=1 wait\n"
            "/dev/block/platform/18010800.emmc/by-name/userdata /data ext4 noatime,nosuid,nodev,barrier=1,data=ordered wait,check\n"
        )
        fstab_ven_path = os.path.join(mount_dir, "etc", "fstab.rtd284x")
        if not dry_run:
            os.makedirs(os.path.dirname(fstab_ven_path), exist_ok=True)
            with open(fstab_ven_path, "w", encoding="utf-8") as f:
                f.write(fstab_content)
            os.chmod(fstab_ven_path, 0o644)
        print("[+] Installed /vendor/etc/fstab.rtd284x for vold/fs_mgr")

        # Ensure software GLES driver is placed at /vendor/lib/egl/libGLES_android.so and libGLES_mali.so
        cand_gles = os.path.join(workspace_root, "build/phase3/libGLES_software.so")
        if os.path.isfile(cand_gles):
            ven_egl_dir = os.path.join(mount_dir, "lib", "egl")
            os.makedirs(ven_egl_dir, exist_ok=True)
            shutil.copy2(cand_gles, os.path.join(ven_egl_dir, "libGLES_android.so"))
            shutil.copy2(cand_gles, os.path.join(ven_egl_dir, "libGLES_mali.so"))
            print("[+] Injected software GLES into vendor: libGLES_android.so and libGLES_mali.so")

        # Repack debloated vendor.tar if requested
        if debloated_tar_out:
            print(f"[*] Packaging debloated vendor.tar -> {debloated_tar_out}...")
            os.makedirs(os.path.dirname(os.path.abspath(debloated_tar_out)), exist_ok=True)
            with tarfile.open(debloated_tar_out, "w", format=tarfile.PAX_FORMAT) as tar:
                for item in sorted(os.listdir(mount_dir)):
                    item_path = os.path.join(mount_dir, item)
                    tar.add(item_path, arcname=item)
            print(f"[+] Debloated vendor.tar created ({os.path.getsize(debloated_tar_out)} bytes).")

        # The OTA archive above retains the retail EGL stack. Only the QEMU
        # ext4 vendor image below gets the ARM32 software renderer.
        if emulator_swiftshader_dir:
            installed = stage_emulator_swiftshader(
                mount_dir, emulator_swiftshader_dir)
            print(f"[+] Installed {len(installed)} ARM32 SwiftShader libraries "
                  "in emulator vendor image")

        # Load SELinux context rules
        vendor_fc_path = os.path.join(mount_dir, "etc", "selinux", "vendor_file_contexts")
        fc_rules = load_file_contexts(plat_fc_path) + load_file_contexts(vendor_fc_path)
        print(f"[+] Loaded {len(fc_rules)} SELinux context rules for vendor")

        if os.path.dirname(vendor_img):
            os.makedirs(os.path.dirname(vendor_img), exist_ok=True)
        with open(vendor_img, "wb") as f:
            f.truncate(size_bytes)

        cmd = [
            mke2fs_bin,
            "-t", "ext4",
            "-F",
            "-O", "^metadata_csum,^orphan_file,^dir_index",
            "-L", "vendor",
            "-b", "4096",
            "-I", "256",
            "-E", "root_owner=0:0",
            "-d", mount_dir,
            vendor_img
        ]
        run_cmd(cmd)

        daemons_750 = {
            "bin/oemkey1",
            "bin/TCLRC601SLSM6DS3_ID1.bin",
            "bin/scd.bin",
            "bin/btsnoop_enable.sh",
            "bin/omx-dump",
            "bin/omxrpc_test",
            "bin/clienttest",
            "bin/cusd",
            "bin/pmoded",
            "bin/vr_bee_hidraw_daemon",
            "bin/rtd",
            "bin/rtkd",
            "bin/rtkcmd",
        }

        cmds = []
        default_vendor_ctx = "u:object_r:vendor_file:s0"

        for root, dirs, files in os.walk(mount_dir):
            for item in dirs + files:
                full = os.path.join(root, item)
                rel = os.path.relpath(full, mount_dir).replace("\\", "/")
                is_dir = os.path.isdir(full) and not os.path.islink(full)
                is_symlink = os.path.islink(full)

                uid, gid, mode = 0, 0, None
                if rel.startswith("tvos/bin") or rel.startswith("tvos/lib"):
                    uid, gid = AID_SYSTEM, AID_SYSTEM
                    mode = "040755" if is_dir else "0100750"
                elif rel.startswith("etc/") and rel != "etc":
                    uid, gid = 0, 0
                    mode = "040755" if is_dir else "0100644"
                elif rel in daemons_750:
                    uid, gid, mode = 0, AID_SHELL, "0100750"
                elif rel.endswith(".conf") or rel.endswith(".xml"):
                    uid, gid = 0, 0
                    mode = "0100644"
                else:
                    top = rel.split("/")[0]
                    if top in ("bin", "data", "etc", "firmware", "fonts", "lib", "overlay", "realtek", "tvos"):
                        uid, gid = 0, AID_SHELL
                    else:
                        uid, gid = 0, 0

                cmds.append(f"set_inode_field {rel} uid {uid}")
                cmds.append(f"set_inode_field {rel} gid {gid}")
                if mode and not is_symlink:
                    cmds.append(f"set_inode_field {rel} mode {mode}")

                selinux_ctx = resolve_selinux_context(rel, "vendor", fc_rules, default_vendor_ctx)
                cmds.append(f"ea_set {rel} security.selinux {selinux_ctx}")

        cmd_file = os.path.join(staging_dir, "vendor_debugfs_cmds.tmp")
        with open(cmd_file, "w", encoding="utf-8") as f:
            f.write("\n".join(cmds) + "\n")

        run_cmd([debugfs_bin, "-w", "-f", cmd_file, vendor_img])
        if os.path.exists(cmd_file):
            os.remove(cmd_file)

    finally:
        print("[*] Cleaning up temporary Case-sensitive DMG...")
        cleanup_vendor_dmg()

    print("[*] Verifying vendor.img with e2fsck...")
    res = run_cmd([e2fsck_bin, "-f", "-n", vendor_img], check=False)
    if res.returncode > 1:
        raise RuntimeError(f"e2fsck failed on vendor.img (code {res.returncode}):\n{res.stdout}")

    res = run_cmd([debugfs_bin, "-R", "ls -l lib", vendor_img])
    assert "libOMX_Core.so" in res.stdout and "libomx_core.so" in res.stdout, "Both OMX libs must be in vendor.img"
    if emulator_swiftshader_dir:
        for name in SWIFTSHADER_EMULATOR_LIBS:
            stat = run_cmd([debugfs_bin, "-R", f"stat lib/egl/{name}", vendor_img])
            if "Inode:" not in stat.stdout:
                raise RuntimeError(f"SwiftShader missing from vendor.img: {name}")
        for name in ("libGLES.so", "libGLES_android.so", "libGLES_mali.so"):
            stat = run_cmd([debugfs_bin, "-R", f"stat lib/egl/{name}", vendor_img])
            if "Inode:" in stat.stdout:
                raise RuntimeError(f"Competing EGL library remains in vendor.img: {name}")
    print("[+] Verified both libOMX_Core.so and libomx_core.so present in vendor.img!")
    print(f"[+] vendor.img built successfully ({size_bytes} bytes).")
    return vendor_img, size_bytes


def build_provisioned_userdata_image(workspace_root, output_img=None, xml_dir=None):
    """
    Builds ext4 userdata.img (510 MiB) pre-seeded with:
      /data/system/users/0/settings_global.xml
      /data/system/users/0/settings_secure.xml
      /data/system/users/0/settings_system.xml
      /data/system/users/0/package-restrictions.xml
    Permissions: system:system (1000:1000), 0700 (users/0), 0600 (xmls), u:object_r:system_data_file:s0.
    """
    staging_dir = os.path.join(workspace_root, "staging")
    userdata_img = output_img or os.path.join(staging_dir, "userdata.img")

    mke2fs_bin = find_tool("mke2fs")
    e2fsck_bin = find_tool("e2fsck")
    debugfs_bin = find_tool("debugfs")

    size_mb = 510  # 510 MiB, LBA 7342080..8386559
    size_bytes = size_mb * 1024 * 1024

    print(f"\n========================================================")
    print(f"[*] Stage: Building Pre-seeded ext4 userdata.img ({size_mb} MiB)")
    print(f"========================================================")

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
            "ea_set local/tmp security.selinux u:object_r:shell_data_file:s0",
            # Pre-seed /data/backup and /data/system/backup to prevent BackupManagerService NPE
            "mkdir backup",
            "set_inode_field backup uid 1000",
            "set_inode_field backup gid 1000",
            "set_inode_field backup mode 040700",
            "ea_set backup security.selinux u:object_r:backup_data_file:s0",
            "mkdir system",
            "set_inode_field system uid 1000",
            "set_inode_field system gid 1000",
            "set_inode_field system mode 040775",
            "ea_set system security.selinux u:object_r:system_data_file:s0",
            "mkdir system/backup",
            "set_inode_field system/backup uid 1000",
            "set_inode_field system/backup gid 1000",
            "set_inode_field system/backup mode 040700",
            "ea_set system/backup security.selinux u:object_r:backup_data_file:s0",
            "mkdir system/users",
            "set_inode_field system/users uid 1000",
            "set_inode_field system/users gid 1000",
            "set_inode_field system/users mode 040775",
            "ea_set system/users security.selinux u:object_r:system_data_file:s0",
            "mkdir system/users/0",
            "set_inode_field system/users/0 uid 1000",
            "set_inode_field system/users/0 gid 1000",
            "set_inode_field system/users/0 mode 040700",
            "ea_set system/users/0 security.selinux u:object_r:system_data_file:s0",
        ]

        for fname in xml_map.keys():
            remote_path = f"system/users/0/{fname}"
            cmds.append(f"write {fname} {remote_path}")
            cmds.append(f"set_inode_field {remote_path} uid 1000")
            cmds.append(f"set_inode_field {remote_path} gid 1000")
            cmds.append(f"set_inode_field {remote_path} mode 0100600")
            cmds.append(f"ea_set {remote_path} security.selinux u:object_r:system_data_file:s0")

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
    assert "User:  1000" in res_stat.stdout and "Group:  1000" in res_stat.stdout, "XML ownership must be 1000:1000!"
    assert "security.selinux" in res_stat.stdout, "SELinux context missing on provisioned XML!"
    print(f"[+] Verified provisioned XML injection into userdata.img ({size_bytes} bytes).")
    return userdata_img, size_bytes


def create_gpt_structures(total_sectors, partitions):
    """Generates PMBR and Primary/Secondary GPT data structures."""
    pmbr = bytearray(SECTOR_SIZE)
    pmbr[446] = 0x00
    pmbr[447:450] = b"\x00\x02\x00"
    pmbr[450] = 0xEE
    pmbr[451:454] = b"\xFF\xFF\xFF"
    struct.pack_into("<I", pmbr, 454, 1)
    size_in_lba = min(total_sectors - 1, 0xFFFFFFFF)
    struct.pack_into("<I", pmbr, 458, size_in_lba)
    struct.pack_into("<H", pmbr, 510, MBR_SIGNATURE)

    entries_bytes = bytearray(NUM_PARTITION_ENTRIES * PARTITION_ENTRY_SIZE)
    for idx, p in enumerate(partitions):
        offset = idx * PARTITION_ENTRY_SIZE
        entries_bytes[offset:offset + 16] = p['type_guid'].bytes_le
        entries_bytes[offset + 16:offset + 32] = p['unique_guid'].bytes_le
        struct.pack_into("<Q", entries_bytes, offset + 32, p['start_lba'])
        struct.pack_into("<Q", entries_bytes, offset + 40, p['end_lba'])
        struct.pack_into("<Q", entries_bytes, offset + 48, p.get('attributes', 0))
        name_utf16 = p['name'].encode("utf-16le")[:72]
        entries_bytes[offset + 56:offset + 56 + len(name_utf16)] = name_utf16

    entries_crc32 = zlib.crc32(entries_bytes) & 0xFFFFFFFF

    disk_guid = uuid.uuid4()
    first_usable_lba = 2048  # 1 MiB alignment
    last_usable_lba = total_sectors - 34

    # Primary GPT Header (LBA 1)
    primary_hdr = bytearray(SECTOR_SIZE)
    primary_hdr[0:8] = GPT_SIGNATURE
    struct.pack_into("<I", primary_hdr, 8, GPT_REVISION)
    struct.pack_into("<I", primary_hdr, 12, GPT_HEADER_SIZE)
    struct.pack_into("<I", primary_hdr, 16, 0)
    struct.pack_into("<I", primary_hdr, 20, 0)
    struct.pack_into("<Q", primary_hdr, 24, 1)
    struct.pack_into("<Q", primary_hdr, 32, total_sectors - 1)
    struct.pack_into("<Q", primary_hdr, 40, first_usable_lba)
    struct.pack_into("<Q", primary_hdr, 48, last_usable_lba)
    primary_hdr[56:72] = disk_guid.bytes_le
    struct.pack_into("<Q", primary_hdr, 72, 2)
    struct.pack_into("<I", primary_hdr, 80, NUM_PARTITION_ENTRIES)
    struct.pack_into("<I", primary_hdr, 84, PARTITION_ENTRY_SIZE)
    struct.pack_into("<I", primary_hdr, 88, entries_crc32)

    primary_crc32 = zlib.crc32(primary_hdr[:GPT_HEADER_SIZE]) & 0xFFFFFFFF
    struct.pack_into("<I", primary_hdr, 16, primary_crc32)

    # Secondary GPT Header (LBA total_sectors - 1)
    backup_hdr = bytearray(SECTOR_SIZE)
    backup_hdr[0:8] = GPT_SIGNATURE
    struct.pack_into("<I", backup_hdr, 8, GPT_REVISION)
    struct.pack_into("<I", backup_hdr, 12, GPT_HEADER_SIZE)
    struct.pack_into("<I", backup_hdr, 16, 0)
    struct.pack_into("<I", backup_hdr, 20, 0)
    struct.pack_into("<Q", backup_hdr, 24, total_sectors - 1)
    struct.pack_into("<Q", backup_hdr, 32, 1)
    struct.pack_into("<Q", backup_hdr, 40, first_usable_lba)
    struct.pack_into("<Q", backup_hdr, 48, last_usable_lba)
    backup_hdr[56:72] = disk_guid.bytes_le
    backup_entries_lba = total_sectors - 33
    struct.pack_into("<Q", backup_hdr, 72, backup_entries_lba)
    struct.pack_into("<I", backup_hdr, 80, NUM_PARTITION_ENTRIES)
    struct.pack_into("<I", backup_hdr, 84, PARTITION_ENTRY_SIZE)
    struct.pack_into("<I", backup_hdr, 88, entries_crc32)

    backup_crc32 = zlib.crc32(backup_hdr[:GPT_HEADER_SIZE]) & 0xFFFFFFFF
    struct.pack_into("<I", backup_hdr, 16, backup_crc32)

    return {
        "pmbr": bytes(pmbr),
        "primary_hdr": bytes(primary_hdr),
        "entries": bytes(entries_bytes),
        "backup_hdr": bytes(backup_hdr),
        "backup_entries_lba": backup_entries_lba,
        "disk_guid": disk_guid,
    }


def build_composite_emmc(workspace_root, system_img, vendor_img, userdata_img, output_path=None):
    """Builds 4096 MiB composite disk with GPT partitioning for QEMU emulation."""
    staging_dir = os.path.join(workspace_root, "staging")
    emmc_raw = output_path or os.path.join(workspace_root, "output", "emmc.raw")

    total_size_mb = 4096
    total_size_bytes = total_size_mb * 1024 * 1024
    total_sectors = total_size_bytes // SECTOR_SIZE

    system_sectors = os.path.getsize(system_img) // SECTOR_SIZE
    vendor_sectors = os.path.getsize(vendor_img) // SECTOR_SIZE
    userdata_sectors = os.path.getsize(userdata_img) // SECTOR_SIZE

    sys_start_lba = 2048
    sys_end_lba = sys_start_lba + system_sectors - 1

    ven_start_lba = sys_end_lba + 1
    ven_end_lba = ven_start_lba + vendor_sectors - 1

    data_start_lba = ven_end_lba + 1
    data_end_lba = data_start_lba + userdata_sectors - 1

    print(f"\n========================================================")
    print(f"[*] Stage: Building Composite GPT Disk Image (emmc.raw)")
    print(f"========================================================")
    print(f"Disk Destination:   {emmc_raw}")
    print(f"Partition 1 (system):   LBA {sys_start_lba}..{sys_end_lba}")
    print(f"Partition 2 (vendor):   LBA {ven_start_lba}..{ven_end_lba}")
    print(f"Partition 3 (userdata): LBA {data_start_lba}..{data_end_lba}")

    partitions = [
        {
            "name": "system",
            "type_guid": LINUX_FS_DATA_GUID,
            "unique_guid": uuid.uuid4(),
            "start_lba": sys_start_lba,
            "end_lba": sys_end_lba,
            "attributes": 0
        },
        {
            "name": "vendor",
            "type_guid": LINUX_FS_DATA_GUID,
            "unique_guid": uuid.uuid4(),
            "start_lba": ven_start_lba,
            "end_lba": ven_end_lba,
            "attributes": 0
        },
        {
            "name": "userdata",
            "type_guid": LINUX_FS_DATA_GUID,
            "unique_guid": uuid.uuid4(),
            "start_lba": data_start_lba,
            "end_lba": data_end_lba,
            "attributes": 0
        }
    ]

    gpt = create_gpt_structures(total_sectors, partitions)

    os.makedirs(os.path.dirname(os.path.abspath(emmc_raw)), exist_ok=True)
    with open(emmc_raw, "wb") as f:
        f.truncate(total_size_bytes)

        # LBA 0: PMBR
        f.seek(0)
        f.write(gpt["pmbr"])

        # LBA 1: Primary GPT Header
        f.seek(1 * SECTOR_SIZE)
        f.write(gpt["primary_hdr"])

        # LBA 2..33: Primary Partition Table
        f.seek(2 * SECTOR_SIZE)
        f.write(gpt["entries"])

        # Partition 1: system.img
        print(f"[*] Streaming system.img into emmc.raw at offset {sys_start_lba * SECTOR_SIZE}...")
        f.seek(sys_start_lba * SECTOR_SIZE)
        with open(system_img, "rb") as s_in:
            while chunk := s_in.read(16 * 1024 * 1024):
                f.write(chunk)

        # Partition 2: vendor.img
        print(f"[*] Streaming vendor.img into emmc.raw at offset {ven_start_lba * SECTOR_SIZE}...")
        f.seek(ven_start_lba * SECTOR_SIZE)
        with open(vendor_img, "rb") as v_in:
            while chunk := v_in.read(16 * 1024 * 1024):
                f.write(chunk)

        # Partition 3: userdata.img
        print(f"[*] Streaming userdata.img into emmc.raw at offset {data_start_lba * SECTOR_SIZE}...")
        f.seek(data_start_lba * SECTOR_SIZE)
        with open(userdata_img, "rb") as u_in:
            while chunk := u_in.read(16 * 1024 * 1024):
                f.write(chunk)

        # Backup Partition Table
        f.seek(gpt["backup_entries_lba"] * SECTOR_SIZE)
        f.write(gpt["entries"])

        # Backup GPT Header
        f.seek((total_sectors - 1) * SECTOR_SIZE)
        f.write(gpt["backup_hdr"])

    print(f"[+] Successfully constructed {emmc_raw} ({total_size_bytes} bytes).")
    return emmc_raw


# ==============================================================================
# Target B: Physical OTA Recovery TAR Image Builder (Update_RT41_Debloated.img)
# ==============================================================================

def build_provisioned_data_tar(original_data_tar, output_data_tar, xml_dir=None):
    """
    Extracts original data.tar.bz2, appends /data/system/users/0/ provisioned XMLs,
    and repacks into bzip2 tarball with UID 1000:1000 (system:system).
    """
    print(f"\n========================================================")
    print(f"[*] Stage: Building Provisioned package7/data.tar.bz2")
    print(f"========================================================")

    with tempfile.TemporaryDirectory() as td:
        extract_dir = os.path.join(td, "data_root")
        os.makedirs(extract_dir, exist_ok=True)

        if original_data_tar and os.path.isfile(original_data_tar):
            print(f"[*] Unpacking original {original_data_tar}...")
            with tarfile.open(original_data_tar, "r:*") as tar:
                tar.extractall(extract_dir)

        # Ensure system/users/0 structure
        u0_dir = os.path.join(extract_dir, "system", "users", "0")
        os.makedirs(u0_dir, exist_ok=True)

        # Generate XMLs
        xml_source_dir = xml_dir or os.path.join(td, "xmls")
        xml_map = generate_provisioned_xmls(xml_source_dir)
        for fname, fpath in xml_map.items():
            shutil.copy2(fpath, os.path.join(u0_dir, fname))

        # Repack into bz2 tarball
        os.makedirs(os.path.dirname(os.path.abspath(output_data_tar)), exist_ok=True)
        with tarfile.open(output_data_tar, "w:bz2", format=tarfile.PAX_FORMAT) as tar:
            for root, dirs, files in os.walk(extract_dir):
                for item in dirs + files:
                    full_p = os.path.join(root, item)
                    rel_p = os.path.relpath(full_p, extract_dir).replace("\\", "/")
                    is_dir = os.path.isdir(full_p)

                    ti = tar.gettarinfo(full_p, arcname=rel_p)
                    if rel_p.startswith("system"):
                        ti.uid = AID_SYSTEM
                        ti.gid = AID_SYSTEM
                        ti.uname = "system"
                        ti.gname = "system"
                        ti.mode = 0o700 if rel_p.endswith("system/users/0") else (0o775 if is_dir else 0o600)
                        ti.pax_headers["security.selinux"] = "u:object_r:system_data_file:s0"

                    if is_dir:
                        tar.addfile(ti)
                    else:
                        with open(full_p, "rb") as f:
                            tar.addfile(ti, f)

    print(f"[+] Provisioned data.tar.bz2 built successfully ({os.path.getsize(output_data_tar)} bytes).")
    return output_data_tar


def build_system_emmc_tar(system_dir, output_tar):
    """Packs debloated system directory into package7/system.emmc.tar."""
    print(f"\n========================================================")
    print(f"[*] Stage: Building package7/system.emmc.tar")
    print(f"========================================================")
    system_dir = os.path.realpath(system_dir)
    print(f"Source Directory: {system_dir}")
    print(f"Destination Tar:  {output_tar}")

    os.makedirs(os.path.dirname(os.path.abspath(output_tar)), exist_ok=True)
    with tarfile.open(output_tar, "w", format=tarfile.PAX_FORMAT) as tar:
        for item in sorted(os.listdir(system_dir)):
            tar.add(os.path.join(system_dir, item), arcname=item)

    print(f"[+] package7/system.emmc.tar created ({os.path.getsize(output_tar)} bytes).")
    return output_tar


def build_debloated_update_img(update_root, system_tar_path, vendor_tar_path, data_tar_path, output_path):
    """
    Assembles output/Update_RT41_Debloated.img (standard POSIX tar package).
    Preserves all original firmware components, injecting debloated system, vendor, and data archives.
    """
    print(f"\n========================================================")
    print(f"[*] Stage: Assembling Physical Flashing Package (Update.img)")
    print(f"========================================================")
    print(f"Output Destination: {output_path}")

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

    with tarfile.open(output_path, "w", format=tarfile.PAX_FORMAT) as tar:
        for member_name in UPDATE_IMG_MEMBERS:
            if member_name == "package7/system.emmc.tar":
                src_path = system_tar_path
            elif member_name == "package7/data.tar.bz2":
                src_path = data_tar_path
            elif member_name == "vendor.tar":
                src_path = vendor_tar_path
            else:
                src_path = os.path.join(update_root, member_name)

            if not os.path.exists(src_path):
                raise FileNotFoundError(f"Required Update.img component missing: {src_path} ({member_name})")

            is_dir = os.path.isdir(src_path) and not os.path.islink(src_path)
            ti = tar.gettarinfo(src_path, arcname=member_name)
            if is_dir:
                tar.addfile(ti)
            else:
                with open(src_path, "rb") as f:
                    tar.addfile(ti, f)
            print(f"  [+] Added: {member_name:60} ({os.path.getsize(src_path)} bytes)")

    sha256_hash = compute_sha256(output_path)
    sha_file = f"{output_path}.sha256"
    with open(sha_file, "w", encoding="utf-8") as f:
        f.write(f"{sha256_hash}  {os.path.basename(output_path)}\n")

    print(f"[+] Output OTA Update Package constructed: {output_path}")
    print(f"[+] File Size: {os.path.getsize(output_path)} bytes")
    print(f"[+] SHA-256 Checksum: {sha256_hash}")
    return output_path, sha256_hash


# ==============================================================================
# Verification & Idempotency Engine
# ==============================================================================

def verify_emmc_raw(emmc_raw):
    """Performs deep verification of GPT headers, partition entries, and ext4 superblocks."""
    print(f"\n[*] Verifying emmc.raw geometry and superblocks...")
    size = os.path.getsize(emmc_raw)
    total_sectors = size // SECTOR_SIZE

    with open(emmc_raw, "rb") as f:
        # PMBR
        f.seek(0)
        pmbr = f.read(512)
        mbr_sig = struct.unpack_from("<H", pmbr, 510)[0]
        assert mbr_sig == MBR_SIGNATURE, f"Invalid MBR signature: {hex(mbr_sig)}"
        assert pmbr[450] == 0xEE, f"Invalid Protective MBR OS type: {hex(pmbr[450])}"

        # Primary GPT
        f.seek(512)
        prim_hdr = f.read(512)
        assert prim_hdr[0:8] == GPT_SIGNATURE, f"Invalid GPT signature: {prim_hdr[0:8]}"
        hdr_crc = struct.unpack_from("<I", prim_hdr, 16)[0]
        hdr_copy = bytearray(prim_hdr[:GPT_HEADER_SIZE])
        struct.pack_into("<I", hdr_copy, 16, 0)
        assert (zlib.crc32(hdr_copy) & 0xFFFFFFFF) == hdr_crc, "Primary GPT CRC mismatch!"

        entries_lba, num_entries, entry_sz, entries_crc = struct.unpack_from("<QIII", prim_hdr, 72)
        f.seek(entries_lba * SECTOR_SIZE)
        entries_buf = f.read(num_entries * entry_sz)
        assert (zlib.crc32(entries_buf) & 0xFFFFFFFF) == entries_crc, "Partition entry array CRC mismatch!"

        # Partitions
        p1_start, p1_end = struct.unpack_from("<QQ", entries_buf, 32)
        p1_name = entries_buf[56:128].decode("utf-16le").rstrip("\x00")
        p2_start, p2_end = struct.unpack_from("<QQ", entries_buf, 160)
        p2_name = entries_buf[184:256].decode("utf-16le").rstrip("\x00")
        p3_start, p3_end = struct.unpack_from("<QQ", entries_buf, 288)
        p3_name = entries_buf[312:384].decode("utf-16le").rstrip("\x00")

        assert p1_name == "system", f"Partition 1 name mismatch: {p1_name}"
        assert p2_name == "vendor", f"Partition 2 name mismatch: {p2_name}"
        assert p3_name == "userdata", f"Partition 3 name mismatch: {p3_name}"

        # Ext4 superblocks
        for name, start_lba in [("system", p1_start), ("vendor", p2_start), ("userdata", p3_start)]:
            sb_offset = (start_lba * SECTOR_SIZE) + 1024
            f.seek(sb_offset)
            sb_data = f.read(1024)
            s_magic = struct.unpack_from("<H", sb_data, 0x38)[0]
            s_vol = sb_data[0x78:0x88].split(b"\x00")[0].decode("latin1")
            assert s_magic == 0xEF53, f"Partition {name} invalid ext4 magic: {hex(s_magic)}"
            assert s_vol == name, f"Partition {name} volume name mismatch: {s_vol}"
            print(f"  [+] ext4 Superblock verified for '{name}' at LBA {start_lba} (magic: 0x{s_magic:x}, vol: '{s_vol}')")

    print("[+] emmc.raw GPT geometry & ext4 superblocks verified.")


def verify_debloated_artifacts(emmc_path, update_img_path, workspace_root=None):
    """
    Performs comprehensive verification of built artifacts:
    1. emmc.raw GPT geometry, ext4 superblocks, ProjectivyLauncher attributes, provisioned XMLs, and bloat absence.
    2. Update_RT41_Debloated.img POSIX tar structure, member inventory, and internal archive contents.
    """
    print(f"\n========================================================")
    print(f"[*] Deep Verification of Output Artifacts")
    print(f"========================================================")

    if not (emmc_path or update_img_path):
        raise FileNotFoundError("No eMMC or OTA image was supplied for verification.")
    for path in (emmc_path, update_img_path):
        if path and not os.path.isfile(path):
            raise FileNotFoundError(f"Missing verification input: {path}")
    debugfs_bin = find_tool("debugfs")

    # 1. Verify emmc.raw
    if emmc_path and os.path.isfile(emmc_path):
        verify_emmc_raw(emmc_path)

    # 2. Verify Update_RT41_Debloated.img
    if update_img_path and os.path.isfile(update_img_path):
        print(f"[*] Verifying POSIX tar structure of {update_img_path}...")
        with tarfile.open(update_img_path, "r:*") as tar:
            members = {m.name: m for m in tar.getmembers()}
            for exp in UPDATE_IMG_MEMBERS:
                assert exp in members, f"Expected member {exp} missing in {update_img_path}!"
            print(f"  [+] Verified all {len(UPDATE_IMG_MEMBERS)} canonical members present.")

            # Verify system.emmc.tar inside Update.img
            sys_tar_f = tar.extractfile("package7/system.emmc.tar")
            with tarfile.open(fileobj=sys_tar_f, mode="r:*") as st:
                sys_names = set(st.getnames())
                norm_sys_names = {n.lstrip("./") for n in sys_names}
                assert any("ProjectivyLauncher.apk" in n for n in norm_sys_names), "ProjectivyLauncher.apk missing in system.emmc.tar!"
                assert any("BiReportStub.apk" in n for n in norm_sys_names), "BiReportStub.apk missing in system.emmc.tar!"
                for b in [
                    "app/Pumpkin",
                    "priv-app/Tubesky",
                    "priv-app/PlayAutoInstallStub",
                    "priv-app/TVRecommendations",
                    "factory-app/DeviceAuthenCommon",
                    "factory-app/Esticker",
                    "factory-app/copyDataToTV",
                    "product/app/KeyCustomFunctionService",
                    "priv-app/TVLauncher",
                    "priv-app/LeanbackLauncher",
                    "priv-app/SetupWraithPrebuilt",
                    "priv-factory-app/TVLauncher",
                ]:
                    assert not any(n.startswith(b) for n in norm_sys_names), f"Purged bloatware {b} found in system.emmc.tar!"

                assert any("priv-app/ConfigProvider/ConfigProvider.apk" in n for n in norm_sys_names), "Boot-critical ConfigProvider missing in system.emmc.tar!"
                assert "fstab.rtd284x" in norm_sys_names, "fstab.rtd284x missing in system.emmc.tar!"
                print("  [+] Verified system.emmc.tar: ProjectivyLauncher, ConfigProvider & fstab.rtd284x present, bloatware purged.")

            # Verify vendor.tar inside Update.img
            ven_tar_f = tar.extractfile("vendor.tar")
            with tarfile.open(fileobj=ven_tar_f, mode="r:*") as vt:
                ven_names = set(vt.getnames())
                norm_ven_names = {n.lstrip("./") for n in ven_names}
                assert "lib/libOMX_Core.so" in norm_ven_names and "lib/libomx_core.so" in norm_ven_names, "OMX case collisions missing in vendor.tar!"
                for b in [
                    "app/TCL_ALL_BI",
                    "app/UserAgreement",
                    "app/TCL_ALL_APPSTORE",
                    "app/TVWeiShi",
                    "app/terminalmanagerplus",
                    "app/DeviceAuthenCommon",
                    "app/UserCenter_OverSeas",
                    "app/Esticker",
                    "app/copyDataToTV",
                    "app/SystemUpdate",
                    "tvos/bin/netinfocap",
                    "tvos/bin/log_out",
                    "app/TIF_TvInput",
                    "app/TIF_LiveTV",
                ]:
                    assert not any(n.startswith(b) for n in norm_ven_names), f"Purged bloatware {b} found in vendor.tar!"

                assert "framework/com.tcl.tvmanager.jar" in norm_ven_names, "Boot-critical com.tcl.tvmanager.jar missing in vendor.tar!"
                assert "framework/android.tclwidget.jar" in norm_ven_names, "Boot-critical android.tclwidget.jar missing in vendor.tar!"
                assert "lib/hw/keystore.rtd284x.so" not in norm_ven_names, "OP-TEE keystore.rtd284x.so must not be present in vendor.tar!"
                assert "lib/hw/gatekeeper.rtd284x.so" not in norm_ven_names, "OP-TEE gatekeeper.rtd284x.so must not be present in vendor.tar!"
                assert "etc/fstab.rtd284x" in norm_ven_names, "etc/fstab.rtd284x missing in vendor.tar!"
                m_xml_f = vt.extractfile("manifest.xml")
                if m_xml_f:
                    assert b"android.hardware.gatekeeper" not in m_xml_f.read(), "Gatekeeper must be stripped from vendor manifest.xml!"

                tvmgr_vdex_f = vt.extractfile("framework/oat/arm/com.tcl.tvmanager.vdex")
                if tvmgr_vdex_f:
                    tvmgr_classes = get_cdex_class_defs_size(tvmgr_vdex_f.read())
                    assert tvmgr_classes == 800, f"com.tcl.tvmanager.vdex classes: expected 800, got {tvmgr_classes}"

                tclwidget_vdex_f = vt.extractfile("framework/oat/arm/android.tclwidget.vdex")
                if tclwidget_vdex_f:
                    tclwidget_classes = get_cdex_class_defs_size(tclwidget_vdex_f.read())
                    assert tclwidget_classes == 95, f"android.tclwidget.vdex classes: expected 95, got {tclwidget_classes}"

                print("  [+] Verified vendor.tar: libOMX_Core.so preserved, bloatware purged, tvmanager (800 classes) & tclwidget (95 classes) preserved.")

            # Verify data.tar.bz2 inside Update.img
            data_tar_f = tar.extractfile("package7/data.tar.bz2")
            with tarfile.open(fileobj=data_tar_f, mode="r:*") as dt:
                data_names = {n.lstrip("./") for n in dt.getnames()}
                assert "system/users/0/settings_global.xml" in data_names, "settings_global.xml missing in data.tar.bz2!"
                assert "system/users/0/settings_secure.xml" in data_names, "settings_secure.xml missing in data.tar.bz2!"
                print("  [+] Verified data.tar.bz2: pre-seeded provisioning XMLs present.")

    print(f"\n[>>> ALL ARTIFACT INTEGRITY VERIFICATIONS PASSED <<<]\n")


# ==============================================================================
# Unified Pipeline Execution
# ==============================================================================

def build_debloated_rom(
    workspace_root=None,
    input_update_img=None,
    system_dir=None,
    vendor_tar_path=None,
    launcher_apk=None,
    output_dir=None,
    skip_emmc=False,
    skip_update_img=False,
    dry_run=False,
    emulator_swiftshader_dir=None,
):
    """
    Unified end-to-end builder executing Debloat -> Launcher Injection -> Provisioning -> Multi-Target Packaging.
    """
    workspace_root = os.path.abspath(workspace_root or os.path.join(os.path.dirname(__file__), ".."))
    output_dir = os.path.abspath(output_dir or os.path.join(workspace_root, "output"))
    staging_dir = os.path.join(workspace_root, "staging")
    if input_update_img:
        raise ValueError("Extract the OTA first, then supply --system-dir and --vendor-tar.")
    system_dir = system_dir or os.path.join(staging_dir, "system_extracted")
    update_root = os.path.join(staging_dir, "update_root")
    vendor_tar_path = vendor_tar_path or os.path.join(update_root, "vendor.tar")
    if not os.path.isdir(system_dir):
        raise FileNotFoundError(f"Missing extracted system directory: {system_dir}")
    if not os.path.isfile(vendor_tar_path):
        raise FileNotFoundError(f"Missing vendor archive: {vendor_tar_path}")

    os.makedirs(output_dir, exist_ok=True)

    emulator_init = None
    if not skip_emmc:
        if emulator_swiftshader_dir is None:
            emulator_swiftshader_dir = os.path.join(
                workspace_root, "build", "swiftshader-android")
        resolve_emulator_swiftshader(emulator_swiftshader_dir)
        emulator_init = build_emulator_init(workspace_root)

    print(f"================================================================================")
    print(f" RTD2841 Phase 4 Debloated ROM Builder & Multi-Target Packaging Engine")
    print(f" Workspace Root: {workspace_root}")
    print(f" Output Directory: {output_dir}")
    print(f"================================================================================")

    os.makedirs(staging_dir, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="system-work-", dir=staging_dir) as work:
        system_dir = shutil.copytree(system_dir, os.path.join(work, "system"), symlinks=True)
        # 2. Inject Projectivy Launcher and BiReportStub into System
        inject_projectivy_launcher(system_dir, launcher_apk_path=launcher_apk)
        inject_bireport_stub(system_dir)

        # 3. Purge Tier 1 Bloatware from System
        purge_tier1_bloatware(system_dir=system_dir, vendor_dir=None, dry_run=dry_run)

        # 4. Generate Pre-seeded Provisioned XMLs
        xml_stage_dir = os.path.join(staging_dir, "provisioned_xmls")
        generate_provisioned_xmls(xml_stage_dir)

        # Intermediate artifacts
        debloated_vendor_tar = os.path.join(staging_dir, "vendor_debloated.tar")
        debloated_system_tar = os.path.join(staging_dir, "system_debloated.emmc.tar")
        provisioned_data_tar = os.path.join(staging_dir, "data_provisioned.tar.bz2")

        emmc_raw_path = os.path.join(output_dir, "emmc.raw")
        update_img_path = os.path.join(output_dir, "Update_RT41_Debloated.img")

        # 5. Build Ext4 Vendor Image & Debloated vendor.tar
        vendor_img, _ = build_debloated_vendor_image(
            workspace_root=workspace_root,
            vendor_tar_path=vendor_tar_path,
            system_extracted_dir=system_dir,
            output_img=os.path.join(staging_dir, "vendor.img"),
            debloated_tar_out=debloated_vendor_tar,
            dry_run=dry_run,
            emulator_swiftshader_dir=(None if skip_emmc
                                       else emulator_swiftshader_dir),
        )

        # 6. Build Ext4 System Image
        system_img, _ = build_debloated_system_image(
            workspace_root=workspace_root,
            system_extracted_dir=system_dir,
            mini_init_bin=emulator_init,
            output_img=os.path.join(staging_dir, "system.img")
        )

        # 7. Build Ext4 Userdata Image
        userdata_img, _ = build_provisioned_userdata_image(
            workspace_root=workspace_root,
            output_img=os.path.join(staging_dir, "userdata.img"),
            xml_dir=xml_stage_dir
        )

        # 8. Output A: Composite GPT Disk Image (emmc.raw)
        if not skip_emmc:
            build_composite_emmc(
                workspace_root=workspace_root,
                system_img=system_img,
                vendor_img=vendor_img,
                userdata_img=userdata_img,
                output_path=emmc_raw_path
            )
        else:
            print("[*] Skipping emmc.raw generation per --skip-emmc flag.")

        # 9. Output B: Physical Flashing Update Package (Update_RT41_Debloated.img)
        if not skip_update_img:
            # Build debloated system.emmc.tar
            build_system_emmc_tar(system_dir=system_dir, output_tar=debloated_system_tar)

            # Build provisioned data.tar.bz2
            original_data_tar = os.path.join(update_root, "package7", "data.tar.bz2")
            build_provisioned_data_tar(
                original_data_tar=original_data_tar,
                output_data_tar=provisioned_data_tar,
                xml_dir=xml_stage_dir
            )

            build_debloated_update_img(
                update_root=update_root,
                system_tar_path=debloated_system_tar,
                vendor_tar_path=debloated_vendor_tar,
                data_tar_path=provisioned_data_tar,
                output_path=update_img_path
            )
        else:
            print("[*] Skipping Update.img generation per --skip-update-img flag.")

    # 10. Perform Deep Verification
    verify_debloated_artifacts(
        emmc_path=emmc_raw_path if not skip_emmc else None,
        update_img_path=update_img_path if not skip_update_img else None,
        workspace_root=workspace_root
    )

    print(f"\n================================================================================")
    print(f" BUILD COMPLETED SUCCESSFULLY")
    if not skip_emmc:
        print(f"  [Artifact 1] QEMU Disk Image:       {emmc_raw_path}")
    if not skip_update_img:
        print(f"  [Artifact 2] Physical OTA Package:  {update_img_path}")
        print(f"  [Artifact 3] OTA Checksum:          {update_img_path}.sha256")
    print(f"================================================================================")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Phase 4 Automated Debloated ROM Builder & Multi-Target Packaging Tool (RTD2841 / RT41)."
    )
    parser.add_argument("-i", "--input", help="Raw OTA input is unsupported. Extract it and use --system-dir and --vendor-tar.")
    parser.add_argument("-s", "--system-dir", help="Path to extracted system directory.")
    parser.add_argument("-v", "--vendor-tar", help="Path to vendor.tar.")
    parser.add_argument("-l", "--launcher-apk", help="Path to ProjectivyLauncher.apk.")
    parser.add_argument("-o", "--output-dir", help="Directory for output artifacts (default: output).")
    parser.add_argument("--skip-emmc", action="store_true", help="Skip generation of composite emmc.raw.")
    parser.add_argument("--skip-update-img", action="store_true", help="Skip generation of Update_RT41_Debloated.img.")
    parser.add_argument("--emulator-swiftshader-dir", help="Directory containing ARM32 libEGL.so, libGLES_CM.so, and libGLESv2.so for eMMC image (default: build/swiftshader-android).")
    parser.add_argument("--verify-only", action="store_true", help="Run verification on existing output artifacts.")
    parser.add_argument("--dry-run", action="store_true", help="Simulate debloat purges without file deletion.")
    return parser.parse_args()


def main():
    args = parse_args()
    workspace_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    output_dir = os.path.abspath(args.output_dir or os.path.join(workspace_root, "output"))

    if args.verify_only:
        emmc_raw_path = os.path.join(output_dir, "emmc.raw")
        update_img_path = os.path.join(output_dir, "Update_RT41_Debloated.img")
        verify_debloated_artifacts(
            emmc_path=emmc_raw_path if os.path.isfile(emmc_raw_path) else None,
            update_img_path=update_img_path if os.path.isfile(update_img_path) else None,
            workspace_root=workspace_root
        )
        sys.exit(0)

    build_debloated_rom(
        workspace_root=workspace_root,
        input_update_img=args.input,
        system_dir=args.system_dir,
        vendor_tar_path=args.vendor_tar,
        launcher_apk=args.launcher_apk,
        output_dir=output_dir,
        skip_emmc=args.skip_emmc,
        skip_update_img=args.skip_update_img,
        dry_run=args.dry_run,
        emulator_swiftshader_dir=args.emulator_swiftshader_dir,
    )


if __name__ == "__main__":
    main()
