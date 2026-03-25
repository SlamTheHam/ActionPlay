@echo off
setlocal

echo ============================================================
echo  ActionPlay Build Script
echo ============================================================
echo.

:: ── Try to find a build tool ────────────────────────────────────────────────

:: Check for MSVC (Visual Studio Build Tools / VS)
where cl.exe >nul 2>&1
if %errorlevel% == 0 (
    set GENERATOR=Visual Studio 17 2022
    goto :found_msvc
)

:: Check for CMake + Ninja + MinGW-w64
where cmake.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: cmake.exe not found.
    echo Please install one of:
    echo   - Visual Studio 2019/2022 with "Desktop development with C++"
    echo   - CMake + MinGW-w64 (e.g. via winget or https://cmake.org)
    pause
    exit /b 1
)

where g++.exe >nul 2>&1
if %errorlevel% == 0 (
    goto :build_mingw
)

echo ERROR: No C++ compiler found.
echo Please install Visual Studio or MinGW-w64.
pause
exit /b 1

:: ── MSVC path ───────────────────────────────────────────────────────────────
:found_msvc
echo Detected: MSVC (cl.exe)
if not exist build_msvc mkdir build_msvc
cd build_msvc
cmake .. -G "%GENERATOR%" -A x64
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo BUILD FAILED.
    cd ..
    pause
    exit /b 1
)
echo.
echo Build succeeded: build_msvc\Release\ActionPlay.exe
cd ..
goto :done

:: ── MinGW path ──────────────────────────────────────────────────────────────
:build_mingw
echo Detected: MinGW-w64 (g++.exe)
if not exist build_mingw mkdir build_mingw
cd build_mingw
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
if %errorlevel% neq 0 (
    echo BUILD FAILED.
    cd ..
    pause
    exit /b 1
)
echo.
echo Build succeeded: build_mingw\ActionPlay.exe
cd ..
goto :done

:done
echo.
echo ============================================================
echo  Done.  Run ActionPlay.exe to launch.
echo ============================================================
pause
