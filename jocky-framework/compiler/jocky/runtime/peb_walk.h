#pragma once
// PEB walking - resolves modules and exports with zero static imports

typedef unsigned long long  ULONG_PTR;
typedef unsigned long       ULONG32;
typedef unsigned short      USHORT;
typedef void*               PVOID;

struct JKY_LIST_ENTRY {
    struct JKY_LIST_ENTRY* Flink;
    struct JKY_LIST_ENTRY* Blink;
};

struct PEB_LDR_DATA {
    ULONG32        Length;
    ULONG32        Initialized;
    PVOID          SsHandle;
    JKY_LIST_ENTRY InLoadOrderModuleList;
    JKY_LIST_ENTRY InMemoryOrderModuleList;
};

struct LDR_DATA_TABLE_ENTRY {
    JKY_LIST_ENTRY InLoadOrderLinks;
    JKY_LIST_ENTRY InMemoryOrderLinks;
    JKY_LIST_ENTRY InInitializationOrderLinks;
    PVOID          DllBase;
    PVOID      EntryPoint;
    ULONG32    SizeOfImage;
    ULONG32    pad;
    struct { USHORT Length; USHORT MaximumLength; wchar_t* Buffer; } FullDllName;
    struct { USHORT Length; USHORT MaximumLength; wchar_t* Buffer; } BaseDllName;
};

// djb2 hash of a wide string (lowercased)
static inline unsigned int peb_hash_w(const wchar_t* s) {
    unsigned int h = 5381;
    while (*s) {
        wchar_t c = *s++;
        if (c >= L'A' && c <= L'Z') c += 32;
        h = ((h << 5) + h) + (unsigned char)c;
    }
    return h;
}

// djb2 hash of a narrow string
static inline unsigned int peb_hash(const char* s) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h;
}

// Find a loaded module by name hash via PEB InMemoryOrderModuleList
static inline PVOID peb_get_module(unsigned int name_hash) {
    PVOID peb;
#if defined(_M_X64) || defined(__x86_64__)
    __asm__ volatile ("mov %%gs:0x60, %0" : "=r"(peb));
#else
    __asm__ volatile ("mov %%fs:0x30, %0" : "=r"(peb));
#endif
    PEB_LDR_DATA* ldr = *(PEB_LDR_DATA**)((unsigned char*)peb + 0x18);
    JKY_LIST_ENTRY* head = &ldr->InMemoryOrderModuleList;
    JKY_LIST_ENTRY* cur  = head->Flink;
    while (cur != head) {
        LDR_DATA_TABLE_ENTRY* entry =
            (LDR_DATA_TABLE_ENTRY*)((unsigned char*)cur - sizeof(JKY_LIST_ENTRY));
        if (entry->BaseDllName.Buffer) {
            if (peb_hash_w(entry->BaseDllName.Buffer) == name_hash)
                return entry->DllBase;
        }
        cur = cur->Flink;
    }
    return (PVOID)0;
}

// Resolve an export from a module base by function-name hash
static inline PVOID peb_get_export(PVOID base, unsigned int fn_hash) {
    unsigned char* b = (unsigned char*)base;

    // DOS header -> PE header
    unsigned int e_lfanew = *(unsigned int*)(b + 0x3C);
    unsigned char* nth = b + e_lfanew;

    // Optional header offset: +4 (sig) +20 (file header)
    unsigned int opt_off = e_lfanew + 4 + 20;
    unsigned short magic = *(unsigned short*)(b + opt_off);

    // Export directory RVA: DataDirectory[0]
    unsigned int exp_rva;
    if (magic == 0x20B) // PE32+
        exp_rva = *(unsigned int*)(b + opt_off + 112);
    else
        exp_rva = *(unsigned int*)(b + opt_off + 96);

    if (!exp_rva) return (PVOID)0;

    unsigned char* exp = b + exp_rva;
    unsigned int  num_names = *(unsigned int*)(exp + 0x18);
    unsigned int  names_rva = *(unsigned int*)(exp + 0x20);
    unsigned int  ords_rva  = *(unsigned int*)(exp + 0x24);
    unsigned int  funcs_rva = *(unsigned int*)(exp + 0x1C);

    unsigned int*  names = (unsigned int*) (b + names_rva);
    unsigned short* ords = (unsigned short*)(b + ords_rva);
    unsigned int*  funcs = (unsigned int*) (b + funcs_rva);

    for (unsigned int i = 0; i < num_names; i++) {
        const char* name = (const char*)(b + names[i]);
        if (peb_hash(name) == fn_hash)
            return (PVOID)(b + funcs[ords[i]]);
    }
    return (PVOID)0;
}

// Compile-time djb2 hash for narrow strings
static constexpr unsigned int ct_hash(const char* s, unsigned int h = 5381) {
    return *s ? ct_hash(s + 1, ((h << 5) + h) + (unsigned char)(*s)) : h;
}

#define HASH_K32     ct_hash("kernel32.dll")
#define HASH_NTDLL   ct_hash("ntdll.dll")
#define HASH_ADV32   ct_hash("advapi32.dll")

// Compile-time djb2 hash for wide strings (lowercased)
static constexpr unsigned int ct_whash(const wchar_t* s, unsigned int h = 5381) {
    if (!*s) return h;
    wchar_t c = *s;
    if (c >= L'A' && c <= L'Z') c += 32;
    return ct_whash(s + 1, ((h << 5) + h) + (unsigned char)c);
}

#define WHASH_K32    ct_whash(L"kernel32.dll")
#define WHASH_NTDLL  ct_whash(L"ntdll.dll")
#define WHASH_ADV32  ct_whash(L"advapi32.dll")
