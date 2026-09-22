package auth

import (
	"crypto/rand"
	"encoding/base64"
	"fmt"
)

// GenerateToken creates a cryptographically secure random bearer token.
// The raw token is returned to the agent exactly once at registration
// time and only its SHA-256 hash is persisted server-side.
func GenerateToken() (string, error) {
	buf := make([]byte, 48)
	if _, err := rand.Read(buf); err != nil {
		return "", fmt.Errorf("generate token: %w", err)
	}
	return base64.RawURLEncoding.EncodeToString(buf), nil
}
