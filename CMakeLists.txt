cmake_minimum_required(VERSION 3.16)
project(ActionPlay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Unicode build
add_compile_definitions(UNICODE _UNICODE)

# Sources
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/Recorder.cpp
    src/Player.cpp
)

# Resource file (manifest + version info)
if(MSVC)
    list(APPEND SOURCES src/ActionPlay.rc)
endif()

# WIN32 = no console window
add_executable(ActionPlay WIN32 ${SOURCES})

target_include_directories(ActionPlay PRIVATE src)

target_link_libraries(ActionPlay PRIVATE
    user32
    gdi32
    comctl32
    winmm
)

# MSVC-specific settings
if(MSVC)
    # Use static runtime so the .exe needs no MSVC redist
    set_property(TARGET ActionPlay PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    target_compile_options(ActionPlay PRIVATE /W3 /WX-)

    # Manifest is already embedded via ActionPlay.rc (RT_MANIFEST).
    # Tell the linker not to generate its own to avoid a duplicate-manifest error.
    set_target_properties(ActionPlay PROPERTIES
        LINK_FLAGS "/MANIFEST:NO")
endif()

# MinGW-specific settings
if(MINGW)
    target_link_options(ActionPlay PRIVATE -mwindows -static)
endif()

# Install rule (used by CPack)
install(TARGETS ActionPlay DESTINATION .)

# ── CPack / NSIS installer ────────────────────────────────────────────────────
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME        "ActionPlay")
set(CPACK_PACKAGE_VENDOR      "SlamTheHam")
set(CPACK_PACKAGE_VERSION     "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "Mouse and keyboard macro recorder and player")
set(CPACK_NSIS_DISPLAY_NAME   "ActionPlay")
set(CPACK_NSIS_INSTALL_ROOT   "$PROGRAMFILES64")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\ActionPlay\\\\ActionPlay.lnk' '$INSTDIR\\\\ActionPlay.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\ActionPlay\\\\ActionPlay.lnk'")
include(CPack)
