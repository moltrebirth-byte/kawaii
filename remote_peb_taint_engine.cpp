#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <map>
#include <set>
#include <string>

// Assume Zydis is linked
#include <Zydis/Zydis.h>

// Fox's Remote PEB Path Resolution & Advanced Taint Engine PoC
//
// Jack's Final Roast:
// 1. Assuming C:\Windows\System32\ntdll.dll ignores WoW64, SxS, and redirected KnownDlls.
// 2. Taint engine loses data on arithmetic (LEA R8, [RCX+8]).
// 3. Indirect calls (CALL [RAX]) are ignored.
//
// The Architect's Evolution:
// 1. Walk the remote process PEB -> Ldr -> InMemoryOrderModuleList to extract the EXACT 
//    file path of the ntdll.dll loaded in the target process. Map THAT file from disk.
// 2. Upgrade the Taint Engine to track Values + Offsets (Deltas). If RCX is a known base, 
//    and we see LEA R8, [RCX+8], we know R8 = Base + 8.
// 3. If we see an indirect CALL [R8], we can resolve it because we tracked the arithmetic.

// --- 1. Remote PEB Path Resolution ---

// Simplified structures for remote PEB walking
typedef struct _PEB_LDR_DATA_64 {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY64 InLoadOrderModuleList;
    LIST_ENTRY64 InMemoryOrderModuleList;
    LIST_ENTRY64 InInitializationOrderModuleList;
} PEB_LDR_DATA_64, *PPEB_LDR_DATA_64;

typedef struct _LDR_DATA_TABLE_ENTRY_64 {
    LIST_ENTRY64 InLoadOrderLinks;
    LIST_ENTRY64 InMemoryOrderLinks;
    LIST_ENTRY64 InInitializationOrderLinks;
    DWORD64 DllBase;
    DWORD64 EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY_64, *PLDR_DATA_TABLE_ENTRY_64;

// Helper to get the exact path of ntdll.dll loaded in the target process
std::wstring GetRemoteModulePath(HANDLE hProcess, const wchar_t* targetModuleName) {
    PROCESS_BASIC_INFORMATION pbi;
    ULONG returnLength;
    
    // NtQueryInformationProcess to get remote PEB address
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    typedef NTSTATUS(NTAPI* fnNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
    fnNtQueryInformationProcess pNtQueryInformationProcess = (fnNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");
    
    if (!NT_SUCCESS(pNtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength))) {
        return L"";
    }

    // Read PEB
    PEB peb;
    if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(PEB), NULL)) return L"";

    // Read PEB_LDR_DATA
    PEB_LDR_DATA_64 pebLdr;
    if (!ReadProcessMemory(hProcess, peb.Ldr, &pebLdr, sizeof(PEB_LDR_DATA_64), NULL)) return L"";

    // Walk InMemoryOrderModuleList
    DWORD64 currentEntryAddr = (DWORD64)pebLdr.InMemoryOrderModuleList.Flink;
    DWORD64 headAddr = (DWORD64)peb.Ldr + offsetof(PEB_LDR_DATA_64, InMemoryOrderModuleList);

    while (currentEntryAddr != headAddr && currentEntryAddr != 0) {
        LDR_DATA_TABLE_ENTRY_64 entry;
        // Adjust for InMemoryOrderLinks offset
        DWORD64 entryBase = currentEntryAddr - offsetof(LDR_DATA_TABLE_ENTRY_64, InMemoryOrderLinks);
        
        if (!ReadProcessMemory(hProcess, (PVOID)entryBase, &entry, sizeof(LDR_DATA_TABLE_ENTRY_64), NULL)) break;

        // Read BaseDllName
        wchar_t baseName[256] = {0};
        ReadProcessMemory(hProcess, entry.BaseDllName.Buffer, baseName, entry.BaseDllName.Length, NULL);

        if (_wcsicmp(baseName, targetModuleName) == 0) {
            // Found it! Read FullDllName
            wchar_t fullPath[MAX_PATH] = {0};
            ReadProcessMemory(hProcess, entry.FullDllName.Buffer, fullPath, entry.FullDllName.Length, NULL);
            return std::wstring(fullPath);
        }

        currentEntryAddr = (DWORD64)entry.InMemoryOrderLinks.Flink;
    }

    return L"";
}

