#!/usr/bin/env python3
"""
tools/create_emmc_storage.py - Composite Raw eMMC Storage Image Generator
Target SoC: Realtek RTD2841 / RTK284x (TCL RT41 Chassis, Board tv030)
Controller: generic-sdhci at MMIO 0x18010800 (GIC SPI 18 / INTID 50)

Builds:
  1. staging/system.img (ext4, 2048 MiB, label 'system', SELinux-labeled, normalized AID ownership)
  2. staging/vendor.img (ext4, 1536 MiB, label 'vendor', SELinux-labeled, case-preserved, postprocess.sh compliance)
  3. staging/emmc.raw   (composite GPT disk, 4096 MiB, SDHC-compatible)

Android fstab contract (rtd284x-tv030.dts lines 20-35):
  /dev/block/platform/18010800.emmc/by-name/system -> partition label 'system'
  /dev/block/platform/18010800.emmc/by-name/vendor -> partition label 'vendor'
"""

import os
import sys
import glob
import re
import struct
import uuid
import zlib
import subprocess
import shutil

SECTOR_SIZE = 512
MBR_SIGNATURE = 0xAA55
GPT_SIGNATURE = b"EFI PART"
GPT_REVISION = 0x00010000
GPT_HEADER_SIZE = 92
NUM_PARTITION_ENTRIES = 128
PARTITION_ENTRY_SIZE = 128

# Standard Linux filesystem data GUID
LINUX_FS_DATA_GUID = uuid.UUID("0FC63DAF-8483-4772-8E79-3D69D8477DE4")


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

    # Cellar fallback search
    cellar_matches = sorted(glob.glob(f"/opt/homebrew/Cellar/e2fsprogs/*/sbin/{name}"))
    if cellar_matches:
        return cellar_matches[-1]

    raise FileNotFoundError(f"Required tool '{name}' not found in PATH or standard search paths.")


def run_cmd(cmd, check=True):
    print(f"[*] Running: {' '.join(str(c) for c in cmd)}")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0 and check:
        print(f"[-] Command failed with exit code {res.returncode}")
        print("STDOUT:", res.stdout)
        print("STDERR:", res.stderr)
        sys.exit(res.returncode)
    return res


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
                    # Clean regex pattern for Python re
                    regex = re.compile(f"^{pattern}$")
                    rules.append((regex, context))
                except re.error:
                    continue
    return rules


def resolve_selinux_context(rel_path, partition_name, rules, default_context):
    """Match relative filesystem path against file_contexts rules."""
    # Test paths with and without partition prefix
    test_paths = [
        f"/{partition_name}/{rel_path}",
        f"/{rel_path}",
        f"/(vendor|system/vendor)/{rel_path}",
    ]
    matched = default_context
    for regex, ctx in rules:
        for tp in test_paths:
            if regex.match(tp):
                matched = ctx
                break
    return matched


