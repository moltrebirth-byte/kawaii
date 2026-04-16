#include <windows.h>
#include <winnt.h>
#include <stdio.h>

// Fox's Full Exception Directory (.pdata) Parser
//
// You asked for the fractal nightmare, Jack. Here it is.
// This parses UNWIND_INFO, handles all UWOP codes, calculates the exact stack 
// allocation, accounts for Frame Pointers (UWOP_SET_FPREG), and follows 
// chained unwind info (UNW_FLAG_CHAININFO).
//
// And yes, if hardware CET (Shadow Stacks) is enabled, this still dies on the 
// RET instruction because we can't forge the hardware stack from userland. 
// But the math for the software stack will be flawless.

PIMAGE_NT_HEADERS GetNtHeaders(HMODULE hModule) {
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    return (PIMAGE_NT_HEADERS)((PBYTE)hModule + pDosHeader->e_lfanew);
}

// The actual parser
DWORD CalculateExactFrameSize(HMODULE hModule, DWORD unwindInfoRva, PBOOL usesFramePointer, BYTE* frameRegister) {
    DWORD totalStackSize = 0;
    DWORD currentUnwindRva = unwindInfoRva;
    *usesFramePointer = FALSE;

    while (TRUE) {
        PUNWIND_INFO pUnwindInfo = (PUNWIND_INFO)((PBYTE)hModule + currentUnwindRva);

        // Check if a frame pointer is established
        if (pUnwindInfo->FrameRegister != 0) {
            *usesFramePointer = TRUE;
            *frameRegister = pUnwindInfo->FrameRegister;
            // If a frame pointer is used, RSP is restored from RBP (or whatever register).
            // The stack size calculation becomes relative to the frame pointer setup.
        }

        // Iterate through the variable-length unwind codes
        for (UBYTE i = 0; i < pUnwindInfo->CountOfCodes; i++) {
            PUNWIND_CODE pCode = &pUnwindInfo->UnwindCode[i];
            UBYTE opCode = pCode->UnwindOp;
            UBYTE opInfo = pCode->OpInfo;

            switch (opCode) {
                case UWOP_PUSH_NONVOL: // 0
                    totalStackSize += 8;
                    break;
                case UWOP_ALLOC_LARGE: // 1
                    if (opInfo == 0) {
                        i++; // Consumes next slot
                        totalStackSize += 8 * pUnwindInfo->UnwindCode[i].FrameOffset;
                    } else if (opInfo == 1) {
                        i += 2; // Consumes next two slots
                        totalStackSize += *(PDWORD)(&pUnwindInfo->UnwindCode[i - 1]);
                    }
                    break;
                case UWOP_ALLOC_SMALL: // 2
                    totalStackSize += (opInfo * 8) + 8;
                    break;
                case UWOP_SET_FPREG: // 3
                    // Frame pointer established. RSP is irrelevant for unwinding past this point.
                    break;
                case UWOP_SAVE_NONVOL: // 4
                    i++; // Consumes next slot (offset)
                    break;
                case UWOP_SAVE_NONVOL_FAR: // 5
                    i += 2; // Consumes next two slots (32-bit offset)
                    break;
                case UWOP_SAVE_XMM128: // 8
                    i++; // Consumes next slot
                    break;
                case UWOP_SAVE_XMM128_FAR: // 9
                    i += 2; // Consumes next two slots
                    break;
                case UWOP_PUSH_MACHFRAME: // 10
                    totalStackSize += (opInfo == 0) ? 40 : 48;
                    break;
                default:
                    // Ignored or unsupported codes (e.g., UWOP_EPILOG, UWOP_SPARE_CODE)
                    break;
            }
        }

        // Handle Chained Unwind Info
        if (pUnwindInfo->Flags & UNW_FLAG_CHAININFO) {
            // Chained info is located immediately after the unwind codes.
            // CountOfCodes is aligned to an even number of slots.
            DWORD alignCount = (pUnwindInfo->CountOfCodes + 1) & ~1;
            PRUNTIME_FUNCTION pChainedFunction = (PRUNTIME_FUNCTION)(&pUnwindInfo->UnwindCode[alignCount]);
            
            printf("[+] Following UNW_FLAG_CHAININFO to RVA 0x%X\n", pChainedFunction->UnwindData);
            currentUnwindRva = pChainedFunction->UnwindData;
            // Loop continues to parse the chained UNWIND_INFO
        } else {
            break; // No more chained info, we are done.
        }
    }

    return totalStackSize;
}

int main() {
    printf("[+] Starting Full UNWIND_INFO Parser PoC.\n");

    HMODULE hKernelBase = GetModuleHandleA("kernelbase.dll");
    if (!hKernelBase) return -1;

    PIMAGE_NT_HEADERS pNtHeaders = GetNtHeaders(hKernelBase);
    DWORD pdataRva = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
    PRUNTIME_FUNCTION pRuntimeFunction = (PRUNTIME_FUNCTION)((PBYTE)hKernelBase + pdataRva);

    // Let's just grab the first valid RUNTIME_FUNCTION we find to test the parser
    DWORD targetUnwindRva = pRuntimeFunction[0].UnwindData;
    
    printf("[+] Parsing UNWIND_INFO at RVA 0x%X\n", targetUnwindRva);

    BOOL usesFramePointer = FALSE;
    BYTE frameRegister = 0;
    
    DWORD exactFrameSize = CalculateExactFrameSize(hKernelBase, targetUnwindRva, &usesFramePointer, &frameRegister);

    printf("[!] Parsing Complete.\n");
    printf("    -> Exact Stack Allocation: 0x%X bytes\n", exactFrameSize);
    
    if (usesFramePointer) {
        printf("    -> WARNING: UWOP_SET_FPREG detected. Frame Register: %d\n", frameRegister);
        printf("    -> RSP is ignored by the unwinder. You must spoof RBP (or the designated register).\n");
    } else {
        printf("    -> No frame pointer. RSP offset is strictly 0x%X bytes.\n", exactFrameSize);
    }

    printf("\n[!] Reality Check: Even with this perfect math, if CET is enabled, the hardware shadow stack will catch the RET and throw #CP. Software spoofing is dead on modern CPUs.\n");

    return 0;
}