// --- 2. Advanced Taint Engine (Arithmetic & Indirect Branches) ---

// Represents a tainted value: A known Base Address + an Arithmetic Delta
struct TaintValue {
    bool isTainted;
    DWORD64 knownBase; // e.g., the base address of a structure we are tracking
    LONG delta;        // e.g., +0x18 from an LEA or ADD instruction
};

struct AdvancedTaintState {
    std::map<ZydisRegister, TaintValue> Registers;
};

// Simulated CFG Traversal demonstrating advanced taint tracking
void SimulateAdvancedTaintTracking() {
    printf("[+] Initializing Advanced Taint Engine...\n");

    AdvancedTaintState state;
    
    // Assume RCX holds the base address of a known structure (e.g., TP_WAIT)
    state.Registers[ZYDIS_REGISTER_RCX] = { true, 0x10000000, 0 };
    printf("    -> RCX tainted. Base: 0x%llX, Delta: +0x0\n", state.Registers[ZYDIS_REGISTER_RCX].knownBase);

    // Simulation of Zydis decoding: LEA R8, [RCX + 0x18]
    printf("[+] Simulating Instruction: LEA R8, [RCX + 0x18]\n");
    if (state.Registers[ZYDIS_REGISTER_RCX].isTainted) {
        // We track the arithmetic!
        state.Registers[ZYDIS_REGISTER_R8] = { 
            true, 
            state.Registers[ZYDIS_REGISTER_RCX].knownBase, 
            state.Registers[ZYDIS_REGISTER_RCX].delta + 0x18 
        };
        printf("    -> R8 tainted via LEA. Base: 0x%llX, Delta: +0x%X\n", 
               state.Registers[ZYDIS_REGISTER_R8].knownBase, 
               state.Registers[ZYDIS_REGISTER_R8].delta);
    }

    // Simulation of Zydis decoding: ADD R8, 0x08
    printf("[+] Simulating Instruction: ADD R8, 0x08\n");
    if (state.Registers[ZYDIS_REGISTER_R8].isTainted) {
        state.Registers[ZYDIS_REGISTER_R8].delta += 0x08;
        printf("    -> R8 arithmetic updated. Base: 0x%llX, Delta: +0x%X\n", 
               state.Registers[ZYDIS_REGISTER_R8].knownBase, 
               state.Registers[ZYDIS_REGISTER_R8].delta);
    }

    // Simulation of Zydis decoding: CALL [R8] (Indirect Branch)
    printf("[+] Simulating Instruction: CALL [R8]\n");
    if (state.Registers[ZYDIS_REGISTER_R8].isTainted) {
        // Because we tracked the arithmetic, we know EXACTLY what address is being called.
        DWORD64 resolvedTarget = state.Registers[ZYDIS_REGISTER_R8].knownBase + state.Registers[ZYDIS_REGISTER_R8].delta;
        printf("[!] Indirect Branch Resolved! Target Address: 0x%llX\n", resolvedTarget);
        printf("[!] The engine can now safely follow this indirect call.\n");
    }
}

int main(int argc, char** argv) {
    printf("[+] Starting Remote PEB Path Resolution & Advanced Taint PoC.\n");

    if (argc < 2) {
        printf("Usage: %s <Target_PID>\n", argv[0]);
        return -1;
    }

    DWORD targetPid = atoi(argv[1]);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, targetPid);
    
    if (hProcess) {
        printf("[+] Resolving exact path of ntdll.dll loaded in PID %lu...\n", targetPid);
        std::wstring exactPath = GetRemoteModulePath(hProcess, L"ntdll.dll");
        
        if (!exactPath.empty()) {
            wprintf(L"[!] Success. Exact remote path: %s\n", exactPath.c_str());
            wprintf(L"[!] We will map THIS specific file from disk to ensure 100%% version match.\n");
        } else {
            printf("[-] Failed to resolve remote path. (Access denied or module not found)\n");
        }
        CloseHandle(hProcess);
    } else {
        printf("[-] Failed to open process for PEB reading.\n");
    }

    printf("\n");
    SimulateAdvancedTaintTracking();

    return 0;
}