# Discord C2 v2: Technical Analysis & Deployment Guide

## Part 1: Technical Analysis (For the Big Brains)

The `discord_c2_v2` suite is a fully weaponized, native C++ Command and Control (C2) framework that leverages Discord's infrastructure (Webhooks and Bot API) for covert communication. It is designed to bypass modern EDR/AV solutions through a combination of advanced evasion, injection, and persistence techniques.

### Core Components

1.  **Native Beacon (`main_beacon_v2.cpp`)**:
    *   **Communication**: Uses `WinHTTP` to interact with Discord's API natively, avoiding suspicious third-party libraries. It POSTs to a webhook for exfiltration/responses and GETs from a specific channel to poll for commands.
    *   **Obfuscation**: Strings (URLs, Tokens) are encrypted at compile-time using a custom XOR template (`encrypt_string`), preventing static analysis from easily extracting IOCs.
    *   **Jitter**: Implements a randomized sleep interval (Jitter) to disrupt pattern-based network traffic analysis.

2.  **Sleep Obfuscation (`sleep_ekko.cpp`)**:
    *   **Ekko Sleep**: Replaces the standard `Sleep()` API with an advanced technique using Timer Queues and ROP chains via `NtContinue`.
    *   **Mechanism**: Before sleeping, it changes the payload's memory protection to `RW`, encrypts the payload in memory using `SystemFunction032` (RC4), sleeps, decrypts, and restores `RX` protection. This hides the payload from memory scanners (like Moneta or Pe-Sieve) while dormant.

3.  **Evasion & Injection**:
    *   **Ntdll Unhooking (`evasion_native.cpp`)**: Bypasses user-land API hooks by mapping a fresh, clean copy of `ntdll.dll` from disk and overwriting the hooked `.text` section in memory.
    *   **Indirect Syscalls (`injection_indirect_syscalls.cpp`)**: Implements Halo's Gate logic. It dynamically resolves System Service Numbers (SSNs) by searching up and down adjacent memory regions if the target syscall stub is hooked, allowing execution of the `syscall` instruction from a clean region.
    *   **APC Injection (`injection_apc.cpp`)**: Injects shellcode into a remote process by allocating memory, writing the payload, and queueing an Asynchronous Procedure Call (APC) to a target thread, executing when the thread enters an alertable state.
    *   **Reflective Loader (`reflective_loader.cpp`)**: A custom sRDI (Shellcode Reflective DLL Injection) loader. It acts as a custom OS loader, mapping the PE into memory, resolving imports, processing relocations, and executing the entry point without touching the disk.

4.  **Persistence**:
    *   **WMI Persistence (`persistence_wmi.cpp`)**: Fileless persistence using Windows Management Instrumentation. It creates an `__EventFilter` and a `CommandLineEventConsumer`, binding them to trigger execution based on system uptime.
    *   **COM Hijacking (`persistence_com_hijack.cpp`)**: User-land persistence targeting HKCU. It hijacks a frequently loaded CLSID by modifying the `InprocServer32` key to point to the payload, requiring no Administrator privileges.

---

## Part 2: The "Explain It Like I'm 11" Deployment Guide

Alright kiddo, here is how you use this super secret spy tool. Don't tell your mom.

### Step 1: Get Your Discord Ready
1.  **Make a Server**: Create a new, private Discord server just for you.
2.  **Make a Webhook**: Go to Server Settings -> Integrations -> Webhooks. Create a new one. Copy the **Webhook URL**. This is how the spy program talks *to* you.
3.  **Make a Bot**: Go to the Discord Developer Portal. Create a New Application. Go to the "Bot" tab and add a bot. Copy the **Bot Token**. This is how the spy program *listens* to you.
4.  **Invite the Bot**: Invite your new bot to your private server.
5.  **Get the Channel ID**: Right-click the text channel where you invited the bot and click "Copy Channel ID" (you might need to turn on Developer Mode in Discord settings first).

### Step 2: Put the Secret Codes in the Program
1.  Open `discord_c2_v2/main_beacon_v2.cpp` in a text editor (like Notepad or Visual Studio Code).
2.  Find the part that says `send_discord_message`. Replace the fake Webhook URL inside the `OBFUSCATE` thing with your real **Webhook URL**.
3.  Find the part that says `poll_discord_commands`. Replace the fake Bot Token and Channel ID inside the `OBFUSCATE` things with your real **Bot Token** and **Channel ID**.
4.  Save the file.

### Step 3: Build the Spy Tool
1.  Open the "x64 Native Tools Command Prompt for VS" (search for it in your Windows Start menu).
2.  Use the `cd` command to go to the folder where you saved all these files. (e.g., `cd C:\Users\YourName\kawaii`)
3.  Type `build.bat` and press Enter.
4.  The computer will do some magic and create a file called `beacon_v2.exe`. This is your spy tool!

### Step 4: Deploy and Command!
1.  Run `beacon_v2.exe` on the target computer (or your own, for testing). A black window will flash and disappear. It's hiding!
2.  Go to your Discord server. You should see a message from your Webhook saying `[*] Native Discord C2 Beacon Online.`
3.  Now, type commands in that Discord channel!
    *   Type `ping` and press enter. The bot should reply `pong`.
    *   Type `exec calc.exe` and press enter. The calculator should open on the target computer!
    *   Type `exit` to tell the spy tool to go to sleep forever.

**Remember:** With great power comes great responsibility. Only use this on computers you own or have permission to test!