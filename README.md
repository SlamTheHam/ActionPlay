# ActionPlay

A native Windows macro recorder and player. Record mouse and keyboard actions, then replay them with adjustable speed, repeat count, and pause/resume support.

## Features

- **Record** — captures mouse moves, clicks, scroll, and all keyboard input
- **Play** — replays recorded actions with exact timing
- **Speed multiplier** — 0.1x (slow motion) up to 10x (fast forward)
- **Repeat** — run a sequence a fixed number of times or loop infinitely
- **Pause / Resume** — pause mid-sequence and continue from the same point
- **Global hotkeys** — work even when the window is not focused

## Hotkeys

| Key | Action |
|-----|--------|
| F9  | Start recording |
| F10 | Stop recording  |
| F5  | Play / Resume   |
| F6  | Pause / Resume  |
| F7  | Stop playback   |

## Building

### Requirements

- Windows 10 or later (64-bit)
- **Option A:** Visual Studio 2019 or 2022 with the *Desktop development with C++* workload
- **Option B:** CMake 3.16+ and MinGW-w64 (e.g. via [winget](https://winget.run/) or [MSYS2](https://www.msys2.org/))

### Quick build

Double-click **`build.bat`** — it auto-detects your compiler and produces the `.exe`.

### Manual CMake (MSVC)

```bat
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Manual CMake (MinGW)

```bat
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Output: `build\Release\ActionPlay.exe` (MSVC) or `build\ActionPlay.exe` (MinGW)

## Creating an Installer (.exe)

1. Build `ActionPlay.exe` first (see above)
2. Install [Inno Setup 6](https://jrsoftware.org/isinfo.php)
3. Open `installer\setup.iss` in the Inno Setup Compiler and click **Compile**
4. Installer is written to `installer\Output\ActionPlay-Setup-1.0.0.exe`

## Project Structure

```
ActionPlay/
  src/
    Action.h          — action data structure
    Recorder.h/cpp    — low-level mouse & keyboard hooks (WH_MOUSE_LL, WH_KEYBOARD_LL)
    Player.h/cpp      — SendInput-based playback with threading
    MainWindow.h/cpp  — Win32 window and all UI controls
    resource.h        — control IDs, hotkey IDs, custom message IDs
    ActionPlay.rc     — manifest (visual styles) and version info
    main.cpp          — WinMain entry point
  installer/
    setup.iss         — Inno Setup installer script
  CMakeLists.txt      — CMake build definition
  build.bat           — one-click build script
```