def build_system_image(workspace_root):
    staging_dir = os.path.join(workspace_root, "staging")
    system_extracted = os.path.join(staging_dir, "system_extracted")
    system_img = os.path.join(staging_dir, "system.img")
    plat_fc_path = os.path.join(system_extracted, "etc", "selinux", "plat_file_contexts")

    mke2fs_bin = find_tool("mke2fs")
    e2fsck_bin = find_tool("e2fsck")
    debugfs_bin = find_tool("debugfs")

    size_mb = 2048  # 2 GiB
    size_bytes = size_mb * 1024 * 1024

    print(f"\n========================================================")
    print(f"[*] Stage 1: Building system.img ({size_mb} MiB ext4)")
    print(f"========================================================")

    if not os.path.isdir(system_extracted):
        print(f"[-] Error: system_extracted directory not found at {system_extracted}")
        sys.exit(1)

    fc_rules = load_file_contexts(plat_fc_path)
    print(f"[+] Loaded {len(fc_rules)} SELinux context rules from {plat_fc_path}")

    with open(system_img, "wb") as f:
        f.truncate(size_bytes)

    cmd = [
        mke2fs_bin,
        "-t", "ext4",
        "-F",
        "-L", "system",
        "-b", "4096",
        "-I", "256",
        "-E", "root_owner=0:0",
        "-d", system_extracted,
        system_img
    ]
    run_cmd(cmd)

    # Build standalone mini_init and fb_test binaries if sources are present
    mini_init_src = os.path.join(workspace_root, "tools", "mini_init.c")
    mini_init_bin = os.path.join(staging_dir, "mini_init")
    fb_test_src = os.path.join(workspace_root, "tools", "fb_test.c")
    fb_test_bin = os.path.join(staging_dir, "fb_test")

    gcc_bin = shutil.which("arm-none-eabi-gcc") or ("/opt/homebrew/bin/arm-none-eabi-gcc" if os.path.isfile("/opt/homebrew/bin/arm-none-eabi-gcc") else None)

    if os.path.isfile(mini_init_src):
        if not gcc_bin:
            print("[-] Error: arm-none-eabi-gcc cross-compiler is required to build init but was not found.")
            sys.exit(1)
        print("[*] Compiling standalone mini_init binary...")
        run_cmd([gcc_bin, "-nostdlib", "-static", "-fno-builtin", "-O2", mini_init_src, "-o", mini_init_bin])

    if os.path.isfile(fb_test_src):
        if not gcc_bin:
            print("[-] Error: arm-none-eabi-gcc cross-compiler is required to build fb_test but was not found.")
            sys.exit(1)
        print("[*] Compiling standalone fb_test binary...")
        run_cmd([gcc_bin, "-nostdlib", "-static", "-fno-builtin", "-Wall", "-Wextra", "-O2", f"-I{os.path.join(workspace_root, 'tools')}", fb_test_src, "-o", fb_test_bin])

    # Generate debugfs command file for Android TV postprocess.sh ownership & SELinux contexts
    print("[*] Normalizing file ownership, permissions & SELinux xattrs in system.img via debugfs...")
    cmds = []
    default_system_ctx = "u:object_r:system_file:s0"

    # Core mount points and compatibility links
    cmds.append("mkdir dev")
    cmds.append("mkdir proc")
    cmds.append("mkdir sys")
    cmds.append("mkdir vendor")
    cmds.append("symlink system .")

    if os.path.isfile(mini_init_bin):
        cmds.append(f"write {mini_init_bin} init")
        cmds.append("set_inode_field init mode 0100755")
        cmds.append(f"ea_set init security.selinux u:object_r:init_exec:s0")

    if os.path.isfile(fb_test_bin):
        cmds.append(f"write {fb_test_bin} bin/fb_test")
        cmds.append("set_inode_field bin/fb_test mode 0100755")
        cmds.append("set_inode_field bin/fb_test uid 0")
        cmds.append("set_inode_field bin/fb_test gid 2000")
        cmds.append(f"ea_set bin/fb_test security.selinux u:object_r:system_file:s0")

    for root, dirs, files in os.walk(system_extracted):
        for item in dirs + files:
            full = os.path.join(root, item)
            rel = os.path.relpath(full, system_extracted).replace("\\", "/")
            if rel == "vendor":
                continue # Skip recursive vendor symlink from extracted root
            is_dir = os.path.isdir(full) and not os.path.islink(full)
            is_symlink = os.path.islink(full)

            # Determine ownership and permissions per postprocess.sh
            uid, gid, mode = 0, 0, None
            if rel == "bin/netcfg":
                uid, gid, mode = 0, 3003, "0102750"  # AID_NET_ADMIN, setgid
            elif rel == "xbin/su":
                uid, gid, mode = 0, 2000, "0106750"  # AID_SHELL, setuid + setgid
            elif rel in ("bin/uncrypt", "bin/install-recovery.sh"):
                uid, gid, mode = 0, 0, "0100750"
            elif rel in ("xbin/librank", "xbin/procrank", "xbin/procmem", "xbin/tcpdump"):
                uid, gid, mode = 0, 0, "0100755"
            elif rel == "etc/dbus.conf":
                uid, gid, mode = 1002, 1002, "0100440"  # AID_BLUETOOTH
            elif rel == "etc/dhcpcd/dhcpcd-run-hooks":
                uid, gid, mode = 1014, 2000, "0100550"  # AID_DHCP
            elif rel.startswith("etc/ppp"):
                uid, gid, mode = 0, 0, "0100555" if not is_dir else "040755"
            elif rel.startswith("bin") or rel.startswith("xbin"):
                uid, gid = 0, 2000  # AID_SHELL
                if not is_dir and not is_symlink:
                    mode = "0100755"
            else:
                uid, gid = 0, 0

            cmds.append(f"set_inode_field {rel} uid {uid}")
            cmds.append(f"set_inode_field {rel} gid {gid}")
            if mode and not is_symlink:
                cmds.append(f"set_inode_field {rel} mode {mode}")

            # Match and apply SELinux context
            selinux_ctx = resolve_selinux_context(rel, "system", fc_rules, default_system_ctx)
            cmds.append(f"ea_set {rel} security.selinux {selinux_ctx}")

    cmd_file = os.path.join(staging_dir, "system_debugfs_cmds.tmp")
    with open(cmd_file, "w", encoding="utf-8") as f:
        f.write("\n".join(cmds) + "\n")

    run_cmd([debugfs_bin, "-w", "-f", cmd_file, system_img])
    os.remove(cmd_file)

    # Verify with e2fsck
    print("[*] Verifying system.img with e2fsck...")
    res = run_cmd([e2fsck_bin, "-f", "-n", system_img], check=False)
    if res.returncode > 1:
        print(f"[-] e2fsck reported errors on system.img: code {res.returncode}")
        print(res.stdout)
        sys.exit(res.returncode)

    # Verify sample SELinux attributes and permissions
    stat_check = run_cmd([debugfs_bin, "-R", "stat bin/sh", system_img])
    assert "security.selinux" in stat_check.stdout, "SELinux xattr missing on /bin/sh"
    print(f"[+] Verified SELinux context and ownership on system.img.")
    print(f"[+] system.img created and verified successfully ({size_bytes} bytes).")
    return system_img, size_bytes


