#include <windows.h>
#include <stdio.h>

// Fox's Remote PE Parser & LDE Taint Analysis PoC
//
// Jack's Roast: 
// 1. Local ntdll != Remote ntdll (Sandboxes, WoW64, updates).
// 2. Byte scanning '48 89' is fragile garbage.
// 3. Functions call internal wrappers (TpAllocWait -> RtlpAllocWait).
// 4. Registers change (RCX -> R8 -> [RAX+10h]).
//
// The Architect's Solution:
// 1. Read the remote process's PEB and parse the remote ntdll.dll in memory.
// 2. Parse the remote Export Directory to find the exact remote address of TpAllocWait.
// 3. Use a Length Disassembler Engine (LDE) to safely walk instructions.
// 4. Follow relative branches (E8 CALL, E9 JMP) to trace into internal wrappers.
// 5. Perform Taint Analysis: Track the flow of the target register (e.g., RCX) 
//    through MOV instructions until it is stored into memory.

// Simplified Remote PE Parser
DWORD64 GetRemoteExport(HANDLE hProcess, DWORD64 remoteBase, const char* exportName) {
    IMAGE_DOS_HEADER dosHeader;
    ReadProcessMemory(hProcess, (PVOID)remoteBase, &dosHeader, sizeof(dosHeader), NULL);
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS64 ntHeaders;
    ReadProcessMemory(hProcess, (PVOID)(remoteBase + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), NULL);

    DWORD exportDirRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportDirRva) return 0;

    IMAGE_EXPORT_DIRECTORY exportDir;
    ReadProcessMemory(hProcess, (PVOID)(remoteBase + exportDirRva), &exportDir, sizeof(exportDir), NULL);

    DWORD* nameRvas = (DWORD*)malloc(exportDir.NumberOfNames * sizeof(DWORD));
    WORD* ordinals = (WORD*)malloc(exportDir.NumberOfNames * sizeof(WORD));
    DWORD* funcRvas = (DWORD*)malloc(exportDir.NumberOfFunctions * sizeof(DWORD));

    ReadProcessMemory(hProcess, (PVOID)(remoteBase + exportDir.AddressOfNames), nameRvas, exportDir.NumberOfNames * sizeof(DWORD), NULL);
    ReadProcessMemory(hProcess, (PVOID)(remoteBase + exportDir.AddressOfNameOrdinals), ordinals, exportDir.NumberOfNames * sizeof(WORD), NULL);
    ReadProcessMemory(hProcess, (PVOID)(remoteBase + exportDir.AddressOfFunctions), funcRvas, exportDir.NumberOfFunctions * sizeof(DWORD), NULL);

    DWORD64 targetAddress = 0;
    char currentName[256];

    // Binary search is better, but linear is fine for PoC
    for (DWORD i = 0; i < exportDir.NumberOfNames; i++) {
        ReadProcessMemory(hProcess, (PVOID)(remoteBase + nameRvas[i]), currentName, sizeof(currentName), NULL);
        if (strcmp(currentName, exportName) == 0) {
            targetAddress = remoteBase + funcRvas[ordinals[i]];
            break;
        }
    }

    free(nameRvas); free(ordinals); free(funcRvas);
    return targetAddress;
}

