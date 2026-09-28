package tests

import (
	"strings"
	"testing"
	"time"

	"github.com/google/uuid"

	"endpoint-management-server/internal/auth"
)

// ─── GenerateToken ───────────────────────────────────────────────────────────

func TestGenerateToken_NotEmpty(t *testing.T) {
	tok, err := auth.GenerateToken()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if tok == "" {
		t.Error("token is empty")
	}
}

func TestGenerateToken_Unique(t *testing.T) {
	seen := make(map[string]bool, 100)
	for i := 0; i < 100; i++ {
		tok, err := auth.GenerateToken()
		if err != nil {
			t.Fatalf("iteration %d: %v", i, err)
		}
		if seen[tok] {
			t.Fatalf("duplicate token on iteration %d", i)
		}
		seen[tok] = true
	}
}

func TestGenerateToken_URLSafeBase64(t *testing.T) {
	tok, _ := auth.GenerateToken()
	// RawURLEncoding uses A-Z a-z 0-9 - _ with no padding.
	for _, ch := range tok {
		if !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
			(ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
			t.Errorf("token contains non-URL-safe char: %q", string(ch))
		}
	}
}

func TestGenerateToken_MinLength(t *testing.T) {
	tok, _ := auth.GenerateToken()
	// 48 raw bytes → base64url ≈ 64 chars.
	if len(tok) < 60 {
		t.Errorf("token too short: %d chars", len(tok))
	}
}

// ─── JWT round-trip ──────────────────────────────────────────────────────────

const testJWTSecret = "this-is-a-test-secret-that-is-at-least-32-chars!"

func TestIssueAndValidateJWT_RoundTrip(t *testing.T) {
	opID := uuid.New()
	token, expiry, err := auth.IssueOperatorJWT(opID, "alice", testJWTSecret)
	if err != nil {
		t.Fatalf("IssueOperatorJWT: %v", err)
	}

	if token == "" {
		t.Error("issued token is empty")
	}
	if expiry.Before(time.Now()) {
		t.Error("expiry is in the past")
	}

	claims, err := auth.ValidateOperatorJWT(token, testJWTSecret)
	if err != nil {
		t.Fatalf("ValidateOperatorJWT: %v", err)
	}
	if claims.OperatorID != opID {
		t.Errorf("OperatorID mismatch: want %v, got %v", opID, claims.OperatorID)
	}
	if claims.Username != "alice" {
		t.Errorf("Username mismatch: want alice, got %q", claims.Username)
	}
}

func TestValidateJWT_WrongSecretFails(t *testing.T) {
	opID := uuid.New()
	token, _, err := auth.IssueOperatorJWT(opID, "bob", testJWTSecret)
	if err != nil {
		t.Fatal(err)
	}

	_, err = auth.ValidateOperatorJWT(token, "wrong-secret-wrong-secret-wrong!!")
	if err == nil {
		t.Error("expected validation error for wrong secret, got nil")
	}
}

func TestValidateJWT_TamperedTokenFails(t *testing.T) {
	opID := uuid.New()
	token, _, err := auth.IssueOperatorJWT(opID, "eve", testJWTSecret)
	if err != nil {
		t.Fatal(err)
	}

	// Flip the last character of the signature.
	parts := strings.Split(token, ".")
	if len(parts) != 3 {
		t.Fatalf("unexpected JWT structure: %d parts", len(parts))
	}
	sig := []byte(parts[2])
	sig[len(sig)-1] ^= 0x01
	parts[2] = string(sig)
	tampered := strings.Join(parts, ".")

	_, err = auth.ValidateOperatorJWT(tampered, testJWTSecret)
	if err == nil {
		t.Error("expected validation error for tampered signature, got nil")
	}
}

func TestValidateJWT_MalformedTokenFails(t *testing.T) {
	_, err := auth.ValidateOperatorJWT("not.a.jwt", testJWTSecret)
	if err == nil {
		t.Error("expected error for malformed token")
	}
}

func TestValidateJWT_EmptyTokenFails(t *testing.T) {
	_, err := auth.ValidateOperatorJWT("", testJWTSecret)
	if err == nil {
		t.Error("expected error for empty token")
	}
}

func TestIssueJWT_ExpiryIs8Hours(t *testing.T) {
	opID := uuid.New()
	before := time.Now()
	_, expiry, err := auth.IssueOperatorJWT(opID, "charlie", testJWTSecret)
	if err != nil {
		t.Fatal(err)
	}
	after := time.Now()

	min := before.Add(7*time.Hour + 59*time.Minute)
	max := after.Add(8*time.Hour + 1*time.Minute)

	if expiry.Before(min) || expiry.After(max) {
		t.Errorf("expiry %v not within expected 8h window [%v, %v]", expiry, min, max)
	}
}

func TestIssueJWT_DifferentUsersProduceDifferentTokens(t *testing.T) {
	id1, id2 := uuid.New(), uuid.New()
	t1, _, _ := auth.IssueOperatorJWT(id1, "user1", testJWTSecret)
	t2, _, _ := auth.IssueOperatorJWT(id2, "user2", testJWTSecret)
	if t1 == t2 {
		t.Error("different operators produced identical JWTs")
	}
}
