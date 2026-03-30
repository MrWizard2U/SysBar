#pragma warning(disable: 4530)
#pragma warning(disable: 4577)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <thread>

#include "State.h"
#include "Config.h"
#include "Widget.h"
#include "Renderer.h"
#include "Sensor.h"
#include "SettingsDlg.h"
#include "HelpDlg.h"

#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"dcomp.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"comctl32.lib")

static State  g_state;
static HANDLE g_shutdownEvent = nullptr;
static HANDLE g_singleInstMutex = nullptr;

#define IDM_SETTINGS      1001
#define IDM_EXIT          1003
#define IDM_ABOUT         1004  // About + Attribution combined popup
#define IDM_LICENSE       1005  // opens browser
#define IDM_DONATE        1007  // opens browser
#define IDM_SUPPORT       1008  // opens browser (mailto)
#define IDM_INSTRUCTIONS  1009  // opens browser

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    State* s = &g_state;

    if (m == s->wmTaskbarCreated && s->wmTaskbarCreated != 0)
    {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (tray) SetWindowLongPtrW(h, GWLP_HWNDPARENT, (LONG_PTR)tray);

        AppBarUnregister(s);
        AppBarRegister(s);
        if (!s->isMenuOpen) RepositionWidget(s);
        return 0;
    }

    switch (m)
    {
    case WM_CREATE:
        return 0;

    case WM_SIZE:
    {
        UINT nw = LOWORD(l);
        UINT nh = HIWORD(l);
        if (nw > 0 && nh > 0)
            ResizeSwapChain(s, nw, nh);
        return 0;
    }

    case WM_DPICHANGED:
        return 0;

    case WM_DPICHANGED_AFTERPARENT:
    {
        if (!s->isMenuOpen) RepositionWidget(s);
        RECT rc{};
        GetClientRect(h, &rc);
        UINT nw = (UINT)(rc.right - rc.left);
        UINT nh = (UINT)(rc.bottom - rc.top);
        if (nw > 0 && nh > 0) ResizeSwapChain(s, nw, nh);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }

    case WM_USER + 1:
        if (!s->isMenuOpen) RepositionWidget(s);
        return 0;

    case WM_USER + 2:
        if (w == ABN_POSCHANGED && !s->isMenuOpen) RepositionWidget(s);
        return 0;

    case WM_USER + 3:
        SaveConfig(s);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        Draw(s);
        EndPaint(h, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return TRUE;

    case WM_ERASEBKGND:
        return 1;

    case WM_CONTEXTMENU:
    {
        if (s->isMenuOpen) return 0;
        s->isMenuOpen = true;

        HMENU hHelp = CreatePopupMenu();
        AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"About && Attribution");
        AppendMenuW(hHelp, MF_STRING, IDM_LICENSE, L"License");
        AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelp, MF_STRING, IDM_INSTRUCTIONS, L"User Guide");
        AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelp, MF_STRING, IDM_DONATE, L"Donate...");
        AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelp, MF_STRING, IDM_SUPPORT, L"Tech Support");

        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings...");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)hHelp, L"Help");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Close Widget");

        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.hWnd = FindWindowW(L"Shell_TrayWnd", nullptr);
        SHAppBarMessage(ABM_GETTASKBARPOS, &abd);

        int  sx = GET_X_LPARAM(l);
        int  sy = GET_Y_LPARAM(l);
        UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;

        RECT wr{};
        GetWindowRect(h, &wr);

        switch (abd.uEdge)
        {
        case ABE_BOTTOM: default: flags |= TPM_BOTTOMALIGN; sy = wr.top;    break;
        case ABE_TOP:             flags |= TPM_TOPALIGN;    sy = wr.bottom; break;
        case ABE_LEFT:            flags |= TPM_LEFTALIGN;   sx = wr.right;  break;
        case ABE_RIGHT:           flags |= TPM_RIGHTALIGN;  sx = wr.left;   break;
        }

        // We explicitly tell Windows to bring our context menu process to the foreground 
        // so it tracks properly, but we NO LONGER manipulate GWLP_HWNDPARENT here.
        // Manipulating parentage dynamically is what causes the input queue to drop keystrokes into a black hole.
        SetForegroundWindow(h);
        int cmd = TrackPopupMenu(menu, flags, sx, sy, 0, h, nullptr);
        PostMessageW(h, WM_NULL, 0, 0);

        s->isMenuOpen = false;
        DestroyMenu(menu);

        switch (cmd)
        {
        case IDM_SETTINGS:     ShowSettingsDialog(s);  break;
        case IDM_ABOUT:        ShowAboutDialog(s);     break;
        case IDM_LICENSE:      OpenLicense();          break;
        case IDM_INSTRUCTIONS: OpenUserGuide();        break;
        case IDM_DONATE:       OpenDonationPage();     break;
        case IDM_SUPPORT:      OpenTechSupport();      break;
        case IDM_EXIT:         DestroyWindow(h);       break;
        }
        return 0;
    }

    case WM_DESTROY:
        AppBarUnregister(s);
        s->running = false;
        if (g_shutdownEvent) SetEvent(g_shutdownEvent);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int)
{
    State* s = &g_state;
    s->hInst = hInst;

    {
        INITCOMMONCONTROLSEX icex{ sizeof(icex), ICC_LINK_CLASS };
        InitCommonControlsEx(&icex);
    }

    g_singleInstMutex = CreateMutexW(nullptr, FALSE, L"TaskbarWidget_SingleInstance");
    if (!g_singleInstMutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND existing = FindWindowW(WC_WIDGET, nullptr);
        if (existing) SetForegroundWindow(existing);
        CloseHandle(g_singleInstMutex);
        return 0;
    }

    g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_shutdownEvent) { CloseHandle(g_singleInstMutex); return 1; }

    s->wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    s->iniPath = GetIniPath();
    LoadConfig(s);

    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    HWND notify = FindWindowExW(tray, nullptr, L"TrayNotifyWnd", nullptr);
    if (!tray || !notify) return 1;

    RECT tr{}, nr{};
    GetWindowRect(tray, &tr);
    GetWindowRect(notify, &nr);

    s->widgetH = tr.bottom - tr.top;
    if (s->widgetH <= 0) s->widgetH = 40;
    s->widgetW = ComputeWidgetWidth(s);

    int startX = nr.left - s->widgetW;
    if (startX < tr.left) startX = tr.left;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = WC_WIDGET;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    // Assert WS_EX_TOPMOST at creation so it doesn't need to be 
    // dynamically asserted by the background drift guard.
    s->hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW |
        WS_EX_NOACTIVATE |
        WS_EX_NOREDIRECTIONBITMAP |
        WS_EX_TOPMOST,
        WC_WIDGET, L"",
        WS_POPUP | WS_VISIBLE,
        startX, tr.top,
        s->widgetW, s->widgetH,
        nullptr, nullptr, hInst, nullptr);

    if (!s->hwnd) return 1;

    if (tray) SetWindowLongPtrW(s->hwnd, GWLP_HWNDPARENT, (LONG_PTR)tray);

    AppBarRegister(s);
    if (!InitDComp(s))
    {
        AppBarUnregister(s);
        CloseHandle(g_shutdownEvent);
        CloseHandle(g_singleInstMutex);
        return 1;
    }
    RepositionWidget(s);
    Draw(s);

    std::thread sensorThread(SensorThreadProc, s, g_shutdownEvent);

    // FIX: Cleaned up thread message loop to prevent redundant broadcast intercepts 
    // that were bleeding across into child windows.
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    SetEvent(g_shutdownEvent);
    if (sensorThread.joinable()) sensorThread.join();

    CloseHandle(g_shutdownEvent);
    g_shutdownEvent = nullptr;

    CloseHandle(g_singleInstMutex);
    g_singleInstMutex = nullptr;

    return 0;
}