package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

type TelemetryEvent struct {
	Timestamp time.Time `json:"timestamp"`
	Level     string    `json:"level"`
	Message   string    `json:"message"`
	Endpoint  string    `json:"endpoint"`
}

func GetTelemetry(c *gin.Context) {
	events := []TelemetryEvent{
		{
			Timestamp: time.Now().Add(-10 * time.Second),
			Level:     "INFO",
			Message:   "[+] Process hollowed: explorer.exe",
			Endpoint:  "EP-7A3F2C91",
		},
		{
			Timestamp: time.Now().Add(-25 * time.Second),
			Level:     "WARN",
			Message:   "[!] EDR callback disabled: ntdll.dll",
			Endpoint:  "EP-7A3F2C91",
		},
		{
			Timestamp: time.Now().Add(-40 * time.Second),
			Level:     "INFO",
			Message:   "[*] Direct syscall allocated: NtAllocateVirtualMemory",
			Endpoint:  "EP-9B4D1E82",
		},
		{
			Timestamp: time.Now().Add(-55 * time.Second),
			Level:     "CRIT",
			Message:   "[+] Kernel driver loaded: jocky_rk.sys",
			Endpoint:  "EP-9B4D1E82",
		},
		{
			Timestamp: time.Now().Add(-1 * time.Minute),
			Level:     "INFO",
			Message:   "[*] Memory patched: AMSI.DLL!AmsiScanBuffer",
			Endpoint:  "EP-7A3F2C91",
		},
		{
			Timestamp: time.Now().Add(-90 * time.Second),
			Level:     "WARN",
			Message:   "[!] ETW logging disabled",
			Endpoint:  "EP-2C8A5F63",
		},
		{
			Timestamp: time.Now().Add(-2 * time.Minute),
			Level:     "INFO",
			Message:   "[+] Shellcode injected into: svchost.exe (PID: 4582)",
			Endpoint:  "EP-9B4D1E82",
		},
		{
			Timestamp: time.Now().Add(-3 * time.Minute),
			Level:     "CRIT",
			Message:   "[*] Persistence achieved: Registry Run key",
			Endpoint:  "EP-2C8A5F63",
		},
	}

	c.JSON(http.StatusOK, gin.H{
		"events":        events,
		"stream_active": true,
	})
}
