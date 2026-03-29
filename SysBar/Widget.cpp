#include "Widget.h"
#include "Config.h"
#include <cmath>

#pragma comment(lib,"shell32.lib")

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
    return cols * COL_WIDTH;
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

    int taskH = tr.bottom - tr.top;
    if (taskH <= 0) taskH = 40;

    s->widgetH = taskH;
    s->widgetW = ComputeWidgetWidth(s);

    int x = nr.left - s->widgetW;
    if (x < tr.left) x = tr.left;
    int y = tr.top;

    if (isVertical)
    {
        y = nr.top - s->widgetH;
        if (y < tr.top) y = tr.top;
    }

    RECT cr{};
    GetWindowRect(s->hwnd, &cr);

    // CRITICAL FIX: Evaluate physical differences with a 1-pixel tolerance to 
    // entirely avoid infinite DPI fractional scaling loops on 125%/150% monitors.
    int dx = abs(cr.left - x);
    int dy = abs(cr.top - y);
    int dw = abs((cr.right - cr.left) - s->widgetW);
    int dh = abs((cr.bottom - cr.top) - s->widgetH);

    bool posChanged = (dx > 1 || dy > 1 || dw > 1 || dh > 1);
    bool rectEmpty = IsRectEmpty(&s->appbar.abd.rc);

    // If the widget is already exactly where it's supposed to be, ABORT.
    // Do NOT touch SetWindowPos. Doing so on a timer interrupts the OS input queue, 
    // causing the foreground window to randomly drop keyboard focus ("Focus Black Hole").
    if (posChanged || rectEmpty)
    {
        // Use SWP_NOZORDER so it preserves its established topmost state 
        // without commanding the Window Manager to re-evaluate the Z-stack.
        SetWindowPos(s->hwnd, nullptr,
            x, y, s->widgetW, s->widgetH,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);

        if (s->appbar.registered)
        {
            s->appbar.abd.rc = { x, y, x + s->widgetW, y + s->widgetH };
            SHAppBarMessage(ABM_SETPOS, &s->appbar.abd);
        }
    }
}