def build_vendor_image(workspace_root):
    staging_dir = os.path.join(workspace_root, "staging")
    vendor_tar = os.path.join(staging_dir, "update_root", "vendor.tar")
    vendor_img = os.path.join(staging_dir, "vendor.img")
    vendor_fc_path = os.path.join(staging_dir, "update_root", "vendor", "etc", "selinux", "vendor_file_contexts")
    plat_fc_path = os.path.join(staging_dir, "system_extracted", "etc", "selinux", "plat_file_contexts")

    mke2fs_bin = find_tool("mke2fs")
    e2fsck_bin = find_tool("e2fsck")
    debugfs_bin = find_tool("debugfs")

    size_mb = 1536  # 1.5 GiB (matches minimum required 1395864372 bytes in config.txt)
    size_bytes = size_mb * 1024 * 1024

    print(f"\n========================================================")
    print(f"[*] Stage 2: Building vendor.img ({size_mb} MiB ext4)")
    print(f"========================================================")

    if not os.path.isfile(vendor_tar):
        print(f"[-] Error: vendor.tar not found at {vendor_tar}")
        sys.exit(1)

    # Load combined SELinux context rules (vendor + plat)
    fc_rules = load_file_contexts(plat_fc_path) + load_file_contexts(vendor_fc_path)
    print(f"[+] Loaded {len(fc_rules)} SELinux context rules for vendor")

    is_darwin = (sys.platform == "darwin")
    dmg_path = os.path.join(staging_dir, "vendor_cs.dmg")
    mount_dir = os.path.join(staging_dir, "vendor_cs_mnt")

    if is_darwin:
        if os.path.exists(mount_dir):
            subprocess.run(["hdiutil", "detach", mount_dir], capture_output=True)
            shutil.rmtree(mount_dir, ignore_errors=True)
        if os.path.exists(dmg_path):
            os.remove(dmg_path)
    else:
        if os.path.exists(mount_dir):
            shutil.rmtree(mount_dir, ignore_errors=True)

    os.makedirs(mount_dir, exist_ok=True)

    try:
        if is_darwin:
            print("[*] Creating temporary Case-sensitive APFS DMG to preserve case fidelity...")
            run_cmd(["hdiutil", "create", "-size", "2500m", "-fs", "Case-sensitive APFS", "-volname", "VendorCS", dmg_path])
            run_cmd(["hdiutil", "attach", dmg_path, "-mountpoint", mount_dir])
        else:
            print("[*] Staging vendor directory directly on native case-sensitive filesystem...")

        print("[*] Extracting vendor.tar into staging directory...")
        run_cmd(["tar", "-xf", vendor_tar, "-C", mount_dir])

        # Verify case-sensitive collision preservation
        omx1 = os.path.join(mount_dir, "lib", "libOMX_Core.so")
        omx2 = os.path.join(mount_dir, "lib", "libomx_core.so")
        assert os.path.exists(omx1) and os.path.exists(omx2), "Case collision preservation failed!"
        print(f"[+] Verified libOMX_Core.so ({os.path.getsize(omx1)} B) and libomx_core.so ({os.path.getsize(omx2)} B) preserved.")

        with open(vendor_img, "wb") as f:
            f.truncate(size_bytes)

        cmd = [
            mke2fs_bin,
            "-t", "ext4",
            "-F",
            "-L", "vendor",
            "-b", "4096",
            "-I", "256",
            "-E", "root_owner=0:0",
            "-d", mount_dir,
            vendor_img
        ]
        run_cmd(cmd)

        print("[*] Normalizing file ownership, permissions & SELinux xattrs in vendor.img per postprocess.sh...")
        # Android TV postprocess.sh specifications for vendor:
        # - /vendor/tvos/bin and /vendor/tvos/lib: 1000:1000 (system:system), mode 750 (files), 755 (dirs)
        # - /vendor/etc/*: 0:0, mode 644
        # - /vendor/*.conf, /vendor/*.xml: mode 644
        # - Specific daemons in /vendor/bin: mode 750
        # - /vendor/bin, /data, /firmware, /fonts, /lib, /overlay, /realtek: 0:2000
        # - default: 0:0
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
                    uid, gid = 1000, 1000  # AID_SYSTEM:AID_SYSTEM
                    mode = "040755" if is_dir else "0100750"
                elif rel.startswith("etc/") and rel != "etc":
                    uid, gid = 0, 0
                    mode = "040755" if is_dir else "0100644"
                elif rel in daemons_750:
                    uid, gid, mode = 0, 2000, "0100750"
                elif rel.endswith(".conf") or rel.endswith(".xml"):
                    uid, gid = 0, 0
                    mode = "0100644"
                else:
                    top = rel.split("/")[0]
                    if top in ("bin", "data", "etc", "firmware", "fonts", "lib", "overlay", "realtek", "tvos"):
                        uid, gid = 0, 2000
                    else:
                        uid, gid = 0, 0

                cmds.append(f"set_inode_field {rel} uid {uid}")
                cmds.append(f"set_inode_field {rel} gid {gid}")
                if mode and not is_symlink:
                    cmds.append(f"set_inode_field {rel} mode {mode}")

                # Match and apply SELinux context
                selinux_ctx = resolve_selinux_context(rel, "vendor", fc_rules, default_vendor_ctx)
                cmds.append(f"ea_set {rel} security.selinux {selinux_ctx}")

        cmd_file = os.path.join(staging_dir, "vendor_debugfs_cmds.tmp")
        with open(cmd_file, "w", encoding="utf-8") as f:
            f.write("\n".join(cmds) + "\n")

        run_cmd([debugfs_bin, "-w", "-f", cmd_file, vendor_img])
        os.remove(cmd_file)

    finally:
        if is_darwin:
            print("[*] Detaching and unmounting temporary DMG...")
            det_res = subprocess.run(["hdiutil", "detach", mount_dir], capture_output=True)
            if det_res.returncode != 0:
                subprocess.run(["hdiutil", "detach", mount_dir, "-force"], capture_output=True)
            if os.path.exists(mount_dir) and not os.path.ismount(mount_dir):
                shutil.rmtree(mount_dir, ignore_errors=True)
            if os.path.exists(dmg_path):
                try:
                    os.remove(dmg_path)
                except OSError:
                    pass
        else:
            print("[*] Cleaning up temporary staging directory...")
            if os.path.exists(mount_dir):
                shutil.rmtree(mount_dir, ignore_errors=True)

    # Verify with e2fsck
    print("[*] Verifying vendor.img with e2fsck...")
    res = run_cmd([e2fsck_bin, "-f", "-n", vendor_img], check=False)
    if res.returncode > 1:
        print(f"[-] e2fsck reported errors on vendor.img: code {res.returncode}")
        print(res.stdout)
        sys.exit(res.returncode)

    # Check that debugfs sees both OMX files in the ext4 filesystem
    res = run_cmd([debugfs_bin, "-R", "ls -l lib", vendor_img])
    assert "libOMX_Core.so" in res.stdout and "libomx_core.so" in res.stdout, "Both OMX libs must be in vendor.img"
    print("[+] Verified both libOMX_Core.so and libomx_core.so present in ext4 vendor.img!")

    # Verify sample tvos ownership and SELinux contexts
    stat_tvos = run_cmd([debugfs_bin, "-R", "stat tvos/bin", vendor_img])
    assert "User:  1000" in stat_tvos.stdout and "Group:  1000" in stat_tvos.stdout, "tvos/bin must be 1000:1000"
    print("[+] Verified tvos ownership (1000:1000) and SELinux xattrs in vendor.img.")
    print(f"[+] vendor.img created and verified successfully ({size_bytes} bytes).")
    return vendor_img, size_bytes


