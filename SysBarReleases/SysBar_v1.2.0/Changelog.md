1.2.0 — Current release
Features

Native Light/Dark Mode Support: Implemented dwmapi and uxtheme wrappers to dynamically apply Windows Dark Mode to all context menus, dialog backgrounds, and comboboxes.
Dynamic Theme Awareness: The widget text automatically queries the Windows Registry (SystemUsesLightTheme) every frame and swaps between white and black text depending on the user's taskbar color.
Task Manager Shortcut: Left-clicking the widget now natively launches Windows Task Manager (taskmgr.exe).
Auto-Start Integration: Added an auto-start checkbox to the Settings menu that safely interacts with the standard Windows Run registry key.
Smart Startup Delay: When launched via Windows Startup (--autostart), the widget remains completely hidden (SW_SHOWNA) for 5 seconds to prevent visual glitching while LHM and other tray icons initialize.
Resilient Taskbar Fallback: RepositionWidget now gracefully handles missing TrayNotifyWnd elements (like on secondary monitors or modified shells), docking to the far right edge without crashing or disappearing.
Bug Fixes

Fixed compiler warning C4267 in x64 builds by securely casting size_t string lengths to DWORD during Registry manipulation in Config.cpp.