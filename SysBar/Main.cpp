#pragma warning(disable: 4530)
#pragma warning(disable: 4577)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <thread>
#include <vector>

#include "State.h"
#include "Config.h"
#include "Widget.h"
#include "Renderer.h"
#include "Sensor.h"
#include "SettingsDlg.h"
#include "HelpDlg.h"
#include "ThemeUtil.h"
#include "resource.h" // Required to load the IDI_ icon definitions

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

// --- Standardized Context Menu IDs ---
#define IDM_SETTINGS        1001
#define IDM_INSTRUCTIONS    1009
#define IDM_SUPPORT         1008
#define IDM_ABOUT           1004
#define IDM_LICENSE         1005
#define IDM_GOPRO           1012
#define IDM_EXPLORE         1013
#define IDM_DONATE          1007
#define IDM_EXIT            1003

// --- Helper: Converts an HICON to a 32-bpp transparent HBITMAP for native menus ---
static HBITMAP CreateMenuBitmapFromIcon(HICON hIcon, int width = 16, int height = 16)
{
    if (!hIcon) return nullptr;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (hBmp)
    {
        HBITMAP hBmpOld = (HBITMAP)SelectObject(hdcMem, hBmp);
        memset(pBits, 0, width * height * 4); // Ensure memory is cleared (transparent alpha)
        DrawIconEx(hdcMem, 0, 0, hIcon, width, height, 0, nullptr, DI_NORMAL); // Draw the icon
        SelectObject(hdcMem, hBmpOld);
    }

    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return hBmp;
}

