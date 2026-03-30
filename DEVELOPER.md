# SysBar — Developer Documentation

**Author:** Mutant Wizard  
**Version:** 1.1.5
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
```

---

## Project Structure

```text
SysBar/
├── Config.h              # ParamId enum, ParamDesc table, SensorMap, Config struct
├── Config.cpp            # PARAMS[] table, INI load/save, GetIniPath()
├── State.h               # Full application State struct passed by pointer to all modules
├── Widget.h/.cpp         # AppBar registration, RepositionWidget(), ComputeWidgetWidth()
├── Renderer.h/.cpp       # D3D11/DXGI/D2D/DComp init, ResizeSwapChain(), Draw()
├── Sensor.h/.cpp         # HTTP fetch, JSON parse, sensor discovery, ExtractValues()
├── SettingsDlg.h/.cpp    # Settings window — port, param checkboxes, GPU/disk/NIC selectors
├── HelpDlg.h/.cpp        # Help window — scrollable reference text
├── Main.cpp              # wWinMain, WndProc, message loop, single-instance mutex
├── json.hpp              # nlohmann/json single-header library (MIT licence)
├── SysBar.vcxproj.filters  # Visual Studio filter groups
├── README.md             # End-user documentation
└── DEVELOPER.md          # This file
```

### Visual Studio filter groups

| Filter | Files |
|---|---|
| Core | `Main.cpp`, `State.h`, `Config.h/.cpp`, `Widget.h/.cpp` |
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

A 5-second drift guard posts `WM_USER+1` from the sensor thread as a safety net. To prevent input deadlocks and Z-order corruption with the Windows shell (which steals keyboard focus), repositioning is strictly bypassed if a context menu is open (`s->isMenuOpen`) or if the physical bounding rect has not changed. Taskbar parentage (`GWLP_HWNDPARENT`) is intentionally managed to guarantee TopMost presence without continuously hammering the Desktop Window Manager.

### Threading model

| Thread | Responsibility |
|---|---|
| UI thread | Message loop, `WndProc`, all D2D/DComp rendering |
| Sensor thread | HTTP fetch, JSON parse, sensor discovery, `ExtractValues` |

Communication: sensor thread writes to `State` under `dataMutex`, sets `drawPending` flag, posts `InvalidateRect`. UI thread reads under the same mutex in `Draw()`. No other cross-thread calls.

Shutdown: manual-reset `g_shutdownEvent` — sensor thread blocks on `WaitForSingleObject(shutdownEvent, 1000)` between polls. `WM_DESTROY` signals the event; `wWinMain` joins the thread before COM teardown.

### Column layout

```text
[ Normal params, 2 per column, canonical order ]  [ Disk ]  [ Net ]
   ↑ repacked contiguously, no gaps               ↑ fixed   ↑ always rightmost
