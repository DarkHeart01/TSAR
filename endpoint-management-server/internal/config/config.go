package config

import (
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
	"strings"
)

// Config holds all validated runtime configuration for JOCKY.
// Populated once at startup by Load(); never mutated after that.
type Config struct {
	PostgresDSN    string
	RedisAddr      string
	RedisPassword  string
	Port           string
	AESKey         []byte // 32 bytes, decoded from JOCKY_AES_KEY hex
	OperatorSecret string // HS256 JWT signing secret
	AdminPassword  string // bcrypt-hashed on first boot
	WebhookSecret  string // GitHub HMAC-SHA256 webhook secret
	AttackerIP     string // default IP patched into payload
	AttackerPort   uint16 // default port patched into payload
	EC2PublicIP    string // EC2 instance public IP for DNS zone A records
	ZoneFilePath   string // path CoreDNS reads for the zone file
	MaxRetries     int    // max task delivery attempts before marking failed
	GitHubToken    string // optional: needed to download GitHub Actions artifacts
	BundlePath     string // path to pre-built encrypted bundle served to stager
}

// Load reads env vars, validates all required ones are present and
// well-formed, and returns a Config.  Returns an error listing every
// missing or invalid variable so the operator can fix them all at once.
func Load() (*Config, error) {
	var missing []string

	get := func(key string) string {
		v := os.Getenv(key)
		if v == "" {
			missing = append(missing, key)
		}
		return v
	}
	opt := func(key, fallback string) string {
		if v := os.Getenv(key); v != "" {
			return v
		}
		return fallback
	}

	pgDSN          := get("POSTGRES_DSN")
	redisAddr      := opt("REDIS_ADDR", "localhost:6379")
	redisPassword  := os.Getenv("REDIS_PASSWORD")
	aesKeyHex      := get("JOCKY_AES_KEY")
	operatorSecret := get("JOCKY_OPERATOR_SECRET")
	adminPassword  := get("JOCKY_ADMIN_PASSWORD")
	webhookSecret  := get("JOCKY_WEBHOOK_SECRET")
	attackerIP     := get("JOCKY_ATTACKER_IP")
	ec2IP          := get("EC2_PUBLIC_IP")
	zoneFilePath   := opt("ZONE_FILE_PATH", "/etc/coredns/zones/jocky.online.zone")
	port           := opt("PORT", "8080")
	portStr        := opt("JOCKY_ATTACKER_PORT", "4444")
	maxRetriesStr  := opt("JOCKY_MAX_RETRIES", "3")
	githubToken    := os.Getenv("GITHUB_TOKEN")                          // optional
	bundlePath     := opt("BUNDLE_PATH", "/app/binaries/bundle.bin")

	if len(missing) > 0 {
		return nil, fmt.Errorf("missing required env vars: %s", strings.Join(missing, ", "))
	}

	aesKey, err := hex.DecodeString(aesKeyHex)
	if err != nil || len(aesKey) != 32 {
		return nil, fmt.Errorf("JOCKY_AES_KEY must be a 64-character hex string (32 bytes); got %d bytes", len(aesKey))
	}

	if len(operatorSecret) < 32 {
		return nil, fmt.Errorf("JOCKY_OPERATOR_SECRET must be at least 32 characters")
	}
	if len(adminPassword) < 12 {
		return nil, fmt.Errorf("JOCKY_ADMIN_PASSWORD must be at least 12 characters")
	}

	attackerPort, err := strconv.ParseUint(portStr, 10, 16)
	if err != nil {
		return nil, fmt.Errorf("JOCKY_ATTACKER_PORT: %w", err)
	}

	maxRetries, _ := strconv.Atoi(maxRetriesStr)
	if maxRetries <= 0 {
		maxRetries = 3
	}

	return &Config{
		PostgresDSN:    pgDSN,
		RedisAddr:      redisAddr,
		RedisPassword:  redisPassword,
		Port:           port,
		AESKey:         aesKey,
		OperatorSecret: operatorSecret,
		AdminPassword:  adminPassword,
		WebhookSecret:  webhookSecret,
		AttackerIP:     attackerIP,
		AttackerPort:   uint16(attackerPort),
		EC2PublicIP:    ec2IP,
		ZoneFilePath:   zoneFilePath,
		MaxRetries:     maxRetries,
		GitHubToken:    githubToken,
		BundlePath:     bundlePath,
	}, nil
}
