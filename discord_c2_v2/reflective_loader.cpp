#include <windows.h>
#include <winternl.h>

// Custom Reflective Loader (sRDI)
// This function acts as a custom OS loader, mapping the PE into memory,
// resolving imports, processing relocations, and executing the entry point.

typedef HMODULE(WINAPI* LOADLIBRARYA)(LPCSTR);
typedef FARPROC(WINAPI* GETPROCADDRESS)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* DLLMAIN)(HINSTANCE, DWORD, LPVOID);

// Custom GetProcAddress implementation
FARPROC get_proc_address(HMODULE hModule, LPCSTR lpProcName) {
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY exportDirectory = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hModule + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    PDWORD addressOfFunctions = (PDWORD)((BYTE*)hModule + exportDirectory->AddressOfFunctions);
    PDWORD addressOfNames = (PDWORD)((BYTE*)hModule + exportDirectory->AddressOfNames);
    PWORD addressOfNameOrdinals = (PWORD)((BYTE*)hModule + exportDirectory->AddressOfNameOrdinals);

    for (DWORD i = 0; i < exportDirectory->NumberOfNames; ++i) {
        LPCSTR functionName = (LPCSTR)((BYTE*)hModule + addressOfNames[i]);
        if (strcmp(functionName, lpProcName) == 0) {
            return (FARPROC)((BYTE*)hModule + addressOfFunctions[addressOfNameOrdinals[i]]);
        }
    }
    return NULL;
}

// Custom LoadLibraryA implementation (simplified)
HMODULE load_library_a(LPCSTR lpLibFileName) {
    // In a real sRDI, you'd walk the PEB to find kernel32.dll and its exports.
    // For this example, we assume kernel32 is already loaded and we can find it.
    // This is a placeholder for the actual PEB walking logic.
    return LoadLibraryA(lpLibFileName);
}

void reflective_loader(LPVOID lpPayload) {
    // 1. Resolve Dependencies (Kernel32 base, LoadLibraryA, GetProcAddress)
    // (Assuming we have them for this example)
    LOADLIBRARYA pLoadLibraryA = load_library_a;
    GETPROCADDRESS pGetProcAddress = get_proc_address;

    // 2. Parse Payload Headers
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)lpPayload;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)lpPayload + dosHeader->e_lfanew);

    // 3. Allocate Memory for the Payload
    LPVOID lpAllocatedMemory = VirtualAlloc(NULL, ntHeaders->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!lpAllocatedMemory) return;

    // 4. Map Sections
    // Copy headers
    memcpy(lpAllocatedMemory, lpPayload, ntHeaders->OptionalHeader.SizeOfHeaders);

    // Copy sections
    PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        LPVOID lpSectionDestination = (LPVOID)((BYTE*)lpAllocatedMemory + sectionHeader[i].VirtualAddress);
        LPVOID lpSectionSource = (LPVOID)((BYTE*)lpPayload + sectionHeader[i].PointerToRawData);
        memcpy(lpSectionDestination, lpSectionSource, sectionHeader[i].SizeOfRawData);
    }

    // 5. Resolve IAT (Import Address Table)
    PIMAGE_IMPORT_DESCRIPTOR importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)lpAllocatedMemory + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    while (importDescriptor->Name) {
        LPCSTR libraryName = (LPCSTR)((BYTE*)lpAllocatedMemory + importDescriptor->Name);
        HMODULE hLibrary = pLoadLibraryA(libraryName);

        PIMAGE_THUNK_DATA thunkData = (PIMAGE_THUNK_DATA)((BYTE*)lpAllocatedMemory + importDescriptor->FirstThunk);
        PIMAGE_THUNK_DATA originalThunkData = (PIMAGE_THUNK_DATA)((BYTE*)lpAllocatedMemory + importDescriptor->OriginalFirstThunk);

        while (originalThunkData->u1.AddressOfData) {
            if (IMAGE_SNAP_BY_ORDINAL(originalThunkData->u1.Ordinal)) {
                LPCSTR functionOrdinal = (LPCSTR)IMAGE_ORDINAL(originalThunkData->u1.Ordinal);
                thunkData->u1.Function = (ULONGLONG)pGetProcAddress(hLibrary, functionOrdinal);
            } else {
                PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)((BYTE*)lpAllocatedMemory + originalThunkData->u1.AddressOfData);
                thunkData->u1.Function = (ULONGLONG)pGetProcAddress(hLibrary, importByName->Name);
            }
            ++thunkData;
            ++originalThunkData;
        }
        ++importDescriptor;
    }

    // 6. Process Relocations
    DWORD delta = (DWORD)((BYTE*)lpAllocatedMemory - ntHeaders->OptionalHeader.ImageBase);
    if (delta != 0) {
        PIMAGE_BASE_RELOCATION relocation = (PIMAGE_BASE_RELOCATION)((BYTE*)lpAllocatedMemory + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        while (relocation->VirtualAddress) {
            DWORD count = (relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            PWORD list = (PWORD)(relocation + 1);
            for (DWORD i = 0; i < count; ++i) {
                if (list[i] >> 12 == IMAGE_REL_BASED_DIR64) {
                    PDWORD_PTR address = (PDWORD_PTR)((BYTE*)lpAllocatedMemory + relocation->VirtualAddress + (list[i] & 0xFFF));
                    *address += delta;
                }
            }
            relocation = (PIMAGE_BASE_RELOCATION)((BYTE*)relocation + relocation->SizeOfBlock);
        }
    }

    // 7. Execute Entry Point
    DLLMAIN entryPoint = (DLLMAIN)((BYTE*)lpAllocatedMemory + ntHeaders->OptionalHeader.AddressOfEntryPoint);
    entryPoint((HINSTANCE)lpAllocatedMemory, DLL_PROCESS_ATTACH, NULL);
}
