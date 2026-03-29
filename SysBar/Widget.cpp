#include "Widget.h"
#include "Config.h"
#include <cmath>

#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"user32.lib")

int ComputeWidgetWidth(State* s)
{
    int normal = 0;
    int fixed = 0;
    for (int i = 0; i < P_COUNT; ++i)
    {
        if (!s->cfg.enabled[i]) continue;
        if (PARAMS[i].isFixedPair) ++fixed;
        else                       ++normal;
    }

    int cols = (normal + 1) / 2 + fixed;
    if (cols < MIN_COLS) cols = MIN_COLS;
    if (cols > MAX_COLS) cols = MAX_COLS;

    // CRITICAL FIX: Scale column width strictly by the monitor's DPI setting.
    // Previously, this used the taskbar height which artificially inflated the gap
    // on Windows 11 (which uses a 48px base height vs Win10's 40px base height).
    UINT dpi = 96;
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray)
    {
        dpi = GetDpiForWindow(tray);
        if (dpi == 0) dpi = 96;
    }

    int scaledColWidth = (COL_WIDTH * (int)dpi) / 96;

    return cols * scaledColWidth;
}

void AppBarRegister(State* s)
{
    if (s->appbar.registered) return;

    ZeroMemory(&s->appbar.abd, sizeof(s->appbar.abd));
    s->appbar.abd.cbSize = sizeof(APPBARDATA);
    s->appbar.abd.hWnd = s->hwnd;
    s->appbar.abd.uCallbackMessage = WM_USER + 2;

    SHAppBarMessage(ABM_NEW, &s->appbar.abd);
    s->appbar.registered = true;
}

void AppBarUnregister(State* s)
{
    if (!s->appbar.registered) return;

    s->appbar.abd.cbSize = sizeof(APPBARDATA);
    s->appbar.abd.hWnd = s->hwnd;
    SHAppBarMessage(ABM_REMOVE, &s->appbar.abd);
    s->appbar.registered = false;
}

void RepositionWidget(State* s)
{
    if (s->isMenuOpen) return;

    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    HWND notify = FindWindowExW(tray, nullptr, L"TrayNotifyWnd", nullptr);
    if (!tray || !notify) return;

    RECT tr{}, nr{};
    GetWindowRect(tray, &tr);
    GetWindowRect(notify, &nr);

    bool isVertical = (tr.bottom - tr.top) > (tr.right - tr.left);

    int targetH = tr.bottom - tr.top;
    if (targetH <= 0) targetH = 40;

    s->widgetH = targetH;
    int targetW = ComputeWidgetWidth(s);

    int x = nr.left - targetW;
    if (x < tr.left) x = tr.left;
    int y = tr.top;

    if (isVertical)
    {
        y = nr.top - targetH;
        if (y < tr.top) y = tr.top;
    }

    RECT cr{};
    GetWindowRect(s->hwnd, &cr);

    int dx = std::abs(cr.left - x);
    int dy = std::abs(cr.top - y);
    int dw = std::abs((cr.right - cr.left) - targetW);
    int dh = std::abs((cr.bottom - cr.top) - targetH);

    bool posChanged = (dx > 1 || dy > 1 || dw > 1 || dh > 1);
    bool isTopmost = (GetWindowLongW(s->hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    bool rectEmpty = IsRectEmpty(&s->appbar.abd.rc);

    if (posChanged || !isTopmost || rectEmpty)
    {
        SetWindowPos(s->hwnd, nullptr,
            x, y, targetW, targetH,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);

        if (s->appbar.registered)
        {
            s->appbar.abd.rc = { x, y, x + targetW, y + targetH };
            SHAppBarMessage(ABM_SETPOS, &s->appbar.abd);
        }
    }
}