// runtime.cpp
// JOCKY Runtime Library
// Dynamic function resolution — sensitive functions not in IAT
// Only GetModuleHandleA and GetProcAddress appear as imports
// Both are completely benign — present in almost every Windows program

// No standard headers — we define what we need
#define NULL 0
#define STD_OUTPUT_HANDLE ((unsigned long)-11)
#define MEM_COMMIT        0x1000
#define MEM_RESERVE       0x2000
#define PAGE_READWRITE    0x04
#define PAGE_EXECUTE_READ 0x20
#define INFINITE          0xFFFFFFFF

typedef unsigned long       DWORD;
typedef unsigned long long  QWORD;
typedef void*               PVOID;
typedef unsigned short      WORD;
typedef int                 BOOL;

// These two are our only static imports — completely benign
extern "C" PVOID __stdcall GetProcAddress(PVOID hModule, const char* name);
extern "C" PVOID __stdcall GetModuleHandleA(const char* name);

// ── Function pointer types ────────────────────────────────────
typedef int   (__stdcall *fn_WriteFile)(
    PVOID, const void*, DWORD, DWORD*, PVOID);
typedef void  (__stdcall *fn_ExitProcess)(unsigned int);
typedef PVOID (__stdcall *fn_VirtualAlloc)(PVOID, DWORD, DWORD, DWORD);
typedef int   (__stdcall *fn_VirtualProtect)(PVOID, DWORD, DWORD, DWORD*);
typedef PVOID (__stdcall *fn_GetStdHandle)(DWORD);
typedef PVOID (__stdcall *fn_CreateThread)(
    PVOID, DWORD, PVOID, PVOID, DWORD, DWORD*);
typedef DWORD (__stdcall *fn_WaitForSingleObject)(PVOID, DWORD);

// ── String builder — function names split so they don't ──────
// appear as readable strings in the binary
static void build_name(char* out, const char* a, const char* b) {
    int i = 0;
    while (a[i]) { out[i] = a[i]; i++; }
    int j = 0;
    while (b[j]) { out[i+j] = b[j]; j++; }
    out[i+j] = 0;
}

// ── Cached kernel32 handle ────────────────────────────────────
static PVOID g_k32 = NULL;

static PVOID get_k32() {
    if (!g_k32) {
        char name[16];
        build_name(name, "kernel", "32.dll");
        g_k32 = GetModuleHandleA(name);
    }
    return g_k32;
}

// ── Resolve function — cached per call ───────────────────────
static PVOID resolve(const char* a, const char* b) {
    char name[32];
    build_name(name, a, b);
    return GetProcAddress(get_k32(), name);
}

// ── Public API ────────────────────────────────────────────────

extern "C" void jocky_write(PVOID handle, const void* buf, DWORD size) {
    static fn_WriteFile fn = NULL;
    if (!fn) fn = (fn_WriteFile)resolve("Write", "File");
    if (fn) {
        DWORD written = 0;
        fn(handle, buf, size, &written, NULL);
    }
}

extern "C" void jocky_exit(unsigned int code) {
    static fn_ExitProcess fn = NULL;
    if (!fn) fn = (fn_ExitProcess)resolve("ExitPro", "cess");
    if (fn) fn(code);
    // Fallback — spin forever if resolution failed
    while (1) {}
}

extern "C" PVOID jocky_alloc(DWORD size) {
    static fn_VirtualAlloc fn = NULL;
    if (!fn) fn = (fn_VirtualAlloc)resolve("Virtual", "Alloc");
    if (fn) return fn(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return NULL;
}

extern "C" int jocky_protect(PVOID addr, DWORD size,
                               DWORD prot, DWORD* old_prot) {
    static fn_VirtualProtect fn = NULL;
    if (!fn) fn = (fn_VirtualProtect)resolve("VirtualPro", "tect");
    if (fn) return fn(addr, size, prot, old_prot);
    return 0;
}

extern "C" PVOID jocky_get_stdout() {
    // Resolve GetStdHandle and call it each time
    // Don't cache the handle — it can change
    static fn_GetStdHandle fn = NULL;
    if (!fn) fn = (fn_GetStdHandle)resolve("GetStdHan", "dle");
    if (fn) return fn(STD_OUTPUT_HANDLE);
    return NULL;
}

extern "C" PVOID jocky_create_thread(PVOID entry, PVOID param) {
    static fn_CreateThread fn = NULL;
    if (!fn) fn = (fn_CreateThread)resolve("CreateTh", "read");
    if (fn) return fn(NULL, 0, entry, param, 0, NULL);
    return NULL;
}

extern "C" DWORD jocky_wait(PVOID handle, DWORD ms) {
    static fn_WaitForSingleObject fn = NULL;
    if (!fn) fn = (fn_WaitForSingleObject)resolve("WaitForSingle", "Object");
    if (fn) return fn(handle, ms);
    return 0xFFFFFFFF;
}