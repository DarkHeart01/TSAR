#!/usr/bin/env python3
"""
pe_header_spoofer.py
JOCKY PE Header Post-Processor v2

Changes from v1:
- Removed section renaming (custom names trigger detections)
- Added timestamp faking
- Added Rich Header grafting (Strategy C — spoof, not zero)

Usage: python pe_header_spoofer.py input.exe output.exe [rich_template.bin]
"""

import struct
import sys
import os
import random
from datetime import datetime, timezone

def read_pe(path):
    with open(path, 'rb') as f:
        data = bytearray(f.read())
    if data[0:2] != b'MZ':
        print(f"[-] Not a valid PE file: {path}")
        sys.exit(1)
    return data

def write_pe(path, data):
    with open(path, 'wb') as f:
        f.write(data)

def get_lfanew(data):
    return struct.unpack_from('<I', data, 0x3C)[0]

def remove_rich_header(data):
    """Zero Rich Header — used when no template available"""
    rich_marker = b'Rich'
    pos = data.find(rich_marker)
    if pos == -1:
        print("[*] No Rich Header found")
        return data
    e_lfanew = get_lfanew(data)
    for i in range(0x40, e_lfanew):
        data[i] = 0x00
    print(f"[+] Rich Header zeroed (0x40 to 0x{e_lfanew:X})")
    return data

def graft_rich_header(data, template_path):
    """
    Strategy C: Graft legitimate MSVC Rich Header.
    Binary appears compiled with Visual Studio.
    Far less suspicious than a missing Rich Header.
    """
    if not template_path or not os.path.exists(template_path):
        print("[!] No Rich Header template — zeroing instead")
        return remove_rich_header(data)
    
    with open(template_path, 'rb') as f:
        template = bytearray(f.read())
    
    e_lfanew = get_lfanew(data)
    gap_start = 0x40
    gap_size  = e_lfanew - gap_start
    
    if len(template) > gap_size:
        # Trim template to fit
        template = template[:gap_size]
    
    # Zero the gap
    for i in range(gap_start, e_lfanew):
        data[i] = 0
    
    # Write template
    data[gap_start:gap_start + len(template)] = template
    
    print(f"[+] Rich Header grafted: {len(template)} bytes from MSVC template")
    return data

def fake_timestamp(data):
    """
    Fake COFF timestamp to look like established software.
    Fresh timestamps reveal the binary was just built.
    """
    e_lfanew = get_lfanew(data)
    timestamp_offset = e_lfanew + 4 + 4
    
    # Random date between 2020 and 2023
    start = 1577836800  # 2020-01-01
    end   = 1672531200  # 2023-01-01
    fake_ts = random.randint(start, end)
    
    struct.pack_into('<I', data, timestamp_offset, fake_ts)
    
    dt = datetime.fromtimestamp(fake_ts, tz=timezone.utc)
    print(f"[+] Timestamp faked: {dt.strftime('%Y-%m-%d %H:%M:%S UTC')}")
    return data

def zero_debug_directory(data):
    """Zero debug directory entry"""
    e_lfanew = get_lfanew(data)
    opt_header_offset = e_lfanew + 4 + 20
    magic = struct.unpack_from('<H', data, opt_header_offset)[0]

    if magic == 0x10B:
        data_dir_offset = opt_header_offset + 96
    elif magic == 0x20B:
        data_dir_offset = opt_header_offset + 112
    else:
        return data

    debug_dir_offset = data_dir_offset + (6 * 8)
    if debug_dir_offset + 8 <= len(data):
        rva  = struct.unpack_from('<I', data, debug_dir_offset)[0]
        size = struct.unpack_from('<I', data, debug_dir_offset + 4)[0]
        if rva != 0 or size != 0:
            struct.pack_into('<I', data, debug_dir_offset, 0)
            struct.pack_into('<I', data, debug_dir_offset + 4, 0)
            print(f"[+] Debug directory zeroed")
        else:
            print("[*] Debug directory already empty")
    return data

def calculate_checksum(data):
    checksum = 0
    data_len = len(data)
    for i in range(0, data_len - 1, 2):
        word = struct.unpack_from('<H', data, i)[0]
        checksum += word
        if checksum > 0xFFFF:
            checksum = (checksum & 0xFFFF) + 1
    if data_len % 2:
        checksum += data[-1]
        if checksum > 0xFFFF:
            checksum = (checksum & 0xFFFF) + 1
    checksum += data_len
    return checksum & 0xFFFFFFFF

def fix_checksum(data):
    e_lfanew   = get_lfanew(data)
    opt_offset = e_lfanew + 4 + 20
    cs_offset  = opt_offset + 0x40

    if cs_offset + 4 > len(data):
        return data

    struct.pack_into('<I', data, cs_offset, 0)
    new_cs = calculate_checksum(data)
    struct.pack_into('<I', data, cs_offset, new_cs)
    print(f"[+] PE checksum updated: 0x{new_cs:08X}")
    return data

def print_pe_info(data, label):
    e_lfanew     = get_lfanew(data)
    opt_offset   = e_lfanew + 4 + 20
    magic        = struct.unpack_from('<H', data, opt_offset)[0]
    arch         = "PE32+" if magic == 0x20B else "PE32"
    num_sections = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
    ts_offset    = e_lfanew + 4 + 4
    ts           = struct.unpack_from('<I', data, ts_offset)[0]
    dt           = datetime.fromtimestamp(ts, tz=timezone.utc)

    print(f"\n  {label}:")
    print(f"    Architecture: {arch}")
    print(f"    File size:    {len(data):,} bytes")
    print(f"    Timestamp:    {dt.strftime('%Y-%m-%d %H:%M:%S UTC')}")
    print(f"    Sections:     {num_sections}")

def spoof(input_path, output_path, template_path=None):
    print(f"[*] JOCKY PE Header Spoofer v2")
    print(f"[*] Input:    {input_path}")
    print(f"[*] Output:   {output_path}")
    if template_path:
        print(f"[*] Template: {template_path}")
    print()

    data = read_pe(input_path)
    print_pe_info(data, "Before")
    print()

    # Strategy C: graft MSVC Rich Header (not zero)
    data = graft_rich_header(data, template_path)
    # Remove debug info
    data = zero_debug_directory(data)
    # Fake the timestamp
    data = fake_timestamp(data)
    # Fix checksum last
    data = fix_checksum(data)

    print()
    print_pe_info(data, "After")
    print()

    write_pe(output_path, data)
    print(f"[+] Output written: {output_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: pe_header_spoofer.py input.exe output.exe [rich_template.bin]")
        sys.exit(1)
    template = sys.argv[3] if len(sys.argv) > 3 else None
    spoof(sys.argv[1], sys.argv[2], template)