package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

type Endpoint struct {
	ID           string    `json:"id"`
	Hostname     string    `json:"hostname"`
	OS           string    `json:"os"`
	InternalIP   string    `json:"internal_ip"`
	Status       string    `json:"status"`
	LastSeen     time.Time `json:"last_seen"`
	Architecture string    `json:"architecture"`
	Privileges   string    `json:"privileges"`
}

func GetEndpoints(c *gin.Context) {
	endpoints := []Endpoint{
		{
			ID:           "EP-7A3F2C91",
			Hostname:     "WIN-DESKTOP-01",
			OS:           "Windows 11 Pro",
			InternalIP:   "192.168.1.101",
			Status:       "live",
			LastSeen:     time.Now().Add(-30 * time.Second),
			Architecture: "x64",
			Privileges:   "SYSTEM",
		},
		{
			ID:           "EP-9B4D1E82",
			Hostname:     "Fedora-38",
			OS:           "Fedora 38",
			InternalIP:   "192.168.1.102",
			Status:       "live",
			LastSeen:     time.Now().Add(-2 * time.Minute),
			Architecture: "x64",
			Privileges:   "root",
		},
		{
			ID:           "EP-2C8A5F63",
			Hostname:     "UBUNTU-SRV-02",
			OS:           "Ubuntu 22.04 LTS",
			InternalIP:   "192.168.1.103",
			Status:       "live",
			LastSeen:     time.Now().Add(-5 * time.Minute),
			Architecture: "aarch64",
			Privileges:   "user",
		},
		{
			ID:           "EP-5D9B7E14",
			Hostname:     "MACBOOK-PRO-7",
			OS:           "macOS 14 Sonoma",
			InternalIP:   "192.168.1.104",
			Status:       "offline",
			LastSeen:     time.Now().Add(-2 * time.Hour),
			Architecture: "arm64",
			Privileges:   "user",
		},
		{
			ID:           "EP-4E6C8A25",
			Hostname:     "WIN-SERVER-2019",
			OS:           "Windows Server 2019",
			InternalIP:   "192.168.1.105",
			Status:       "offline",
			LastSeen:     time.Now().Add(-24 * time.Hour),
			Architecture: "x64",
			Privileges:   "SYSTEM",
		},
	}

	c.JSON(http.StatusOK, gin.H{
		"endpoints": endpoints,
		"total":     len(endpoints),
		"live":      countLive(endpoints),
	})
}

func countLive(endpoints []Endpoint) int {
	count := 0
	for _, ep := range endpoints {
		if ep.Status == "live" {
			count++
		}
	}
	return count
}
