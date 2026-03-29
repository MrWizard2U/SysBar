#include "HelpDlg.h"
#include "Config.h"
#include "DialogUtils.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ─────────────────────────────────────────────────────────────────────────────
// GitHub Pages base URL — update here if the site ever moves
// ─────────────────────────────────────────────────────────────────────────────
static const wchar_t* SITE_BASE = L"https://mrwizard2u.github.io";

// ─────────────────────────────────────────────────────────────────────────────
// Shared control IDs
// ─────────────────────────────────────────────────────────────────────────────
#define IDC_SYSLINK 201

// ─────────────────────────────────────────────────────────────────────────────
// Shared control factory helpers
// ─────────────────────────────────────────────────────────────────────────────
static HWND CreateSysLink(HWND parent, HINSTANCE hInst,
    const wchar_t* text, int x, int y, int w, int h)
{
    HWND hLink = CreateWindowExW(0, WC_LINK, text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, (HMENU)IDC_SYSLINK, hInst, nullptr);
    SendMessageW(hLink, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);
    return hLink;
}

static void CreateOKButton(HWND parent, HINSTANCE hInst, int winW, int y)
{
    HWND hOK = CreateWindowExW(0, L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        (winW - 88) / 2, y, 88, 26,
        parent, (HMENU)IDOK, hInst, nullptr);
    SendMessageW(hOK, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);
}

static LRESULT HandleCtlColor(WPARAM wParam)
{
    SetBkColor((HDC)wParam, RGB(255, 255, 255));
    return (LRESULT)GetStockObject(WHITE_BRUSH);
}

static LRESULT HandleSysLinkClick(LPARAM lParam)
{
    LPNMHDR pnmh = (LPNMHDR)lParam;
    if (pnmh->idFrom == IDC_SYSLINK &&
        (pnmh->code == NM_CLICK || pnmh->code == NM_RETURN))
    {
        PNMLINK pNMLink = (PNMLINK)pnmh;
        ShellExecuteW(nullptr, L"open",
            pNMLink->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
        return 1;
    }
    return 0;
}

// Single registration helper — no-op if class already registered.
static void RegisterDialogClass(const wchar_t* className,
                                WNDPROC        proc,
                                HINSTANCE      hInst)
{
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = proc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = className;
    RegisterClassExW(&wc);
}

// ─────────────────────────────────────────────────────────────────────────────
// About + Attribution — combined popup (remains in-app)
// ─────────────────────────────────────────────────────────────────────────────
static const wchar_t* ABOUT_TEXT =
L"SysBar \u2014 Real-Time Hardware Monitor\r\n"
L"Version 1.1.3 \u2014 Windows 10 1809+ | x64\r\n"
L"Author: Mutant Wizard "
L"(<a href=\"https://mrwizard2u.github.io\">mrwizard2u.github.io</a>)\r\n\r\n"
L"A lightweight, transparent taskbar widget that displays real-time "
L"hardware sensor data alongside your system clock. No ads. No telemetry. "
L"No internet access. All data comes from your local LibreHardwareMonitor "
L"instance.\r\n\r\n"
L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\r\n\r\n"
L"ATTRIBUTION\r\n\r\n"
L"LibreHardwareMonitor\r\n"
L"  Sensor data provider. License: MPL-2.0\r\n"
L"  <a href=\"https://github.com/LibreHardwareMonitor/LibreHardwareMonitor\">"
L"github.com/LibreHardwareMonitor</a>\r\n\r\n"
L"nlohmann/json\r\n"
L"  JSON parsing library. License: MIT\r\n"
L"  <a href=\"https://github.com/nlohmann/json\">github.com/nlohmann/json</a>\r\n\r\n"
L"All other code copyright \u00a9 2026 Mutant Wizard.\r\n\r\n"
L"Full details: "
L"<a href=\"https://mrwizard2u.github.io/sysbar-about.html\">"
L"mrwizard2u.github.io/sysbar-about.html</a>";

static State* s_aboutState = nullptr;

static LRESULT CALLBACK AboutWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l);
        int winW   = 420;
        int winH   = 440;
        int margin = 15;

        SetClientSizeAndPosition(h, winW, winH, s_aboutState);

        int btnH = 26;
        int btnY = winH - btnH - margin;
        CreateOKButton(h, cs->hInstance, winW, btnY);

        int textH = btnY - margin - 10;
        CreateSysLink(h, cs->hInstance, ABOUT_TEXT,
            margin, margin, winW - (margin * 2), textH);
        return 0;
    }
    case WM_NOTIFY:         return HandleSysLinkClick(l);
    case WM_CTLCOLORSTATIC: return HandleCtlColor(w);
    case WM_COMMAND:
        if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) DestroyWindow(h);
        return 0;
    case WM_CLOSE:   DestroyWindow(h); return 0;
    case WM_DESTROY: if (s_aboutState) s_aboutState->hwndAbout = nullptr; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ShowAboutDialog(State* s)
{
    if (s->hwndAbout && IsWindow(s->hwndAbout))
    {
        SetForegroundWindow(s->hwndAbout);
        return;
    }
    RegisterDialogClass(WC_ABOUT, AboutWndProc, s->hInst);
    s_aboutState = s;
    s->hwndAbout = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
        WC_ABOUT, L"SysBar \u2014 About & Attribution",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
        nullptr, nullptr, s->hInst, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Browser-launched pages
// ─────────────────────────────────────────────────────────────────────────────
void OpenUserGuide()
{
    wchar_t url[256];
    wcscpy_s(url, SITE_BASE);
    wcscat_s(url, L"/sysbar-guide.html");
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenLicense()
{
    wchar_t url[256];
    wcscpy_s(url, SITE_BASE);
    wcscat_s(url, L"/license.html");
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenTechSupport()
{
    ShellExecuteW(nullptr, L"open",
        L"mailto:sendtowizzard@gmail.com?subject=SysBar%20Tech%20Support",
        nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenDonationPage()
{
    wchar_t url[256];
    wcscpy_s(url, SITE_BASE);
    wcscat_s(url, L"/donate.html");
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}