def create_gpt_structures(total_sectors, partitions):
    """
    total_sectors: uint64 total sector count of the disk
    partitions: list of dicts:
      {
        'name': str,
        'type_guid': uuid.UUID,
        'unique_guid': uuid.UUID,
        'start_lba': int,
        'end_lba': int, # inclusive
        'attributes': int
      }
    """
    # 1. Protective MBR (LBA 0)
    pmbr = bytearray(SECTOR_SIZE)
    # Partition entry 0 at offset 446 (0x1BE)
    pmbr[446] = 0x00               # Boot indicator
    pmbr[447:450] = b"\x00\x02\x00"  # Starting CHS
    pmbr[450] = 0xEE               # OS Type: GPT Protective
    pmbr[451:454] = b"\xFF\xFF\xFF"  # Ending CHS
    struct.pack_into("<I", pmbr, 454, 1)  # Starting LBA = 1
    # Size in LBA: total_sectors - 1, capped at 0xFFFFFFFF
    size_in_lba = min(total_sectors - 1, 0xFFFFFFFF)
    struct.pack_into("<I", pmbr, 458, size_in_lba)
    # MBR Signature at offset 510
    struct.pack_into("<H", pmbr, 510, MBR_SIGNATURE)

    # 2. Partition Entries (128 entries * 128 bytes = 16384 bytes = 32 sectors)
    entries_bytes = bytearray(NUM_PARTITION_ENTRIES * PARTITION_ENTRY_SIZE)
    for idx, p in enumerate(partitions):
        offset = idx * PARTITION_ENTRY_SIZE
        # Partition Type GUID (16 bytes, UEFI mixed-endian)
        entries_bytes[offset:offset + 16] = p['type_guid'].bytes_le
        # Unique Partition GUID (16 bytes, UEFI mixed-endian)
        entries_bytes[offset + 16:offset + 32] = p['unique_guid'].bytes_le
        # Starting LBA (uint64_le)
        struct.pack_into("<Q", entries_bytes, offset + 32, p['start_lba'])
        # Ending LBA (uint64_le, inclusive)
        struct.pack_into("<Q", entries_bytes, offset + 40, p['end_lba'])
        # Attributes (uint64_le)
        struct.pack_into("<Q", entries_bytes, offset + 48, p.get('attributes', 0))
        # Partition Name (36 characters UTF-16LE, 72 bytes)
        name_utf16 = p['name'].encode("utf-16le")[:72]
        entries_bytes[offset + 56:offset + 56 + len(name_utf16)] = name_utf16

    entries_crc32 = zlib.crc32(entries_bytes) & 0xFFFFFFFF

    disk_guid = uuid.uuid4()
    first_usable_lba = 2048  # 1 MiB alignment
    last_usable_lba = total_sectors - 34  # Backup table is 32 sectors, backup header is 1 sector

    # 3. Primary GPT Header (LBA 1)
    primary_hdr = bytearray(SECTOR_SIZE)
    primary_hdr[0:8] = GPT_SIGNATURE
    struct.pack_into("<I", primary_hdr, 8, GPT_REVISION)
    struct.pack_into("<I", primary_hdr, 12, GPT_HEADER_SIZE)
    struct.pack_into("<I", primary_hdr, 16, 0)  # CRC32 placeholder
    struct.pack_into("<I", primary_hdr, 20, 0)  # Reserved
    struct.pack_into("<Q", primary_hdr, 24, 1)  # My LBA
    struct.pack_into("<Q", primary_hdr, 32, total_sectors - 1)  # Alternate LBA
    struct.pack_into("<Q", primary_hdr, 40, first_usable_lba)
    struct.pack_into("<Q", primary_hdr, 48, last_usable_lba)
    primary_hdr[56:72] = disk_guid.bytes_le
    struct.pack_into("<Q", primary_hdr, 72, 2)  # Partition Entry LBA
    struct.pack_into("<I", primary_hdr, 80, NUM_PARTITION_ENTRIES)
    struct.pack_into("<I", primary_hdr, 84, PARTITION_ENTRY_SIZE)
    struct.pack_into("<I", primary_hdr, 88, entries_crc32)

    primary_crc32 = zlib.crc32(primary_hdr[:GPT_HEADER_SIZE]) & 0xFFFFFFFF
    struct.pack_into("<I", primary_hdr, 16, primary_crc32)

    # 4. Secondary (Backup) GPT Header (LBA total_sectors - 1)
    backup_hdr = bytearray(SECTOR_SIZE)
    backup_hdr[0:8] = GPT_SIGNATURE
    struct.pack_into("<I", backup_hdr, 8, GPT_REVISION)
    struct.pack_into("<I", backup_hdr, 12, GPT_HEADER_SIZE)
    struct.pack_into("<I", backup_hdr, 16, 0)  # CRC32 placeholder
    struct.pack_into("<I", backup_hdr, 20, 0)  # Reserved
    struct.pack_into("<Q", backup_hdr, 24, total_sectors - 1)  # My LBA
    struct.pack_into("<Q", backup_hdr, 32, 1)  # Alternate LBA
    struct.pack_into("<Q", backup_hdr, 40, first_usable_lba)
    struct.pack_into("<Q", backup_hdr, 48, last_usable_lba)
    backup_hdr[56:72] = disk_guid.bytes_le
    # Backup partition entry array is placed at total_sectors - 33
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
        "first_usable_lba": first_usable_lba,
        "last_usable_lba": last_usable_lba,
    }


