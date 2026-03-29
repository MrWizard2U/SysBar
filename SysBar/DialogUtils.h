#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// DialogUtils.h — shared helpers for all SysBar dialog windows
//
// Include this header (not State.h directly) in dialog .cpp files that need
// positioning or font access. Both functions are inline to avoid a separate
// translation unit while keeping a single definition.
// ─────────────────────────────────────────────────────────────────────────────

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "State.h"

// Returns the system UI message font (Segoe UI on Windows 10/11), DPI-aware.
// The font handle is created once per process and must NOT be destroyed by
// the caller — it lives for the process lifetime.
inline HFONT GetUIFont()
{
    static HFONT hFont = nullptr;
    if (!hFont)
    {
        NONCLIENTMETRICSW ncm{ sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        hFont = CreateFontIndirectW(&ncm.lfMessageFont);
    }
    return hFont;
}

// Sizes the window to the requested client area and positions it above (or
// below if taskbar is at the top) the widget, aligned to the widget's right
// edge, clamped to the nearest monitor's work area.
inline void SetClientSizeAndPosition(HWND hwnd, int clientW, int clientH, State* s)
{
    RECT rc = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc,
        GetWindowLongW(hwnd, GWL_STYLE), FALSE,
        GetWindowLongW(hwnd, GWL_EXSTYLE));

    int winW = rc.right  - rc.left;
    int winH = rc.bottom - rc.top;

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    if (s && s->hwnd)
    {
        RECT widgetRc;
        GetWindowRect(s->hwnd, &widgetRc);

        const int gap = 150; // pixels of clearance above/below the context menu

        x = widgetRc.right - winW;
        y = widgetRc.top - gap - winH;

        // If taskbar is docked at the top, place the dialog below instead
        if (y < 0)
            y = widgetRc.bottom + gap;

        HMONITOR hMon = MonitorFromRect(&widgetRc, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(mi) };
        if (GetMonitorInfoW(hMon, &mi))
        {
            if (x < mi.rcWork.left)               x = mi.rcWork.left;
            if (x + winW > mi.rcWork.right)        x = mi.rcWork.right  - winW;
            if (y < mi.rcWork.top)                 y = mi.rcWork.top;
            if (y + winH > mi.rcWork.bottom)       y = mi.rcWork.bottom - winH;
        }
    }

    SetWindowPos(hwnd, nullptr, x, y, winW, winH, SWP_NOZORDER);
}
