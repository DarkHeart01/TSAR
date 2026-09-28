package tests

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"endpoint-management-server/internal/c2"
	"endpoint-management-server/internal/models"
)

func sampleManifest() models.PayloadManifest {
	return models.PayloadManifest{
		TotalChunks: 3,
		TotalSize:   567,
		SHA256:      "abc123def456abc123def456abc123def456abc123def456abc123def456abcd",
		Version:     "1",
		UploadedAt:  time.Now().UTC().Format(time.RFC3339),
	}
}

func TestWriteZoneFile_CreatesFile(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "jocky.online.zone")

	chunks := []string{"chunk0data", "chunk1data", "chunk2data"}
	m := sampleManifest()

	if err := c2.WriteZoneFile(path, "1.2.3.4", chunks, m); err != nil {
		t.Fatalf("WriteZoneFile: %v", err)
	}

	if _, err := os.Stat(path); err != nil {
		t.Fatalf("zone file not created: %v", err)
	}
}

func TestWriteZoneFile_ContainsSOAAndNS(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "jocky.online.zone")

	if err := c2.WriteZoneFile(path, "1.2.3.4", []string{"x"}, sampleManifest()); err != nil {
		t.Fatal(err)
	}

	content, _ := os.ReadFile(path)
	zone := string(content)

	checks := []string{"SOA", "NS", "$ORIGIN", "$TTL", "1.2.3.4"}
	for _, want := range checks {
		if !strings.Contains(zone, want) {
			t.Errorf("zone file missing %q", want)
		}
	}
}

func TestWriteZoneFile_ContainsChunkTXTRecords(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "jocky.online.zone")

	chunks := []string{"aaa", "bbb", "ccc"}
	if err := c2.WriteZoneFile(path, "10.0.0.1", chunks, sampleManifest()); err != nil {
		t.Fatal(err)
	}

	content, _ := os.ReadFile(path)
	zone := string(content)

	for i := range chunks {
		record := strings.Contains(zone, "chunk-"+strings.Repeat("0", 0)+string(rune('0'+i))+".c2")
		_ = record // just check naming pattern exists
		if !strings.Contains(zone, "chunk-") {
			t.Error("zone file has no chunk- TXT records")
			break
		}
	}

	// Verify each chunk label appears.
	for i := 0; i < len(chunks); i++ {
		label := "chunk-" + string(rune('0'+i)) + ".c2"
		if !strings.Contains(zone, label) {
			t.Errorf("zone file missing label %q", label)
		}
	}
}

func TestWriteZoneFile_ContainsManifestTXT(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "jocky.online.zone")

	if err := c2.WriteZoneFile(path, "1.1.1.1", []string{"data"}, sampleManifest()); err != nil {
		t.Fatal(err)
	}

	content, _ := os.ReadFile(path)
	zone := string(content)

	if !strings.Contains(zone, "manifest.c2") {
		t.Error("zone file missing manifest.c2 TXT record")
	}
	if !strings.Contains(zone, "total_chunks") {
		t.Error("zone file manifest TXT doesn't contain total_chunks field")
	}
}

func TestWriteZoneFile_IsAtomic(t *testing.T) {
	// Verifies no .tmp file is left behind after a successful write.
	dir := t.TempDir()
	path := filepath.Join(dir, "jocky.online.zone")

	if err := c2.WriteZoneFile(path, "1.2.3.4", []string{"x"}, sampleManifest()); err != nil {
		t.Fatal(err)
	}

	entries, _ := os.ReadDir(dir)
	for _, e := range entries {
		if strings.HasSuffix(e.Name(), ".tmp") {
			t.Errorf("stale tmp file left behind: %s", e.Name())
		}
	}
}

func TestWriteZoneFile_LongChunkSplitAt255(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "jocky.online.zone")

	// 600-char chunk must be split into quoted 255-byte segments in the zone.
	longChunk := strings.Repeat("A", 600)
	if err := c2.WriteZoneFile(path, "1.2.3.4", []string{longChunk}, sampleManifest()); err != nil {
		t.Fatal(err)
	}

	content, _ := os.ReadFile(path)
	zone := string(content)

	// There should be multiple quoted strings for the single long chunk.
	quoteCount := strings.Count(zone, `"`)
	// At least 2 pairs for the 600-char chunk (ceil(600/255) = 3 segments = 6 quotes).
	// Plus quotes from the manifest. So quoteCount should be >= 6.
	if quoteCount < 6 {
		t.Errorf("expected at least 6 quote chars for split 600-char chunk, got %d", quoteCount)
	}
}
