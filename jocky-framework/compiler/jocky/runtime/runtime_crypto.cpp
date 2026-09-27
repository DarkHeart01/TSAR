// runtime_crypto.cpp — JOCKY Runtime Crypto Module
#define NULL 0
#include <string>
#include "string_hide.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef unsigned char BYTE;
typedef unsigned long DWORD;
typedef void* PVOID;

#define PROV_RSA_AES        24
#define CRYPT_VERIFYCONTEXT 0xF0000000
#define CALG_SHA_256        0x0000800C
#define HP_HASHVAL          0x0002

typedef BOOL (__stdcall *Fn_CryptAcquireContextA)(PVOID*,const char*,const char*,DWORD,DWORD);
typedef BOOL (__stdcall *Fn_CryptCreateHash)(PVOID,DWORD,PVOID,DWORD,PVOID*);
typedef BOOL (__stdcall *Fn_CryptHashData)(PVOID,const BYTE*,DWORD,DWORD);
typedef BOOL (__stdcall *Fn_CryptGetHashParam)(PVOID,DWORD,BYTE*,DWORD*,DWORD);
typedef BOOL (__stdcall *Fn_CryptImportKey)(PVOID,const BYTE*,DWORD,PVOID,DWORD,PVOID*);
typedef BOOL (__stdcall *Fn_CryptDecrypt)(PVOID,PVOID,BOOL,DWORD,BYTE*,DWORD*);
typedef BOOL (__stdcall *Fn_CryptDestroyKey)(PVOID);
typedef BOOL (__stdcall *Fn_CryptDestroyHash)(PVOID);
typedef BOOL (__stdcall *Fn_CryptReleaseContext)(PVOID,DWORD);
typedef HMODULE (__stdcall *Fn_LoadLibraryA)(const char*);

static PVOID g_adv = NULL;
static PVOID get_adv32() {
    if (!g_adv) {
        static Fn_LoadLibraryA fn_ll = NULL;
        if (!fn_ll) {
            USE_STR(s, XB(0x16), XB(0x35), XB(0x3B), XB(0x3E), XB(0x16), XB(0x33), XB(0x38), XB(0x28), XB(0x3B), XB(0x28), XB(0x23), XB(0x1B)) // LoadLibraryA
            fn_ll = (Fn_LoadLibraryA)GetProcAddress(GetModuleHandleA("kernel32.dll"), s);
            ZERO_STR(s);
        }
        if (fn_ll) {
            USE_STR(n, XB(0x3B), XB(0x3E), XB(0x2C), XB(0x3B), XB(0x2A), XB(0x33), XB(0x69), XB(0x68), XB(0x74), XB(0x3E), XB(0x36), XB(0x36)) // advapi32.dll
            g_adv = (PVOID)fn_ll(n);
            ZERO_STR(n);
        }
    }
    return g_adv;
}

static PVOID advs(const char* name) {
    return (PVOID)GetProcAddress((HMODULE)get_adv32(), name);
}

extern "C" void jocky_xor_buf(PVOID buf, DWORD size, PVOID key, DWORD keylen) {
    BYTE* b = (BYTE*)buf;
    const BYTE* k = (const BYTE*)key;
    if (!keylen) return;
    for (DWORD i = 0; i < size; i++) b[i] ^= k[i % keylen];
}

extern "C" DWORD jocky_aes_decrypt(PVOID ciphertext, DWORD ctlen, PVOID key, DWORD keylen) {
    static Fn_CryptAcquireContextA fn_acq = NULL;
    static Fn_CryptCreateHash      fn_ch  = NULL;
    static Fn_CryptHashData        fn_hd  = NULL;
    static Fn_CryptImportKey       fn_ik  = NULL;
    static Fn_CryptDecrypt         fn_dec = NULL;
    static Fn_CryptDestroyKey      fn_dk  = NULL;
    static Fn_CryptReleaseContext  fn_rc  = NULL;

    if (!fn_acq) {
        USE_STR(s, XB(0x19), XB(0x28), XB(0x23), XB(0x2A), XB(0x2E), XB(0x1B), XB(0x39), XB(0x2B), XB(0x2F), XB(0x33), XB(0x28), XB(0x3F), XB(0x19), XB(0x35), XB(0x34), XB(0x2E), XB(0x3F), XB(0x22), XB(0x2E), XB(0x1B)) // CryptAcquireContextA
        fn_acq = (Fn_CryptAcquireContextA)advs(s); ZERO_STR(s);
    }
    // Additional setup logic follows the same pattern...
    return 0;
}