# SysBar — Real-Time Hardware Monitor for Windows

**Live CPU, GPU, RAM, Network and Disk stats — right on your Windows taskbar.**

SysBar is a lightweight, transparent taskbar widget that displays real-time hardware sensor data directly alongside your system clock. No overlay windows, no floating panels — just clean, always-visible stats that feel native to Windows.

![SysBar Widget Preview](docs/preview.png)

---

## Links

| | |
|---|---|
| **Website** | [mrwizard2u.github.io](https://mrwizard2u.github.io/) |
| **Privacy Policy** | [mrwizard2u.github.io/privacy.html](https://mrwizard2u.github.io/privacy.html) |
| **License** |[mrwizard2u.github.io/license.html](https://mrwizard2u.github.io/license) |
| ☕ **Support the Project** |[mrwizard2u.github.io/donate.html](https://mrwizard2u.github.io/donate.html) |

---

## Features

- **Taskbar-native display** — sits permanently to the left of the notification area, exactly like the Windows clock
- **Native Light/Dark Mode** — dialogs, menus, and widget text automatically adapt to your Windows theme preferences in real-time
- **Quick Task Manager access** — simply left-click the widget to instantly open Windows Task Manager
- **Auto-start with Windows** — easily toggleable in settings, featuring a smart 5-second delay to ensure LibreHardwareMonitor and your taskbar are fully loaded before appearing
- **Real-time sensor data** — updates every 2 seconds with zero perceptible CPU impact
- **Fully configurable** — choose exactly which parameters to display via right-click Settings
- **Smart column layout** — related parameters pair automatically (CPU load + temp, RAM load + used GB); network and disk always anchor the right side
- **Automatic sensor discovery** — detects your hardware configuration from LibreHardwareMonitor at runtime; works across CPU vendors, GPU vendors, and LHM versions without reconfiguration
- **Per-pixel transparency** — GPU-composited via DirectComposition; zero background flicker
- **Network aggregation** — automatically sums all physical network adapters into a single Up/Down figure; virtual, Bluetooth and tunnel adapters excluded
- **No ads. No telemetry. No internet access.** All data comes from your local LibreHardwareMonitor instance.

---

## System Requirements

| Requirement | Minimum |
|---|---|
| Operating System | Windows 10 version 1809 or later |
| Architecture | x64 |
| RAM | 50 MB available |
| GPU | Any — DirectX 11 hardware or software (WARP) fallback |
| Display | Any resolution, any DPI scaling (96 %–200 %+) |
| Dependencies | LibreHardwareMonitor 0.9.3 or later |

---

## Prerequisites — LibreHardwareMonitor

SysBar reads sensor data from **LibreHardwareMonitor (LHM)**, which must be running on the same machine.

### Install LibreHardwareMonitor

1. Download from [Libre Hardware Monitor](https://librehardwaremonitor.net/) or the [GitHub releases page](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases)
2. Extract to any folder (e.g. `C:\Tools\LibreHardwareMonitor\`)
3. Run `LibreHardwareMonitor.exe` **as Administrator** — right-click → Run as administrator
   - Administrator rights are required for CPU power, some GPU sensors, and DIMM temperatures
4. In LHM: **Options → Remote Web Server → Run** (tick the checkbox)
5. Confirm the status bar shows: *Listening on port 8085*

### Auto-start LibreHardwareMonitor with Windows (recommended)

1. In LHM: **Options → Run On Windows Startup** — tick the checkbox
2. Because LHM needs Administrator rights, Windows UAC will prompt on each startup unless you configure Task Scheduler to run it elevated automatically

---

## Getting Started

1. Start LibreHardwareMonitor as Administrator with Remote Web Server enabled
2. Launch SysBar — the widget appears immediately to the left of your system clock
3. **Left-click** the widget at any time to open Task Manager
4. **Right-click** the widget → **Settings** to configure
5. Select which parameters to display, your GPU, and your preferred network adapters
6. Tick **Run SysBar automatically on Windows startup**
7. Click **Save**

*Note: If SysBar starts automatically with Windows, it will remain invisible for the first 5 seconds to ensure your taskbar icons and LibreHardwareMonitor have finished loading.*

---

## Configuration Guide

Right-click the widget to access the context menu:

| Menu Item | Action |
|---|---|
| Settings... | Opens the configuration panel |
| Help... | Opens this reference in a scrollable window |
| Close Widget | Exits SysBar |

### Settings Panel

**LHM Port** — The port SysBar uses to connect to LibreHardwareMonitor's Remote Web Server. Default is `8085`.

**Display Parameters** — Tick the parameters you want displayed. Each parameter shows its widget label in brackets for reference. Maximum 6 columns total.

**GPU / Disk / Network Adapters** — Select which hardware to monitor. Populated live from LHM when Settings is opened.

**Auto-Start** — Tick "Run SysBar automatically on Windows startup" to seamlessly launch the widget when you log in.

---

## Known Limitations

- **LHM must be running** — SysBar has no built-in sensor access; it depends entirely on LibreHardwareMonitor's Remote Web Server
- **LHM Administrator rights** — without them, CPU power, GPU sensors, and DIMM temperatures may be unavailable and show `N/A`
- **Intel iGPU temperature** — Intel integrated graphics (UHD series) do not expose a temperature sensor in LHM; `GT` will show `N/A` on these systems
- **Third-party shells** — The widget includes a fallback to dock to the far right if standard Windows taskbar components are missing, but systems running heavy taskbar replacements (Stardock, ExplorerPatcher) may still experience visual overlap.
- **Single display** — Because Windows 10 does not replicate the System Tray across multiple monitors, the widget will anchor to the primary display.

---

## Building from Source

### Prerequisites

- Visual Studio 2022 (v17.10 or later) with:
  - **Desktop development with C++** workload
  - **Windows 10/11 SDK (10.0.26100.0 or later)**
- Windows 10 SDK minimum version `10.0.17763.0` (1809)

### Steps

1. Clone the repository
2. Open `SysBar.sln` in Visual Studio 2022
3. Set configuration to **Release x64**
4. **Build → Build Solution** (`Ctrl+Shift+B`)
5. Output: `x64\Release\SysBar.exe`

For full build documentation, architecture details, and contributing guidelines see [`DEVELOPER.md`](DEVELOPER.md).

---

## Privacy

SysBar does not collect, transmit, or store any personal data. It makes no outbound network connections. All sensor data is fetched from LibreHardwareMonitor running locally on the same machine via loopback (`127.0.0.1`).

Full privacy policy: [mrwizard2u.github.io/privacy.html](https://mrwizard2u.github.io/privacy.html)

---

## Support the Project

SysBar is free and open source. If it saves you time or you find it useful, consider supporting development. Any even smallest amout is greatly appreciated:[☕ Donate — mrwizard2u.github.io/donate.html](https://mrwizard2u.github.io/donate.html)

---

## Licence

Copyright © 2026 Mutant Wizard ([mrwizard2u.github.io](https://mrwizard2u.github.io/))

This software is provided under a custom MIT-based licence. You are free to use, copy, modify, and redistribute this software — including as part of a larger product or service — subject to the following conditions:

- The software in its **original unmodified form** may not be sold or distributed as a standalone commercial product without prior written permission from the author.
- All distributed copies must include this licence and the original copyright notice.
- All redistributions must credit the original author: *"Original software by Mutant Wizard (https://mrwizard2u.github.io)"*
- Modified versions must be clearly marked as changed.

See [License](https://mrwizard2u.github.io/license) for the full licence text.