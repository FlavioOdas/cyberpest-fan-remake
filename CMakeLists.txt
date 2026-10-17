cmake_minimum_required(VERSION 3.20)
project(CyberPest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cyberpest WIN32
    src/main.cpp
    src/world.cpp
    src/object.cpp
    src/classes.cpp
    src/certificate.cpp
    src/bin.cpp
    src/clock.cpp
    src/panels.cpp
    src/items.cpp
    src/text.cpp
    src/pest.cpp
    src/pest_phone.cpp
    src/punishments.cpp
    src/room_objects.cpp
    src/attention.cpp
    src/archive.cpp
    src/gamedata.cpp
    src/zip.cpp
    src/installer.cpp
    src/adm.cpp
    src/flc.cpp
    src/surface.cpp
    src/audio.cpp
    src/wav.cpp
)
target_compile_definitions(cyberpest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    _WIN32_WINNT=0x0A00 WINVER=0x0A00)  # Windows 10+ (per-monitor DPI APIs)
target_link_libraries(cyberpest PRIVATE winmm gdi32 user32 shell32 advapi32 comdlg32 comctl32 winhttp)

if(MINGW)
    # One self-contained exe: no libc++/libunwind DLLs next to it.
    target_link_options(cyberpest PRIVATE -static -municode)
endif()
if(MSVC)
    set_property(TARGET cyberpest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