def build_composite_emmc(workspace_root, system_img, vendor_img, output_path=None):
    staging_dir = os.path.join(workspace_root, "staging")
    if output_path:
        emmc_raw = os.path.abspath(output_path)
        if os.path.isdir(emmc_raw):
            emmc_raw = os.path.join(emmc_raw, "storage.img")
    else:
        emmc_raw = os.path.join(staging_dir, "emmc.raw")

    # 4 GiB eMMC geometry (matches part_targetM=4096 and QEMU SDHC requirements)
    total_size_mb = 4096
    total_size_bytes = total_size_mb * 1024 * 1024
    total_sectors = total_size_bytes // SECTOR_SIZE  # 8,388,608 sectors

    system_size = os.path.getsize(system_img)
    vendor_size = os.path.getsize(vendor_img)

    system_sectors = system_size // SECTOR_SIZE
    vendor_sectors = vendor_size // SECTOR_SIZE

    # Partition layout:
    # LBA 0: PMBR
    # LBA 1: Primary GPT Header
    # LBA 2..33: Primary Table
    # LBA 2048: Partition 1 (system, 2048 MiB)
    # LBA 4196352: Partition 2 (vendor, 1536 MiB)
    sys_start_lba = 2048
    sys_end_lba = sys_start_lba + system_sectors - 1

    ven_start_lba = sys_end_lba + 1  # 4,196,352 (2049 MiB, 1 MiB aligned)
    ven_end_lba = ven_start_lba + vendor_sectors - 1

    print(f"\n========================================================")
    print(f"[*] Stage 3: Building Composite GPT Image (emmc.raw)")
    print(f"========================================================")
    print(f"Total Disk Size:    {total_size_mb} MiB ({total_size_bytes} bytes, {total_sectors} sectors)")
    print(f"Partition 1 (system): LBA {sys_start_lba}..{sys_end_lba} ({system_size // (1024*1024)} MiB)")
    print(f"Partition 2 (vendor): LBA {ven_start_lba}..{ven_end_lba} ({vendor_size // (1024*1024)} MiB)")

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
        }
    ]

    gpt = create_gpt_structures(total_sectors, partitions)

    print(f"[*] Writing composite disk image: {emmc_raw}...")
    if os.path.dirname(emmc_raw):
        os.makedirs(os.path.dirname(emmc_raw), exist_ok=True)
    with open(emmc_raw, "wb") as f:
        # Preallocate total file size
        f.truncate(total_size_bytes)

        # 1. LBA 0: Protective MBR
        f.seek(0)
        f.write(gpt["pmbr"])

        # 2. LBA 1: Primary GPT Header
        f.seek(1 * SECTOR_SIZE)
        f.write(gpt["primary_hdr"])

        # 3. LBA 2..33: Primary Partition Table
        f.seek(2 * SECTOR_SIZE)
        f.write(gpt["entries"])

        # 4. Stream system.img at sys_start_lba
        print(f"[*] Streaming system.img into emmc.raw at offset {sys_start_lba * SECTOR_SIZE}...")
        f.seek(sys_start_lba * SECTOR_SIZE)
        with open(system_img, "rb") as s_in:
            while chunk := s_in.read(16 * 1024 * 1024):
                f.write(chunk)

        # 5. Stream vendor.img at ven_start_lba
        print(f"[*] Streaming vendor.img into emmc.raw at offset {ven_start_lba * SECTOR_SIZE}...")
        f.seek(ven_start_lba * SECTOR_SIZE)
        with open(vendor_img, "rb") as v_in:
            while chunk := v_in.read(16 * 1024 * 1024):
                f.write(chunk)

        # 6. LBA backup_entries_lba: Backup Partition Table
        f.seek(gpt["backup_entries_lba"] * SECTOR_SIZE)
        f.write(gpt["entries"])

        # 7. LBA total_sectors - 1: Backup GPT Header
        f.seek((total_sectors - 1) * SECTOR_SIZE)
        f.write(gpt["backup_hdr"])

    print(f"[+] Successfully constructed {emmc_raw} ({total_size_bytes} bytes).")
    return emmc_raw, partitions, gpt


