"""AES-256-GCM encryption for uploaded build sources.

The key is derived from the shared connect token via HKDF-SHA256 - client and server
already both know the token (it's how `connect` authenticates), so no separate secret
needs to be generated or distributed. AES-GCM is authenticated encryption: a tampered
or corrupted ciphertext fails to decrypt rather than silently returning garbage.
"""
import os

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF

_SALT = b"jocky-file-encryption-salt-v1"
_INFO = b"jocky-build-upload"
_NONCE_SIZE = 12


def derive_key(token: str) -> bytes:
    hkdf = HKDF(algorithm=hashes.SHA256(), length=32, salt=_SALT, info=_INFO)
    return hkdf.derive(token.encode("utf-8"))


def encrypt(key: bytes, plaintext: bytes, associated_data: bytes) -> bytes:
    nonce = os.urandom(_NONCE_SIZE)
    ciphertext = AESGCM(key).encrypt(nonce, plaintext, associated_data)
    return nonce + ciphertext


def decrypt(key: bytes, blob: bytes, associated_data: bytes) -> bytes:
    if len(blob) < _NONCE_SIZE:
        raise ValueError("encrypted payload too short")
    nonce, ciphertext = blob[:_NONCE_SIZE], blob[_NONCE_SIZE:]
    return AESGCM(key).decrypt(nonce, ciphertext, associated_data)
