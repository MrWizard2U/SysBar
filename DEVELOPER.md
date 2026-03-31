# SysBar — Developer Documentation

**Author:** Mutant Wizard  
**Version:** 1.2.0  
**Platform:** Windows 10 1809+ x64  
**Stack:** Win32 + DirectComposition + D2D1.1 + DWrite + D3D11  

---

## Table of Contents

1.[Build Instructions](#build-instructions)
2. [Project Structure](#project-structure)
3. [Architecture](#architecture)
4. [Sensor Discovery System](#sensor-discovery-system)
5. [Changelog](#changelog)
6. [Contributing Guidelines](#contributing-guidelines)
7. [Store Submission Notes](#store-submission-notes)
8. [Licence](#licence)

---

## Build Instructions

### Prerequisites

- Visual Studio 2022 (v17.10 or later) with the following workloads:
  - **Desktop development with C++**
  - **Windows 10/11 SDK (10.0.26100.0 or later)**
- Windows 10 SDK minimum version `10.0.17763.0` (1809)
- No additional packages or package managers required
- `json.hpp` (nlohmann/json single-header) — included in project root

### Build steps

1. Open `SysBar.sln` in Visual Studio 2022
2. Set configuration to **Debug x64** for development
3. **Build → Build Solution** (`Ctrl+Shift+B`)
4. Output: `x64\Debug\SysBar.exe`

### Release build

1. Set configuration to **Release x64**
2. **Build → Rebuild Solution**
3. Output: `x64\Release\SysBar.exe`
4. Release build differences vs Debug:
   - Full optimisation (`/O2`, `/GL`, `/LTCG`)
   - No PDB / debug symbols
   - `_DEBUG` macro undefined — `HR()` error dialogs suppressed
   - Approximately 300–600 KB EXE vs 5–10 MB Debug

### Runtime dependency

SysBar requires **LibreHardwareMonitor** running on the same machine with Remote Web Server enabled. No other runtime dependencies. All DirectX components are inbox on Windows 10+.

### Linker inputs (via `#pragma comment`)

All library dependencies are declared inline in source files:

```text
user32.lib  gdi32.lib  shell32.lib  ole32.lib
dcomp.lib   d3d11.lib  dxgi.lib
d2d1.lib    dwrite.lib winhttp.lib
dwmapi.lib  uxtheme.lib
```

---

## Project Structure

```text
SysBar/
├── Config.h              # ParamId enum, ParamDesc table, SensorMap, Config struct
├── Config.cpp            # PARAMS[] table, INI load/save, GetIniPath(), AutoStart registry
├── State.h               # Full application State struct passed by pointer to all modules
├── ThemeUtil.h           # Native Windows Dark Mode API wrappers (DWM, UXTheme, Registry)
├── Widget.h/.cpp         # AppBar registration, RepositionWidget(), ComputeWidgetWidth()
├── Renderer.h/.cpp       # D3D11/DXGI/D2D/DComp init, ResizeSwapChain(), Draw()
├── Sensor.h/.cpp         # HTTP fetch, JSON parse, sensor discovery, ExtractValues()
├── SettingsDlg.h/.cpp    # Settings window — port, param checkboxes, GPU/disk/NIC selectors
├── HelpDlg.h/.cpp        # Help window — scrollable reference text
├── Main.cpp              # wWinMain, WndProc, message loop, single-instance mutex, Timer
├── json.hpp              # nlohmann/json single-header library (MIT licence)
├── SysBar.vcxproj.filters  # Visual Studio filter groups
├── README.md             # End-user documentation
└── DEVELOPER.md          # This file
```

### Visual Studio filter groups

| Filter | Files |
|---|---|
| Core | `Main.cpp`, `State.h`, `Config.h/.cpp`, `ThemeUtil.h`, `Widget.h/.cpp` |
| Rendering | `Renderer.h/.cpp` |
| Sensors | `Sensor.h/.cpp` |
| UI | `SettingsDlg.h/.cpp`, `HelpDlg.h/.cpp` |

---

## Architecture

### Rendering stack

```text
Win32 WS_POPUP window
    └── DirectComposition target (IDCompositionTarget)
            └── IDCompositionVisual
                    └── IDXGISwapChain1 (DXGI_ALPHA_MODE_PREMULTIPLIED)
                            └── ID2D1DeviceContext (D2D1.1)
                                    └── IDWriteTextFormat (Segoe UI)
```

This is the lowest-level composited transparency available on Windows. The swap chain uses premultiplied alpha so the widget background is genuinely transparent — no layered window tricks, no GDI blending.

**Why not WPF / WinUI / UWP:** None of these support true per-pixel transparency on a taskbar-docked surface without dropping down to this exact stack. SysBar uses the correct tool directly.

### Shell integration

SysBar is a **top-level `WS_POPUP` window** registered with the shell via `SHAppBarMessage`. This is the documented extensibility point for taskbar widgets — the same mechanism used by third-party taskbars. It is fully MS Store compliant.

```cpp
SHAppBarMessage(ABM_NEW)     // register on startup
SHAppBarMessage(ABM_SETPOS)  // claim position after each reposition
SHAppBarMessage(ABM_REMOVE)  // deregister on exit
```

The widget listens for `WM_TASKBARCREATED` (Explorer restart), `ABN_POSCHANGED` (taskbar moved), and `WM_DPICHANGED_AFTERPARENT` (DPI change), repositioning itself in response to all three.

To prevent input deadlocks and Z-order corruption with the Windows shell (which steals keyboard focus), repositioning is strictly bypassed if a context menu is open (`s->isMenuOpen`) or if the physical bounding rect has not changed. Taskbar parentage (`GWLP_HWNDPARENT`) is intentionally managed to guarantee TopMost presence natively.

### Threading model

| Thread | Responsibility |
|---|---|
| UI thread | Message loop, `WndProc`, all D2D/DComp rendering |
| Sensor thread | HTTP fetch, JSON parse, sensor discovery, `ExtractValues` |

Communication: sensor thread writes to `State` under `dataMutex`, sets `drawPending` flag, posts `InvalidateRect`. UI thread reads under the same mutex in `Draw()`. No other cross-thread calls.

Shutdown: manual-reset `g_shutdownEvent` — sensor thread blocks on `WaitForSingleObject(shutdownEvent, 1000)` between polls. `WM_DESTROY` signals the event; `wWinMain` joins the thread before COM teardown.

---

## Sensor Discovery System

### Why runtime discovery

LHM sensor paths are not stable across versions or hardware. The sensor index for "P-Core #1 clock" on an Intel CPU is `/intelcpu/0/clock/1` today but could change. Hardcoding indices breaks for AMD CPUs, multi-socket systems, and future LHM versions.

### How discovery works

On first poll (and every 60 seconds), `DiscoverSensors()` inspects the parsed hardware tree:

1. Each `HardwareNode` is classified by `HardwareId` prefix (`/intelcpu/`, `/gpu-nvidia/`, `/nic/`, etc.)
2. For each `ParamId`, the correct sensor is found by matching `Type` field (e.g. `"Temperature"`) and `Text` label (e.g. `"P-Core #1"`) using `ContainsCI()`
3. The discovered `SensorId` strings are cached in `State::sensorMap`
4. `ExtractValues()` does direct map lookups using cached IDs — O(1) per sensor per poll

---

## Changelog

### 1.2.0 — Current release
**Features**
- Native Light/Dark Mode Support: Implemented `dwmapi` and `uxtheme` wrappers to dynamically apply Windows Dark Mode to all context menus, dialog backgrounds, and comboboxes.
- Dynamic Theme Awareness: The widget text automatically queries the Windows Registry (`SystemUsesLightTheme`) every frame and swaps between white and black text depending on the user's taskbar color.
- Task Manager Shortcut: Left-clicking the widget now natively launches Windows Task Manager (`taskmgr.exe`).
- Auto-Start Integration: Added an auto-start checkbox to the Settings menu that safely interacts with the standard Windows `Run` registry key.
- Smart Startup Delay: When launched via Windows Startup (`--autostart`), the widget remains completely hidden (`SW_SHOWNA`) for 5 seconds to prevent visual glitching while LHM and other tray icons initialize.
- Resilient Taskbar Fallback: `RepositionWidget` now gracefully handles missing `TrayNotifyWnd` elements (like on secondary monitors or modified shells), docking to the far right edge without crashing or disappearing.

**Bug Fixes**
- Fixed compiler warning `C4267` in x64 builds by securely casting `size_t` string lengths to `DWORD` during Registry manipulation in `Config.cpp`.

### 1.1.5
**Bug Fixes**
- Fixed Critical Keyboard Deadlock: Eradicated Z-Order thrashing. Removed the periodic `SHAppBarMessage(ABM_SETPOS)` broadcast from the drift-guard, ending the infinite, synchronous broadcast loop with the Windows Shell that was choking the Raw Input Thread (RIT).
- Message Loop Cleanup: Removed redundant global interceptions of `WM_TASKBARCREATED` from the thread's primary `GetMessageW` loop.

### 1.1.4
**Bug Fixes & Optimisations**
- Fixed "focus stealing" bug causing keyboard input loss by eliminating Z-order thrashing.
- Fixed visual layout overlapping caused by floating-point formatting edge cases.
- Eliminated Taskbar / DWM UI thread locking by skipping `SHAppBarMessage` and `ResizeSwapChain` when widget dimensions remain unchanged.

---

## Contributing Guidelines

### Code style
- C++17, MSVC compiler, no third-party libraries beyond `json.hpp`
- All functions receive `State*` — no module-level globals other than `g_state` in `Main.cpp`
- UI thread safety: all D2D/DComp calls on UI thread only; cross-thread data via `dataMutex`
- No `new`/`delete` for sensor data — use stack-local `ParseResult` and RAII containers
- All Win32/DirectX calls wrapped in `HR()` for HRESULT checking
- Native Windows APIs (`uxtheme.h`, `dwmapi.h`) preferred over custom drawing for UI elements

---

## Store Submission Notes

### Package type
MSIX packaged Win32 — `runFullTrust` capability required.

### Required manifest capability
```xml
<Capabilities>
  <rescap:Capability Name="runFullTrust" />
</Capabilities>
```

### Minimum version
```xml
<TargetDeviceFamily Name="Windows.Desktop"
    MinVersion="10.0.17763.0"
    MaxVersionTested="10.0.26100.0" />
```

### INI file location
Config is written to `%LOCALAPPDATA%\SysBar\SysBar.ini`. This path is writable in the MSIX sandbox. No registry writes are required for config, but the `Run` key is used natively for Auto-Start.

### What does NOT require special capabilities
- `WinHTTP` to localhost — no network capability needed (loopback is exempt)
- `SHAppBarMessage` — no special capability needed
- `DirectComposition` / D3D11 — no special capability needed
- Standard Registry `Run` keys — MSIX virtualises these into native Startup Tasks without breaking the sandbox.