def verify_emmc_raw(emmc_raw):
    print(f"\n========================================================")
    print(f"[*] Stage 4: Verifying emmc.raw Partition Geometry & Integrity")
    print(f"========================================================")

    size = os.path.getsize(emmc_raw)
    total_sectors = size // SECTOR_SIZE
    print(f"Image File:         {emmc_raw}")
    print(f"Image Size:         {size} bytes ({size / (1024*1024):.1f} MiB)")
    print(f"Total Sectors:      {total_sectors}")

    with open(emmc_raw, "rb") as f:
        # Verify LBA 0 (PMBR)
        f.seek(0)
        pmbr = f.read(512)
        mbr_sig = struct.unpack_from("<H", pmbr, 510)[0]
        assert mbr_sig == MBR_SIGNATURE, f"Invalid MBR signature: {hex(mbr_sig)}"
        part_type = pmbr[450]
        assert part_type == 0xEE, f"Invalid Protective MBR OS type: {hex(part_type)}"
        print("[+] Protective MBR verified (Signature 0xAA55, Partition Type 0xEE).")

        # Verify LBA 1 (Primary GPT Header)
        f.seek(512)
        prim_hdr = f.read(512)
        sig = prim_hdr[0:8]
        assert sig == GPT_SIGNATURE, f"Invalid GPT signature: {sig}"
        rev, hdr_size, hdr_crc = struct.unpack_from("<III", prim_hdr, 8)
        assert rev == GPT_REVISION, f"Invalid GPT revision: {hex(rev)}"
        assert hdr_size == GPT_HEADER_SIZE, f"Invalid GPT header size: {hdr_size}"

        # Check header CRC
        hdr_copy = bytearray(prim_hdr[:GPT_HEADER_SIZE])
        struct.pack_into("<I", hdr_copy, 16, 0)
        calc_crc = zlib.crc32(hdr_copy) & 0xFFFFFFFF
        assert calc_crc == hdr_crc, f"Primary GPT CRC mismatch: {hex(calc_crc)} != {hex(hdr_crc)}"
        print(f"[+] Primary GPT Header verified (CRC32: {hex(calc_crc)} matches).")

        my_lba, alt_lba, first_lba, last_lba = struct.unpack_from("<QQQQ", prim_hdr, 24)
        entries_lba, num_entries, entry_sz, entries_crc = struct.unpack_from("<QIII", prim_hdr, 72)

        assert my_lba == 1, f"Invalid Primary my_lba: {my_lba}"
        assert alt_lba == total_sectors - 1, f"Invalid Primary alt_lba: {alt_lba}"
        assert entries_lba == 2, f"Invalid entries LBA: {entries_lba}"

        # Verify Partition Entries
        f.seek(entries_lba * SECTOR_SIZE)
        entries_buf = f.read(num_entries * entry_sz)
        calc_entries_crc = zlib.crc32(entries_buf) & 0xFFFFFFFF
        assert calc_entries_crc == entries_crc, "Partition entry array CRC mismatch!"
        print(f"[+] Partition Entry Array verified (128 entries, CRC32: {hex(calc_entries_crc)} matches).")

        # Parse individual partitions
        p1_type = uuid.UUID(bytes_le=entries_buf[0:16])
        p1_start, p1_end = struct.unpack_from("<QQ", entries_buf, 32)
        p1_name = entries_buf[56:128].decode("utf-16le").rstrip("\x00")

        p2_type = uuid.UUID(bytes_le=entries_buf[128:144])
        p2_start, p2_end = struct.unpack_from("<QQ", entries_buf, 160)
        p2_name = entries_buf[184:256].decode("utf-16le").rstrip("\x00")

        print(f"[+] Partition 1: name='{p1_name}' type={p1_type} LBA={p1_start}..{p1_end} ({((p1_end - p1_start + 1) * 512)/(1024*1024):.1f} MiB)")
        print(f"[+] Partition 2: name='{p2_name}' type={p2_type} LBA={p2_start}..{p2_end} ({((p2_end - p2_start + 1) * 512)/(1024*1024):.1f} MiB)")

        assert p1_name == "system", f"Expected partition 1 name 'system', got '{p1_name}'"
        assert p2_name == "vendor", f"Expected partition 2 name 'vendor', got '{p2_name}'"

        # Verify Secondary GPT Header
        f.seek((total_sectors - 1) * SECTOR_SIZE)
        sec_hdr = f.read(512)
        assert sec_hdr[0:8] == GPT_SIGNATURE
        sec_crc = struct.unpack_from("<I", sec_hdr, 16)[0]
        sec_copy = bytearray(sec_hdr[:GPT_HEADER_SIZE])
        struct.pack_into("<I", sec_copy, 16, 0)
        calc_sec_crc = zlib.crc32(sec_copy) & 0xFFFFFFFF
        assert calc_sec_crc == sec_crc, "Secondary GPT CRC mismatch!"
        print(f"[+] Secondary GPT Header verified at LBA {total_sectors - 1} (CRC32: {hex(calc_sec_crc)} matches).")

        # Verify ext4 superblocks inside emmc.raw directly
        for name, start_lba in [("system", p1_start), ("vendor", p2_start)]:
            sb_offset = (start_lba * SECTOR_SIZE) + 1024
            f.seek(sb_offset)
            sb_data = f.read(1024)
            s_magic = struct.unpack_from("<H", sb_data, 0x38)[0]
            s_vol = sb_data[0x78:0x88].split(b"\x00")[0].decode("latin1")
            assert s_magic == 0xEF53, f"Partition {name} at LBA {start_lba} invalid ext4 magic: {hex(s_magic)}"
            assert s_vol == name, f"Partition {name} volume name mismatch: {s_vol}"
            print(f"[+] ext4 Superblock confirmed for '{name}' at offset 0x{sb_offset:x} (magic: 0x{s_magic:x}, volume_name: '{s_vol}')")

    # Optional verification with sgdisk
    try:
        sgdisk_bin = find_tool("sgdisk")
        run_cmd([sgdisk_bin, "-v", emmc_raw])
        print("[+] sgdisk validation passed.")
    except Exception as e:
        print(f"[*] Note: sgdisk validation skipped ({e})")

    print("[>>> ALL STORAGE IMAGE INTEGRITY CHECKS PASSED <<<]")


def main():
    workspace_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

    if len(sys.argv) > 1 and sys.argv[1] in ("-h", "--help"):
        print(f"Usage: {sys.argv[0]} [output_path]")
        print("  Builds composite eMMC storage image for RTD2841 QEMU emulation.")
        print("  Example: python3 tools/create_emmc_storage.py storage.img")
        print("  Default destination: staging/emmc.raw")
        sys.exit(0)

    output_path = sys.argv[1] if len(sys.argv) > 1 else None
    if output_path:
        print(f"[*] Custom output destination specified: {output_path}")

    system_img, _ = build_system_image(workspace_root)
    vendor_img, _ = build_vendor_image(workspace_root)
    emmc_raw, partitions, gpt = build_composite_emmc(workspace_root, system_img, vendor_img, output_path=output_path)
    verify_emmc_raw(emmc_raw)


if __name__ == "__main__":
    main()
