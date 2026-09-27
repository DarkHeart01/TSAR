// dummy_imports.cpp — Realistic IAT population
// These functions are ACTUALLY CALLED (once, cheaply) so they appear
// as genuine live imports, not dead linker-forced symbols.
// Multi-DLL imports: KERNEL32, USER32, ADVAPI32 — matches any legitimate GUI-capable app.

// KERNEL32
#pragma comment(linker, "/include:__imp_QueryPerformanceCounter")
#pragma comment(linker, "/include:__imp_GetSystemTimeAsFileTime")
#pragma comment(linker, "/include:__imp_IsDebuggerPresent")
#pragma comment(linker, "/include:__imp_HeapAlloc")
#pragma comment(linker, "/include:__imp_HeapFree")
#pragma comment(linker, "/include:__imp_GetProcessHeap")
#pragma comment(linker, "/include:__imp_RtlUnwind")
#pragma comment(linker, "/include:__imp_GetEnvironmentVariableA")
#pragma comment(linker, "/include:__imp_GetCommandLineA")
#pragma comment(linker, "/include:__imp_GetCurrentProcessId")
#pragma comment(linker, "/include:__imp_GetLastError")

// USER32 — pulls in a second DLL, changes imphash, normal for any windowed app
#pragma comment(linker, "/include:__imp_GetSystemMetrics")
#pragma comment(linker, "/include:__imp_MessageBoxA")

// ADVAPI32 — third DLL, used by virtually every app that touches the registry
#pragma comment(linker, "/include:__imp_RegOpenKeyExA")
#pragma comment(linker, "/include:__imp_RegCloseKey")

// WS2_32 — network socket DLL; Firefox and WinSCP both import this heavily
#pragma comment(linker, "/include:__imp_WSAStartup")
#pragma comment(linker, "/include:__imp_WSACleanup")
#pragma comment(linker, "/include:__imp_socket")
#pragma comment(linker, "/include:__imp_closesocket")

// OLE32 — COM initialization; all major apps call CoInitialize at startup
#pragma comment(linker, "/include:__imp_CoInitialize")
#pragma comment(linker, "/include:__imp_CoUninitialize")

extern "C" {
    // KERNEL32
    __declspec(dllimport) int           __stdcall QueryPerformanceCounter(void* lpPerformanceCount);
    __declspec(dllimport) void          __stdcall GetSystemTimeAsFileTime(void* lpSystemTimeAsFileTime);
    __declspec(dllimport) int           __stdcall IsDebuggerPresent(void);
    __declspec(dllimport) void*         __stdcall HeapAlloc(void* hHeap, unsigned long dwFlags, unsigned long long dwBytes);
    __declspec(dllimport) int           __stdcall HeapFree(void* hHeap, unsigned long dwFlags, void* lpMem);
    __declspec(dllimport) void*         __stdcall GetProcessHeap(void);
    __declspec(dllimport) void          __stdcall RtlUnwind(void*, void*, void*, void*);
    __declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableA(const char* name, char* buf, unsigned long sz);
    __declspec(dllimport) char*         __stdcall GetCommandLineA(void);
    __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
    __declspec(dllimport) unsigned long __stdcall GetLastError(void);

    // USER32
    __declspec(dllimport) int           __stdcall GetSystemMetrics(int nIndex);
    __declspec(dllimport) int           __stdcall MessageBoxA(void* hWnd, const char* lpText, const char* lpCaption, unsigned int uType);

    // ADVAPI32
    __declspec(dllimport) long          __stdcall RegOpenKeyExA(void* hKey, const char* lpSubKey, unsigned long ulOptions, unsigned long samDesired, void** phkResult);
    __declspec(dllimport) long          __stdcall RegCloseKey(void* hKey);

    // WS2_32 — typedef to avoid winsock2.h dependency
    typedef struct { unsigned short wVersion; unsigned short wHighVersion; char szDescription[257]; char szSystemStatus[129]; unsigned short iMaxSockets; unsigned short iMaxUdpDg; char* lpVendorInfo; } WSADATA_JKY;
    __declspec(dllimport) int           __stdcall WSAStartup(unsigned short wVersionRequested, WSADATA_JKY* lpWSAData);
    __declspec(dllimport) int           __stdcall WSACleanup(void);
    __declspec(dllimport) unsigned long long __stdcall socket(int af, int type, int protocol);
    __declspec(dllimport) int           __stdcall closesocket(unsigned long long s);

    // OLE32
    __declspec(dllimport) long          __stdcall CoInitialize(void* pvReserved);
    __declspec(dllimport) void          __stdcall CoUninitialize(void);
}

extern "C" void jocky_init_imports(void) {
    unsigned long long qpc_val = 0;
    QueryPerformanceCounter(&qpc_val);
    (void)qpc_val;

    unsigned long long ft = 0;
    GetSystemTimeAsFileTime(&ft);
    (void)ft;

    volatile int dbg = IsDebuggerPresent();
    (void)dbg;

    void* heap = GetProcessHeap();
    if (heap) {
        void* p = HeapAlloc(heap, 0, 16);
        if (p) HeapFree(heap, 0, p);
    }

    char env_buf[8] = {0};
    GetEnvironmentVariableA("PATH", env_buf, sizeof(env_buf));

    (void)GetCommandLineA();
    (void)GetCurrentProcessId();
    (void)GetLastError();

    // USER32
    volatile int cx = GetSystemMetrics(0);  // SM_CXSCREEN
    (void)cx;
    // MessageBoxA intentionally NOT called — just needs to be in IAT

    // ADVAPI32
    void* hk = (void*)0;
    // HKEY_LOCAL_MACHINE = 0x80000002
    RegOpenKeyExA((void*)0x80000002, "SOFTWARE", 0, 0x20019 /*KEY_READ*/, &hk);
    if (hk) RegCloseKey(hk);

    // WS2_32 — init and immediately clean up
    WSADATA_JKY wsa = {0};
    WSAStartup(0x0202, &wsa);
    WSACleanup();

    // OLE32 — standard startup pattern
    CoInitialize((void*)0);
    CoUninitialize();
}

// Forces .data section to have nonzero raw size (all-BSS layout is a malware indicator)
extern "C" volatile unsigned char jocky_data_anchor[16] = {
    0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00
};
