#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import struct
import sys

HEADER_SIZE = 84
TIMG_HEADER_TOTAL = 284


def compute_md5(file_path):
    h = hashlib.md5()
    with open(file_path, "rb") as f:
        while chunk := f.read(1024 * 1024):
            h.update(chunk)
    return h.digest()


def create_pimg_header(name, device_name, ptype, payload_size, payload_md5):
    """Build 84-byte PIMG chunk header"""
    header = bytearray(HEADER_SIZE)
    header[0:4] = b'PIMG'
    struct.pack_into('<I', header, 4, payload_size)
    header[8:24] = payload_md5
    
    # 16-byte name
    name_bytes = name.encode('ascii')[:15]
    header[24:24 + len(name_bytes)] = name_bytes
    
    # 24-byte device path
    dev_bytes = device_name.encode('ascii')[:23]
    header[40:40 + len(dev_bytes)] = dev_bytes
    
    # 4-byte type
    type_bytes = ptype.encode('ascii')[:4]
    header[64:64 + len(type_bytes)] = type_bytes
    
    return bytes(header)


def repack(metadata_file, partitions_dir, output_file):
    with open(metadata_file, "r") as f:
        meta = json.load(f)
        
    partitions = meta.get("partitions", [])
    if not partitions:
        print("[-] Error: No partitions defined in metadata JSON.")
        sys.exit(1)

    print(f"[*] Repacking {len(partitions)} partitions into '{output_file}'...")
    
    # Build payload file in memory or temporary buffer
    temp_payload = output_file + ".payload.tmp"
    with open(temp_payload, "wb") as out_p:
        for part in partitions:
            pname = part["name"]
            part_file = os.path.join(partitions_dir, f"{pname}.bin")
            if not os.path.exists(part_file):
                print(f"[-] Missing partition binary: {part_file}")
                sys.exit(1)
                
            psize = os.path.getsize(part_file)
            pmd5 = compute_md5(part_file)
            
            p_hdr = create_pimg_header(
                name=pname,
                device_name=part.get("device_name", ""),
                ptype=part.get("type", "none"),
                payload_size=psize,
                payload_md5=pmd5
            )
            out_p.write(p_hdr)
            
            with open(part_file, "rb") as pf:
                while chunk := pf.read(1024 * 1024):
                    out_p.write(chunk)
            print(f"  [+] Packaged: {pname} ({psize} bytes)")

    payload_size = os.path.getsize(temp_payload)
    total_file_size = TIMG_HEADER_TOTAL + payload_size
    payload_md5 = compute_md5(temp_payload)
    
    # Build master TIMG header
    timg_header = bytearray(TIMG_HEADER_TOTAL)
    timg_header[0:4] = b'TIMG'
    struct.pack_into('<I', timg_header, 4, total_file_size)
    timg_header[8:24] = payload_md5
    
    with open(output_file, "wb") as final_out:
        final_out.write(timg_header)
        with open(temp_payload, "rb") as tp:
            while chunk := tp.read(1024 * 1024):
                final_out.write(chunk)
                
    os.remove(temp_payload)
    print(f"[✓] Successfully generated '{output_file}' (Total size: {total_file_size} bytes)")


def main():
    parser = argparse.ArgumentParser(description="Repack TCL Update.img from extracted partitions")
    parser.add_argument("--meta", required=True, help="Metadata JSON generated during scan/extract")
    parser.add_argument("--dir", required=True, help="Directory containing partition .bin files")
    parser.add_argument("--out", required=True, help="Output target Update.img")
    args = parser.parse_args()

    repack(args.meta, args.dir, args.out)


if __name__ == "__main__":
    main()