// Simulated Length Disassembler and Taint Tracker
// In production, this links against Zydis or Capstone.
BOOL AnalyzeRemoteFunctionFlow(HANDLE hProcess, DWORD64 funcAddress, PDWORD pCallbackOffset) {
    BYTE codeBuffer[1024];
    ReadProcessMemory(hProcess, (PVOID)funcAddress, codeBuffer, sizeof(codeBuffer), NULL);

    DWORD64 currentRip = funcAddress;
    int offset = 0;
    
    // Taint tracking state
    // We know RCX (Register 1) holds the Callback at the start of TpAllocWait.
    int taintedRegister = 1; // 1 = RCX

    printf("[+] Starting LDE and Taint Analysis at %llX...\n", currentRip);

    while (offset < sizeof(codeBuffer)) {
        BYTE op = codeBuffer[offset];

        // 1. Handle Branches (Follow internal wrappers)
        // E8 = CALL rel32, E9 = JMP rel32
        if (op == 0xE8 || op == 0xE9) {
            LONG rel32 = *(PLONG)(&codeBuffer[offset + 1]);
            DWORD64 destination = currentRip + 5 + rel32;
            printf("    -> Followed branch (%s) to internal function: %llX\n", (op == 0xE8) ? "CALL" : "JMP", destination);
            
            // Recursively analyze the internal function (or just update our buffer and continue)
            // For PoC, we simulate jumping into the new function:
            return AnalyzeRemoteFunctionFlow(hProcess, destination, pCallbackOffset);
        }

        // 2. Handle Register-to-Register MOVs (Taint Propagation)
        // e.g., MOV R8, RCX (4C 8B C1)
        if (op == 0x4C && codeBuffer[offset+1] == 0x8B) {
            BYTE modRm = codeBuffer[offset+2];
            int srcReg = modRm & 0x07;
            int dstReg = (modRm >> 3) & 0x07;
            
            if (srcReg == taintedRegister) {
                taintedRegister = dstReg + 8; // R8-R15
                printf("    -> Taint moved to register %d\n", taintedRegister);
            }
        }

        // 3. Handle Register-to-Memory MOVs (The Sink)
        // e.g., MOV [RAX+10h], R8 or MOV [RBX+10h], RCX
        if (codeBuffer[offset] == 0x48 || codeBuffer[offset] == 0x4C) { // REX.W
            if (codeBuffer[offset+1] == 0x89) { // MOV r/m64, reg
                BYTE modRm = codeBuffer[offset+2];
                int srcReg = (modRm >> 3) & 0x07;
                
                // Adjust srcReg if REX.R is set (4C)
                if (codeBuffer[offset] == 0x4C) srcReg += 8;

                if (srcReg == taintedRegister) {
                    // We found where the tainted register is stored into memory!
                    // Extract the displacement
                    BYTE mod = (modRm >> 6) & 0x03;
                    if (mod == 1) { // disp8
                        *pCallbackOffset = codeBuffer[offset+3];
                        printf("    -> Tainted register stored at offset: 0x%X\n", *pCallbackOffset);
                        return TRUE;
                    } else if (mod == 2) { // disp32
                        *pCallbackOffset = *(PDWORD)(&codeBuffer[offset+3]);
                        printf("    -> Tainted register stored at offset: 0x%X\n", *pCallbackOffset);
                        return TRUE;
                    }
                }
            }
        }

        // Advance instruction pointer using a Length Disassembler Engine (LDE)
        // In this PoC, we simulate the LDE advancing by a fixed/guessed amount, 
        // but Zydis would provide the exact instruction length here.
        int instrLength = 1; // Simulated LDE length resolution
        if (op == 0xE8 || op == 0xE9) instrLength = 5;
        else if (op == 0x48 || op == 0x4C) instrLength = 4; // Simplified
        
        offset += instrLength;
        currentRip += instrLength;
        
        // Failsafe
        if (offset > 500) break; 
    }

    return FALSE;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <Target_PID> <Remote_Ntdll_Base_Hex>\n", argv[0]);
        return -1;
    }

    DWORD targetPid = atoi(argv[1]);
    DWORD64 remoteNtdllBase = strtoull(argv[2], NULL, 16);

    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, targetPid);
    if (!hProcess) return -1;

    printf("[+] Resolving TpAllocWait from REMOTE process memory...\n");
    DWORD64 remoteTpAllocWait = GetRemoteExport(hProcess, remoteNtdllBase, "TpAllocWait");

    if (!remoteTpAllocWait) {
        printf("[-] Failed to find remote export.\n");
        CloseHandle(hProcess);
        return -1;
    }

    printf("[+] Remote TpAllocWait found at: %llX\n", remoteTpAllocWait);

    DWORD callbackOffset = 0;
    if (AnalyzeRemoteFunctionFlow(hProcess, remoteTpAllocWait, &callbackOffset)) {
        printf("[!] Success. Architect-level resolution complete.\n");
        printf("[!] TP_WAIT->Callback Offset: 0x%X\n", callbackOffset);
    } else {
        printf("[-] Taint analysis failed to find the sink.\n");
    }

    CloseHandle(hProcess);
    return 0;
}