```

Normal params are rendered in `ParamId` enum order. Since related params are adjacent in the enum (CPU Load + CPU Temp are consecutive), they naturally pair into the same column. Fixed-pair columns (Disk, Net) are always appended last so Network is always the rightmost column regardless of what else is enabled.

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

### Discovery rules per parameter

| Param | Hardware category | Type | Text match (priority order) |
|---|---|---|---|
| CPU Load | `/intelcpu/` or `/amdcpu/` | Load | "Total" |
| CPU Temp | same | Temperature | "P-Core #1", "Tdie", "Core #1", "Package" |
| CPU Clock | same | Clock | "P-Core #1", "Core #1" (exclude "Bus") |
| CPU Power | same | Power | "Package" |
| RAM Load | `/ram/` | Load | first Load sensor |
| RAM Used | `/ram/` | Data | "Memory Used", "Used" |
| RAM Temp | `/memory/dimm/` | Temperature | first Temperature sensor |
| GPU Load | selected GPU path | Load | "3D" first, else first Load |
| GPU Temp | same | Temperature | first Temperature |
| GPU Mem | same | SmallData | first SmallData |
| GPU Power | same | Power | first Power |
| Net Up/Dn | `/nic/` | Throughput | "Upload"/"Up", "Download"/"Down" |
| Disk R/W | selected disk path | Throughput | "Read", "Write" |

### NIC aggregation

NIC throughput sensor paths contain a GUID that differs per machine. Discovery extracts the **suffix pattern** (e.g. `/throughput/7`) from the first available NIC, then `ExtractValues` applies that suffix to all selected NIC paths and sums the results.

---

## Changelog

### 1.1.5 — Current release
**Bug fixe**
- Fixed: Resolved an edge-case issue that could disrupt keyboard input/operation under certain conditions.

### 1.1.4
**Bug fixes & Optimisations**
- Fixed context menu freeze / unresponsive right-clicks caused by taskbar Z-order corruption during periodic drift-guard repositioning.
- Fixed "focus stealing" bug causing keyboard input loss by eliminating Z-order thrashing; `SetWindowPos` and `SHAppBarMessage` are now strictly bypassed if the widget's physical pixel coordinates haven't changed.
- Fixed buffer overflow vulnerabilities in INI path resolution and NIC adapter parsing.
- Fixed visual layout overlapping caused by floating-point formatting edge cases (mathematically guaranteeing values like `999.6` scale up dynamically instead of expanding to a 5-character string).
- Eliminated Taskbar / DWM UI thread locking by skipping `SHAppBarMessage` and `ResizeSwapChain` when widget dimensions remain unchanged.
- Reduced WinHTTP timeouts to prevent sensor thread stalling when LibreHardwareMonitor is unreachable.
- Fixed Settings window memory leaks by binding context cleanup directly to the `WM_DESTROY` lifecycle.

### 1.1.3
**Features**
- Runtime sensor discovery — no hardcoded sensor indices; works across LHM versions and hardware vendors
- 13 configurable parameters: CPU (Load, Temp, Clock, Power), RAM (Load, Used GB, Temp), GPU (Load, Temp, Memory, Power), Network (Up+Down), Disk (Read+Write)
- Dynamic column layout — related params pair automatically; Network always rightmost
- Fixed-pair columns for Network and Disk — single checkbox, full column each
- User-configurable GPU, Disk, and NIC selection in Settings
- Auto-discovery and aggregation of non-virtual NICs
- Per-pixel transparency via DirectComposition premultiplied alpha swap chain
- AppBar shell registration (`SHAppBarMessage`) — MS Store compliant
- Single-instance enforcement via named mutex
- D3D11 WARP software renderer fallback for broken GPU drivers
- Settings panel with live GPU/Disk dropdowns and NIC multi-select
- Scrollable Help dialog
- 3-char widget labels with `[label]` reference in Settings
- 4-char maximum value display with auto-scaling units
- Windows clock-style line spacing (natural leading, no artificial gap)
- `%LOCALAPPDATA%\\SysBar` INI path for MSIX compatibility
- Column count validation based on actual columns, not raw param count

**Bug fixes**
- Fixed horizontal shift caused by stale `TrayNotifyWnd` handle — now re-queried fresh on every reposition
- Fixed `WM_TASKBARCREATED` not re-registering AppBar after Explorer restart
- Fixed GPU sensors returning zero — was caused by `HardwareType` field absence in LHM JSON; now uses `HardwareId` prefix classification
- Fixed NIC throughput returning zero — corrected sensor suffix pattern discovery
- Fixed disk throughput returning zero — corrected throughput sensor index (`/throughput/54`, `/throughput/55`)
- Fixed `FormatValue` 2-argument call after signature update to 3 arguments
- Fixed `FormatSpeed` defined after use — moved above `BuildColumns`
- Fixed `ContainsCI` heap allocations — replaced with zero-allocation sliding window comparison
- Fixed `pollCount` signed integer overflow — changed to `unsigned int`
- Fixed duplicate `<set>` include in `Sensor.cpp`
- Fixed `DWORD` redefinition across translation units — moved `windows.h` include to `Config.h`
- Fixed `wchar_t` to `std::string` direct assignment — replaced with `WideCharToMultiByte`
- Fixed structured binding `auto[x,y]` requiring `/std:c++17` — added explicit variable declarations as fallback

---

## Contributing Guidelines

### Code style

- C++17, MSVC compiler, no third-party libraries beyond `json.hpp`
- All functions receive `State*` — no module-level globals other than `g_state` in `Main.cpp`
- UI thread safety: all D2D/DComp calls on UI thread only; cross-thread data via `dataMutex`
- No `new`/`delete` for sensor data — use stack-local `ParseResult` and RAII containers
- No reflection, dynamic typing, or unsafe code
- All Win32/DirectX calls wrapped in `HR()` for HRESULT checking
- Comments only for non-obvious logic

### Adding a new sensor parameter

1. Add a new `ParamId` value to the enum in `Config.h` — insert it in the correct group, before `P_DISK_READWRITE`
2. Add a `ParamDesc` entry to `PARAMS[]` in `Config.cpp` — 3-char label, full name with `[label]`, `isFixedPair = false`
3. Add default enabled state to `Config::Config()` constructor
4. Add discovery logic to `DiscoverSensors()` in `Sensor.cpp` — match by hardware category, Type, and Text
5. Add value extraction to `ExtractValues()` — direct map lookup via discovered SensorId
6. Add formatting to `FormatValue()` — max 4 chars output
7. Update `HelpDlg.cpp` parameter reference section

### Pull requests

- One feature or fix per PR
- Must build cleanly in both Debug and Release with zero warnings
- Must not break existing sensor discovery for Intel/AMD CPU, Intel iGPU, NIC aggregation
- Update `CHANGELOG` section in `DEVELOPER.md` with the change

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
Config is written to `%LOCALAPPDATA%\SysBar\SysBar.ini`. This path is writable in MSIX sandbox. No registry writes.

### What does NOT require special capabilities
- `WinHTTP` to localhost — no network capability needed (loopback is exempt)
- `SHAppBarMessage` — no special capability needed
- `DirectComposition` / D3D11 — no special capability needed
- `FindWindowW(Shell_TrayWnd)` for positioning — no special capability needed

### Packaging steps in VS2022
1. Solution → Add → New Project → **Windows Application Packaging Project**
2. Applications → Add Reference → SysBar
3. Edit `Package.appxmanifest` — add `runFullTrust` capability
4. Set `WindowsTargetPlatformMinVersion` = `10.0.17763.0`
5. Build packaging project in **Release x64**
6. Output: `AppPackages\SysBar_1.1.4.0_x64.msix`

### Before submitting
- Test on a clean Windows 10 1809 VM
- Test on Windows 11 (latest)
- Verify widget positions correctly at 100%, 125%, 150%, 200% DPI scaling
- Verify Settings dia