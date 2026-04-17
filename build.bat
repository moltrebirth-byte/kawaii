@echo off
setlocal
echo [*] Starting Compilation of the Fox Arsenal...

echo [*] Compiling CVE-2025-23198 (Microsoft Teams Zero-Click RCE)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc cve_2025_23198_teams_rce.cpp /link /OUT:teams_rce.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling Native Discord C2 Beacon (v2)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\main_beacon_v2.cpp /link /OUT:beacon_v2.exe /SUBSYSTEM:WINDOWS /MACHINE:x64

echo [*] Compiling Custom Reflective Loader (sRDI)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG /Tc discord_c2_v2\reflective_loader.cpp /link /OUT:loader.exe /SUBSYSTEM:WINDOWS /MACHINE:x64

echo [*] Done.
