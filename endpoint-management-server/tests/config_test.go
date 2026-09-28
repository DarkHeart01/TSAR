package tests

import (
	"os"
	"strings"
	"testing"

	"endpoint-management-server/internal/config"
)

// setEnv sets key=value and schedules restoration via t.Cleanup.
func setEnv(t *testing.T, key, value string) {
	t.Helper()
	old, existed := os.LookupEnv(key)
	os.Setenv(key, value)
	t.Cleanup(func() {
		if existed {
			os.Setenv(key, old)
		} else {
			os.Unsetenv(key)
		}
	})
}

func unsetEnv(t *testing.T, key string) {
	t.Helper()
	old, existed := os.LookupEnv(key)
	os.Unsetenv(key)
	t.Cleanup(func() {
		if existed {
			os.Setenv(key, old)
		}
	})
}

// setValidEnv sets every required env var to a valid value.
func setValidEnv(t *testing.T) {
	t.Helper()
	setEnv(t, "POSTGRES_DSN", "postgres://user:pass@localhost/db")
	setEnv(t, "JOCKY_AES_KEY", strings.Repeat("ab", 32)) // 64 hex chars = 32 bytes
	setEnv(t, "JOCKY_OPERATOR_SECRET", strings.Repeat("x", 32))
	setEnv(t, "JOCKY_ADMIN_PASSWORD", "Str0ngPassword!")
	setEnv(t, "JOCKY_WEBHOOK_SECRET", "webhook-secret-value")
	setEnv(t, "JOCKY_ATTACKER_IP", "10.0.0.1")
	setEnv(t, "EC2_PUBLIC_IP", "1.2.3.4")
}

func TestConfig_ValidEnvLoadsSuccessfully(t *testing.T) {
	setValidEnv(t)

	cfg, err := config.Load()
	if err != nil {
		t.Fatalf("expected success, got: %v", err)
	}
	if cfg == nil {
		t.Fatal("config is nil")
	}
	if len(cfg.AESKey) != 32 {
		t.Errorf("AESKey: want 32 bytes, got %d", len(cfg.AESKey))
	}
	if cfg.AttackerIP != "10.0.0.1" {
		t.Errorf("AttackerIP: want 10.0.0.1, got %q", cfg.AttackerIP)
	}
	if cfg.EC2PublicIP != "1.2.3.4" {
		t.Errorf("EC2PublicIP: want 1.2.3.4, got %q", cfg.EC2PublicIP)
	}
}

func TestConfig_MissingPostgresDSN(t *testing.T) {
	setValidEnv(t)
	unsetEnv(t, "POSTGRES_DSN")

	_, err := config.Load()
	if err == nil {
		t.Fatal("expected error for missing POSTGRES_DSN")
	}
	if !strings.Contains(err.Error(), "POSTGRES_DSN") {
		t.Errorf("error should mention POSTGRES_DSN, got: %v", err)
	}
}

func TestConfig_MultiplesMissingVarsReportedTogether(t *testing.T) {
	setValidEnv(t)
	unsetEnv(t, "POSTGRES_DSN")
	unsetEnv(t, "EC2_PUBLIC_IP")

	_, err := config.Load()
	if err == nil {
		t.Fatal("expected error")
	}
	if !strings.Contains(err.Error(), "POSTGRES_DSN") {
		t.Errorf("error missing POSTGRES_DSN mention: %v", err)
	}
	if !strings.Contains(err.Error(), "EC2_PUBLIC_IP") {
		t.Errorf("error missing EC2_PUBLIC_IP mention: %v", err)
	}
}

func TestConfig_BadAESKeyNotHex(t *testing.T) {
	setValidEnv(t)
	setEnv(t, "JOCKY_AES_KEY", "not-hex-at-all!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")

	_, err := config.Load()
	if err == nil {
		t.Fatal("expected error for non-hex AES key")
	}
	if !strings.Contains(strings.ToLower(err.Error()), "aes") {
		t.Errorf("error should mention AES key, got: %v", err)
	}
}

func TestConfig_AESKeyWrongLength(t *testing.T) {
	setValidEnv(t)
	// 30 hex chars = 15 bytes — not 32.
	setEnv(t, "JOCKY_AES_KEY", strings.Repeat("ab", 15))

	_, err := config.Load()
	if err == nil {
		t.Fatal("expected error for wrong-length AES key")
	}
}

func TestConfig_ShortOperatorSecretRejected(t *testing.T) {
	setValidEnv(t)
	setEnv(t, "JOCKY_OPERATOR_SECRET", "tooshort")

	_, err := config.Load()
	if err == nil {
		t.Fatal("expected error for short operator secret")
	}
}

func TestConfig_ShortAdminPasswordRejected(t *testing.T) {
	setValidEnv(t)
	setEnv(t, "JOCKY_ADMIN_PASSWORD", "short")

	_, err := config.Load()
	if err == nil {
		t.Fatal("expected error for short admin password")
	}
}

func TestConfig_DefaultsApplied(t *testing.T) {
	setValidEnv(t)
	// Don't set optional vars — check defaults are applied.
	unsetEnv(t, "PORT")
	unsetEnv(t, "JOCKY_ATTACKER_PORT")
	unsetEnv(t, "JOCKY_MAX_RETRIES")
	unsetEnv(t, "ZONE_FILE_PATH")

	cfg, err := config.Load()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if cfg.Port != "8080" {
		t.Errorf("default PORT: want 8080, got %q", cfg.Port)
	}
	if cfg.AttackerPort != 4444 {
		t.Errorf("default attacker port: want 4444, got %d", cfg.AttackerPort)
	}
	if cfg.MaxRetries != 3 {
		t.Errorf("default max_retries: want 3, got %d", cfg.MaxRetries)
	}
	if cfg.ZoneFilePath == "" {
		t.Error("ZoneFilePath should have a default")
	}
}

func TestConfig_AttackerPortParsed(t *testing.T) {
	setValidEnv(t)
	setEnv(t, "JOCKY_ATTACKER_PORT", "8443")

	cfg, err := config.Load()
	if err != nil {
		t.Fatal(err)
	}
	if cfg.AttackerPort != 8443 {
		t.Errorf("AttackerPort: want 8443, got %d", cfg.AttackerPort)
	}
}
