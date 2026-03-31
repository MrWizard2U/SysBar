#pragma once

#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD 19
#endif

// Taskbar color uses the System theme
inline bool IsSystemLightTheme() {
    DWORD useLight = 0; DWORD dataSize = sizeof(useLight); HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&useLight), &dataSize);
        RegCloseKey(hKey);
    }
    return useLight == 1;
}

// Dialogs and Menus use the App theme
inline bool IsAppDarkTheme() {
    DWORD useLight = 1; DWORD dataSize = sizeof(useLight); HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&useLight), &dataSize);
        RegCloseKey(hKey);
    }
    return useLight == 0;
}

// Call this ONCE at startup to force the Context Menu to be dark
inline void InitAppTheme() {
    if (IsAppDarkTheme()) {
        HMODULE hUxtheme = LoadLibraryW(L"uxtheme.dll");
        if (hUxtheme) {
            using fnSetPreferredAppMode = int (WINAPI*)(int);
            // Undocumented ordinal 135 forces native Win32 menus into dark mode
            auto setAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
            if (setAppMode) setAppMode(1); // 1 = AllowDark
            FreeLibrary(hUxtheme);
        }
    }
}

// Call on dialog HWNDs to make the title bar and scrollbars dark
inline void ApplyWindowTheme(HWND hwnd) {
    if (IsAppDarkTheme()) {
        BOOL dark = TRUE;
        if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark)))) {
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &dark, sizeof(dark));
        }
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }
}

inline HBRUSH GetDarkBrush() {
    static HBRUSH hbr = CreateSolidBrush(RGB(32, 32, 32));
    return hbr;
}

inline HBRUSH GetDarkInputBrush() {
    static HBRUSH hbr = CreateSolidBrush(RGB(43, 43, 43));
    return hbr;
}