// --- Helper: Attaches the bitmap to the menu and tracks it for cleanup ---
static void ApplyMenuIcon(HMENU hMenu, UINT idOrPos, BOOL byPos, HICON hIcon, std::vector<HBITMAP>& tracker)
{
    if (!hIcon) return;
    HBITMAP hBmp = CreateMenuBitmapFromIcon(hIcon);
    if (hBmp)
    {
        tracker.push_back(hBmp);
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_BITMAP;
        mii.hbmpItem = hBmp;
        SetMenuItemInfoW(hMenu, idOrPos, byPos, &mii);
    }
}

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

    case WM_TIMER:
        if (w == 1) {
            KillTimer(h, 1);
            s->isVisible = true;
            RepositionWidget(s);
            ShowWindow(h, SW_SHOWNA);
            return 0;
        }
        break;

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

    case WM_LBUTTONUP:
        ShellExecuteW(nullptr, L"open", L"taskmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
        return 0;

    case WM_CONTEXTMENU:
    {
        if (s->isMenuOpen) return 0;
        s->isMenuOpen = true;

        // 1. Build the Help Flyout (Submenu)
        HMENU hHelp = CreatePopupMenu();
        AppendMenuW(hHelp, MF_STRING, IDM_INSTRUCTIONS, L"User Guide");
        AppendMenuW(hHelp, MF_STRING, IDM_SUPPORT, L"Tech Support");
        AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"About");
        AppendMenuW(hHelp, MF_STRING, IDM_LICENSE, L"License");
        AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelp, MF_STRING, IDM_GOPRO, L"Go Pro");
        AppendMenuW(hHelp, MF_STRING, IDM_EXPLORE, L"Explore More Apps");
        AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelp, MF_STRING, IDM_DONATE, L"Support the Project");

        // 2. Build the Main Context Menu
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings");

        // Append the flyout item. Windows natively adds the ">" arrow to the right.
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)hHelp, L"Help");
        AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

        // 3. Apply Native Bitmaps for perfectly aligned columns
        std::vector<HBITMAP> menuBitmaps; // Tracks created bitmaps to delete them later

        // Load custom icons from resources
        HICON hIconSettings = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_SETTINGS), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconHelp = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_HELP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconGuide = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_USERGUIDE), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconSupport = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_SUPPORT), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconAbout = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_ABOUT), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconLicense = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_LICENSE), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconPro = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_GOPRO), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconExplore = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_MOREAPPS), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconDonate = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_DONATE), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        HICON hIconExit = (HICON)LoadImageW(s->hInst, MAKEINTRESOURCEW(IDI_EXIT), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

        // Apply icons to Main Menu
        ApplyMenuIcon(menu, IDM_SETTINGS, FALSE, hIconSettings, menuBitmaps);
        ApplyMenuIcon(menu, 1, TRUE, hIconHelp, menuBitmaps); // Submenus targeted by index position (Help is at Index 1)
        ApplyMenuIcon(menu, IDM_EXIT, FALSE, hIconExit, menuBitmaps);

        // Apply icons to Help Submenu
        ApplyMenuIcon(hHelp, IDM_INSTRUCTIONS, FALSE, hIconGuide, menuBitmaps);
        ApplyMenuIcon(hHelp, IDM_SUPPORT, FALSE, hIconSupport, menuBitmaps);
        ApplyMenuIcon(hHelp, IDM_ABOUT, FALSE, hIconAbout, menuBitmaps);
        ApplyMenuIcon(hHelp, IDM_LICENSE, FALSE, hIconLicense, menuBitmaps);
        ApplyMenuIcon(hHelp, IDM_GOPRO, FALSE, hIconPro, menuBitmaps);
        ApplyMenuIcon(hHelp, IDM_EXPLORE, FALSE, hIconExplore, menuBitmaps);
        ApplyMenuIcon(hHelp, IDM_DONATE, FALSE, hIconDonate, menuBitmaps);

        // --- Execute Menu ---
        APPBARDATA abd{ sizeof(abd) };
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

        SetForegroundWindow(h);
        int cmd = TrackPopupMenu(menu, flags, sx, sy, 0, h, nullptr);
        PostMessageW(h, WM_NULL, 0, 0);

        // 4. Cleanup Memory
        s->isMenuOpen = false;
        DestroyMenu(menu);
        for (HBITMAP bmp : menuBitmaps) {
            DeleteObject(bmp);
        }

        // 5. Handle Clicks
        switch (cmd)
        {
        case IDM_SETTINGS:     ShowSettingsDialog(s);  break;
        case IDM_INSTRUCTIONS: OpenUserGuide();        break;
        case IDM_SUPPORT:      OpenTechSupport();      break;
        case IDM_ABOUT:        ShowAboutDialog(s);     break;
        case IDM_LICENSE:      OpenLicense();          break;
        case IDM_DONATE:       OpenDonationPage();     break;
        case IDM_EXPLORE:
            ShellExecuteW(nullptr, L"open", L"https://mrwizard2u.github.io/", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case IDM_GOPRO:
            MessageBoxW(h, L"Pro features activation coming soon to the MS Store.", L"Go Pro", MB_OK | MB_ICONINFORMATION);
            break;
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
    InitAppTheme();

    State* s = &g_state;
    s->hInst = hInst;

    if (wcsstr(GetCommandLineW(), L"--autostart") != nullptr) {
        s->isVisible = false;
    }
    else {
        s->isVisible = true;
    }

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
    if (!tray) return 1;

    HWND notify = FindWindowExW(tray, nullptr, L"TrayNotifyWnd", nullptr);

    RECT tr{}, nr{};
    GetWindowRect(tray, &tr);
    if (notify) GetWindowRect(notify, &nr);

    s->widgetH = tr.bottom - tr.top;
    if (s->widgetH <= 0) s->widgetH = 40;
    s->widgetW = ComputeWidgetWidth(s);

    int startX = notify ? (nr.left - s->widgetW) : (tr.right - s->widgetW);
    if (startX < tr.left) startX = tr.left;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = WC_WIDGET;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    s->hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST,
        WC_WIDGET, L"",
        WS_POPUP | (s->isVisible ? WS_VISIBLE : 0),
        startX, tr.top, s->widgetW, s->widgetH, nullptr, nullptr, hInst, nullptr);

    if (!s->hwnd) return 1;
    if (tray) SetWindowLongPtrW(s->hwnd, GWLP_HWNDPARENT, (LONG_PTR)tray);

    if (!s->isVisible) {
        SetTimer(s->hwnd, 1, 5000, nullptr);
    }

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