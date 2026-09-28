package handlers

import (
	"crypto/rand"
	"encoding/hex"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

type BuildResponse struct {
	SHA256      string    `json:"sha256"`
	BuildTime   string    `json:"build_time"`
	Size        int       `json:"size"`
	Obfuscation string    `json:"obfuscation"`
	BuildID     string    `json:"build_id"`
	Timestamp   time.Time `json:"timestamp"`
}

func BuildPayload(c *gin.Context) {
	var request struct {
		TargetOS     string `json:"target_os"`
		Architecture string `json:"architecture"`
		EvasionLevel int    `json:"evasion_level"`
	}

	if err := c.BindJSON(&request); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid request"})
		return
	}

	// Simulate build process
	time.Sleep(2 * time.Second)

	// Generate random SHA256 hash
	hash := make([]byte, 32)
	rand.Read(hash)
	sha256Hash := hex.EncodeToString(hash)

	response := BuildResponse{
		SHA256:      sha256Hash,
		BuildTime:   "2.4s",
		Size:        18432 + randInt(1000, 5000),
		Obfuscation: "LLVM-Obfuscator-v3",
		BuildID:     "BLD-" + time.Now().Format("20060102") + "-" + randomString(6),
		Timestamp:   time.Now(),
	}

	c.JSON(http.StatusOK, response)
}

func randInt(min, max int) int {
	b := make([]byte, 1)
	rand.Read(b)
	return min + int(b[0])%(max-min)
}

func randomString(n int) string {
	const letters = "ABCDEF0123456789"
	bytes := make([]byte, n)
	rand.Read(bytes)
	for i, b := range bytes {
		bytes[i] = letters[b%byte(len(letters))]
	}
	return string(bytes)
}
