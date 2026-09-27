// runtime_core.cpp — JOCKY Runtime Core Module v3
// Zero static imports: kernel32/ntdll resolved via PEB walk + export table parse.
#define NULL 0
#include "string_hide.h"
#include "peb_walk.h"

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

typedef unsigned long       DWORD;
typedef unsigned long long  SIZE_T;
typedef void*               PVOID;
typedef PVOID               HANDLE;
typedef int                 BOOL;

#define MEM_COMMIT        0x1000
#define MEM_RESERVE       0x2000
#define PAGE_READWRITE    0x04
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INFINITE          0xFFFFFFFF

typedef PVOID (__stdcall *Fn_VirtualAlloc)  (PVOID,SIZE_T,DWORD,DWORD);
typedef BOOL  (__stdcall *Fn_VirtualProtect)(PVOID,SIZE_T,DWORD,DWORD*);
typedef BOOL  (__stdcall *Fn_WriteFile)     (HANDLE,const PVOID,DWORD,DWORD*,PVOID);
typedef HANDLE(__stdcall *Fn_GetStdHandle)  (DWORD);
typedef void  (__stdcall *Fn_ExitProcess)   (unsigned int);
typedef HANDLE(__stdcall *Fn_CreateThread)  (PVOID,SIZE_T,PVOID,PVOID,DWORD,DWORD*);
typedef DWORD (__stdcall *Fn_WaitForSingleObject)(HANDLE,DWORD);
typedef DWORD (__stdcall *Fn_WaitForMultipleObjects)(DWORD,HANDLE*,BOOL,DWORD);
typedef BOOL  (__stdcall *Fn_CloseHandle)   (HANDLE);
typedef long  (__stdcall *Fn_InterlockedInc)(long volatile*);
typedef DWORD (__stdcall *Fn_GetCurrentThreadId)(void);
typedef DWORD (__stdcall *Fn_GetTickCount)  (void);

// Resolve kernel32 export by function-name hash (no GetProcAddress call)
static PVOID k32(unsigned int fn_hash) {
    static PVOID base = (PVOID)0;
    if (!base) base = peb_get_module(WHASH_K32);
    return peb_get_export(base, fn_hash);
}

// ── Public API ───────────────────────────────────────────────────────────────

extern "C" HANDLE jocky_get_stdout() {
    static Fn_GetStdHandle fn = (Fn_GetStdHandle)0;
    if (!fn) fn = (Fn_GetStdHandle)k32(ct_hash("GetStdHandle"));
    return fn ? fn(STD_OUTPUT_HANDLE) : (HANDLE)0;
}

extern "C" void jocky_exit(unsigned int code) {
    static Fn_ExitProcess fn = (Fn_ExitProcess)0;
    if (!fn) fn = (Fn_ExitProcess)k32(ct_hash("ExitProcess"));
    if (fn) fn(code);
    while (1) {}
}

extern "C" PVOID jocky_alloc(DWORD size) {
    static Fn_VirtualAlloc fn = (Fn_VirtualAlloc)0;
    if (!fn) fn = (Fn_VirtualAlloc)k32(ct_hash("VirtualAlloc"));
    return fn ? fn((PVOID)0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) : (PVOID)0;
}

extern "C" void jocky_write(HANDLE handle, const void* buf, DWORD size) {
    static Fn_WriteFile fn = (Fn_WriteFile)0;
    if (!fn) fn = (Fn_WriteFile)k32(ct_hash("WriteFile"));
    if (fn) { DWORD w = 0; fn(handle, (PVOID)buf, size, &w, (PVOID)0); }
}

extern "C" HANDLE jocky_create_thread(PVOID entry, PVOID param) {
    static Fn_CreateThread fn = (Fn_CreateThread)0;
    if (!fn) fn = (Fn_CreateThread)k32(ct_hash("CreateThread"));
    return fn ? fn((PVOID)0, 0, entry, param, 0, (DWORD*)0) : (HANDLE)0;
}

extern "C" DWORD jocky_wait(HANDLE handle, DWORD ms) {
    static Fn_WaitForSingleObject fn = (Fn_WaitForSingleObject)0;
    if (!fn) fn = (Fn_WaitForSingleObject)k32(ct_hash("WaitForSingleObject"));
    return fn ? fn(handle, ms) : 0xFFFFFFFF;
}

extern "C" DWORD jocky_wait_all(HANDLE* handles, DWORD count, DWORD ms) {
    static Fn_WaitForMultipleObjects fn = (Fn_WaitForMultipleObjects)0;
    if (!fn) fn = (Fn_WaitForMultipleObjects)k32(ct_hash("WaitForMultipleObjects"));
    return fn ? fn(count, handles, 1, ms) : 0xFFFFFFFF;
}

extern "C" BOOL jocky_close(HANDLE h) {
    static Fn_CloseHandle fn = (Fn_CloseHandle)0;
    if (!fn) fn = (Fn_CloseHandle)k32(ct_hash("CloseHandle"));
    return fn ? fn(h) : 0;
}

extern "C" BOOL jocky_protect(PVOID addr, DWORD size, DWORD prot, DWORD* old) {
    static Fn_VirtualProtect fn = (Fn_VirtualProtect)0;
    if (!fn) fn = (Fn_VirtualProtect)k32(ct_hash("VirtualProtect"));
    return fn ? fn(addr, size, prot, old) : 0;
}

extern "C" long jocky_atomic_inc(long volatile* ptr) {
    static Fn_InterlockedInc fn = (Fn_InterlockedInc)0;
    if (!fn) fn = (Fn_InterlockedInc)k32(ct_hash("InterlockedIncrement"));
    return fn ? fn(ptr) : ++(*ptr);
}

extern "C" DWORD jocky_get_tid() {
    static Fn_GetCurrentThreadId fn = (Fn_GetCurrentThreadId)0;
    if (!fn) fn = (Fn_GetCurrentThreadId)k32(ct_hash("GetCurrentThreadId"));
    return fn ? fn() : 0;
}

extern "C" DWORD jocky_get_tick() {
    static Fn_GetTickCount fn = (Fn_GetTickCount)0;
    if (!fn) fn = (Fn_GetTickCount)k32(ct_hash("GetTickCount"));
    return fn ? fn() : 0;
}

// Helper: raw memory ops
extern "C" void jocky_memcopy(PVOID dst, PVOID src, DWORD size) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (DWORD i = 0; i < size; i++) d[i] = s[i];
}

extern "C" void jocky_memzero(PVOID buf, DWORD size) {
    unsigned char* p = (unsigned char*)buf;
    for (DWORD i = 0; i < size; i++) p[i] = 0;
}

extern "C" int jocky_read_int(PVOID base, int offset) {
    return *(int*)((unsigned char*)base + offset);
}

extern "C" void jocky_write_int(PVOID base, int offset, int val) {
    *(int*)((unsigned char*)base + offset) = val;
}

extern "C" unsigned char jocky_read_byte(PVOID base, int offset) {
    return *((unsigned char*)base + offset);
}

extern "C" void jocky_write_byte(PVOID base, int offset, unsigned char val) {
    *((unsigned char*)base + offset) = val;
}

typedef BOOL (__stdcall *Fn_VirtualFree)(PVOID,SIZE_T,DWORD);
extern "C" BOOL jocky_free(PVOID addr, DWORD size) {
    static Fn_VirtualFree fn = (Fn_VirtualFree)0;
    if (!fn) fn = (Fn_VirtualFree)k32(ct_hash("VirtualFree"));
    return fn ? fn(addr, size ? size : 0, size ? 0x4000 : 0x8000) : 0;
}
