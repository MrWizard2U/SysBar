#include "SettingsDlg.h"
#include "Config.h"
#include "DialogUtils.h"
#include "Widget.h"
#include "Renderer.h"
#include "ThemeUtil.h"
#include <set>

#define IDC_PORT_EDIT       200
#define IDC_SAVE_BTN        201
#define IDC_CANCEL_BTN      202
#define IDC_PARAM_BASE      300
#define IDC_GPU_COMBO       400
#define IDC_DISK_COMBO      401
#define IDC_NIC_LIST        402
#define IDC_SELECT_ALL_NIC  403
#define IDC_CLEAR_NIC       404
#define IDC_STATUS_LABEL    405
#define IDC_AUTOSTART_CHK   406

struct SettingsDlgCtx
{
    State* s;
    std::vector<DeviceInfo> gpus;
    std::vector<DeviceInfo> disks;
    std::vector<DeviceInfo> nics;
};

static void PopulateDeviceCombo(HWND hCombo, const std::vector<DeviceInfo>& devices, const std::string& currentPath, const wchar_t* offlineMsg)
{
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    if (devices.empty()) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)offlineMsg);
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
        return;
    }
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"(none)");
    int selIdx = 0;
    for (int i = 0; i < (int)devices.size(); ++i) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)devices[i].name.c_str());
        if (devices[i].path == currentPath) selIdx = i + 1;
    }
    SendMessageW(hCombo, CB_SETCURSEL, selIdx, 0);
}

static LRESULT CALLBACK SettingsWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SettingsDlgCtx* ctx = reinterpret_cast<SettingsDlgCtx*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        ctx = reinterpret_cast<SettingsDlgCtx*>(cs->lpCreateParams);
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

        ApplyWindowTheme(hDlg);

        HFONT hFont = GetUIFont();
        int y = 10;

        HWND lblPort = CreateWindowExW(0, L"STATIC", L"LHM Port:", WS_CHILD | WS_VISIBLE, 10, y + 3, 70, 18, hDlg, nullptr, cs->hInstance, nullptr);
        SendMessageW(lblPort, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND editPort = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER, 85, y, 60, 22, hDlg, (HMENU)IDC_PORT_EDIT, cs->hInstance, nullptr);
        SendMessageW(editPort, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (IsAppDarkTheme()) SetWindowTheme(editPort, L"DarkMode_Explorer", nullptr);

        wchar_t portBuf[16];
        swprintf_s(portBuf, L"%d", ctx->s->cfg.lhmPort);
        SetWindowTextW(editPort, portBuf);

        HWND lblNote = CreateWindowExW(0, L"STATIC", L"(Default 8085. Match LHM \u2192 Options \u2192 Remote Web Server)", WS_CHILD | WS_VISIBLE, 152, y + 4, 350, 16, hDlg, nullptr, cs->hInstance, nullptr);
        SendMessageW(lblNote, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += 34;

        HWND lblSep = CreateWindowExW(0, L"STATIC", L"\u2500\u2500\u2500 Display Parameters (max 10) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500", WS_CHILD | WS_VISIBLE, 10, y, 490, 16, hDlg, nullptr, cs->hInstance, nullptr);
        SendMessageW(lblSep, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += 20;

        for (int i = 0; i < P_COUNT; ++i)
        {
            int col = i % 2;
            int row = i / 2;
            HWND chk = CreateWindowExW(0, L"BUTTON", PARAMS[i].menuLabel, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10 + col * 250, y + row * 22, 240, 20, hDlg, (HMENU)(UINT_PTR)(IDC_PARAM_BASE + i), cs->hInstance, nullptr);
            SendMessageW(chk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(chk, BM_SETCHECK, ctx->s->cfg.enabled[i] ? BST_CHECKED : BST_UNCHECKED, 0);
            if (IsAppDarkTheme()) SetWindowTheme(chk, L"DarkMode_Explorer", nullptr);
        }
        y += ((P_COUNT + 1) / 2) * 22 + 10;

        HWND lblGpu = CreateWindowExW(0, L"STATIC", L"GPU:", WS_CHILD | WS_VISIBLE, 10, y + 3, 45, 18, hDlg, nullptr, cs->hInstance, nullptr);
        SendMessageW(lblGpu, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND gpuCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 60, y, 440, 200, hDlg, (HMENU)IDC_GPU_COMBO, cs->hInstance, nullptr);
        SendMessageW(gpuCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        // FIX: Win32 Comboboxes must use the Common File Dialog (CFD) theme to render completely dark
        if (IsAppDarkTheme()) SetWindowTheme(gpuCombo, L"DarkMode_CFD", nullptr);
        PopulateDeviceCombo(gpuCombo, ctx->gpus, ctx->s->cfg.gpuPath, L"(LHM offline \u2014 start LHM and reopen Settings)");
        y += 30;

        HWND lblDisk = CreateWindowExW(0, L"STATIC", L"Disk:", WS_CHILD | WS_VISIBLE, 10, y + 3, 45, 18, hDlg, nullptr, cs->hInstance, nullptr);
        SendMessageW(lblDisk, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND diskCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 60, y, 440, 200, hDlg, (HMENU)IDC_DISK_COMBO, cs->hInstance, nullptr);
        SendMessageW(diskCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        // FIX: Win32 Comboboxes must use the Common File Dialog (CFD) theme to render completely dark
        if (IsAppDarkTheme()) SetWindowTheme(diskCombo, L"DarkMode_CFD", nullptr);
        PopulateDeviceCombo(diskCombo, ctx->disks, ctx->s->cfg.diskPath, L"(LHM offline \u2014 start LHM and reopen Settings)");
        y += 30;

        HWND lblNic = CreateWindowExW(0, L"STATIC", L"Network Adapters  (Ctrl+Click to select multiple):", WS_CHILD | WS_VISIBLE, 10, y, 400, 16, hDlg, nullptr, cs->hInstance, nullptr);
        SendMessageW(lblNic, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += 18;

        HWND nicList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_MULTIPLESEL | LBS_NOINTEGRALHEIGHT, 10, y, 490, 72, hDlg, (HMENU)IDC_NIC_LIST, cs->hInstance, nullptr);
        SendMessageW(nicList, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (IsAppDarkTheme()) SetWindowTheme(nicList, L"DarkMode_Explorer", nullptr);

        if (ctx->nics.empty()) {
            SendMessageW(nicList, LB_ADDSTRING, 0, (LPARAM)L"(LHM offline \u2014 start LHM and reopen Settings)");
        }
        else {
            std::set<std::string> selPaths;
            std::string np = ctx->s->cfg.nicPaths;
            size_t pos = 0;
            while (pos < np.size()) {
                size_t semi = np.find(';', pos);
                std::string part = np.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
                if (!part.empty()) selPaths.insert(part);
                pos = (semi == std::string::npos) ? np.size() : semi + 1;
            }
            for (int i = 0; i < (int)ctx->nics.size(); ++i) {
                SendMessageW(nicList, LB_ADDSTRING, 0, (LPARAM)ctx->nics[i].name.c_str());
                if (selPaths.count(ctx->nics[i].path)) SendMessageW(nicList, LB_SETSEL, TRUE, i);
            }
        }
        y += 76;

        HWND btnAll = CreateWindowExW(0, L"BUTTON", L"Select All", WS_CHILD | WS_VISIBLE, 10, y, 90, 22, hDlg, (HMENU)IDC_SELECT_ALL_NIC, cs->hInstance, nullptr);
        SendMessageW(btnAll, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (IsAppDarkTheme()) SetWindowTheme(btnAll, L"DarkMode_Explorer", nullptr);

        HWND btnClear = CreateWindowExW(0, L"BUTTON", L"Clear All", WS_CHILD | WS_VISIBLE, 108, y, 90, 22, hDlg, (HMENU)IDC_CLEAR_NIC, cs->hInstance, nullptr);
        SendMessageW(btnClear, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (IsAppDarkTheme()) SetWindowTheme(btnClear, L"DarkMode_Explorer", nullptr);
        y += 30;

        HWND chkAuto = CreateWindowExW(0, L"BUTTON", L"Run SysBar automatically on Windows startup", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, y, 350, 20, hDlg, (HMENU)IDC_AUTOSTART_CHK, cs->hInstance, nullptr);
        SendMessageW(chkAuto, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(chkAuto, BM_SETCHECK, ctx->s->cfg.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);
        if (IsAppDarkTheme()) SetWindowTheme(chkAuto, L"DarkMode_Explorer", nullptr);
        y += 30;

        HWND lblStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, y, 490, 18, hDlg, (HMENU)IDC_STATUS_LABEL, cs->hInstance, nullptr);
        SendMessageW(lblStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += 24;

        HWND btnSave = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 300, y, 90, 26, hDlg, (HMENU)IDC_SAVE_BTN, cs->hInstance, nullptr);
        SendMessageW(btnSave, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (IsAppDarkTheme()) SetWindowTheme(btnSave, L"DarkMode_Explorer", nullptr);

        HWND btnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 400, y, 90, 26, hDlg, (HMENU)IDC_CANCEL_BTN, cs->hInstance, nullptr);
        SendMessageW(btnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (IsAppDarkTheme()) SetWindowTheme(btnCancel, L"DarkMode_Explorer", nullptr);

        SetClientSizeAndPosition(hDlg, 510, y + 44, ctx->s);
        return 0;
    }

    case WM_ERASEBKGND:
    {
        if (IsAppDarkTheme()) {
            RECT rc;
            GetClientRect(hDlg, &rc);
            FillRect((HDC)wParam, &rc, GetDarkBrush());
            return 1;
        }
        return DefWindowProcW(hDlg, msg, wParam, lParam);
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    {
        if (IsAppDarkTheme()) {
            // FIX: Explicitly check for ComboBoxes to give them the lighter "input field" background
            HWND hCtrl = (HWND)lParam;
            int id = GetDlgCtrlID(hCtrl);
            if (id == IDC_GPU_COMBO || id == IDC_DISK_COMBO) {
                SetTextColor((HDC)wParam, RGB(255, 255, 255));
                SetBkColor((HDC)wParam, RGB(43, 43, 43));
                return (LRESULT)GetDarkInputBrush();
            }

            // Normal text labels and checkboxes get the pure dialog background
            SetTextColor((HDC)wParam, RGB(255, 255, 255));
            SetBkColor((HDC)wParam, RGB(32, 32, 32));
            return (LRESULT)GetDarkBrush();
        }
        break;
    }

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    {
        if (IsAppDarkTheme()) {
            SetTextColor((HDC)wParam, RGB(255, 255, 255));
            SetBkColor((HDC)wParam, RGB(43, 43, 43));
            return (LRESULT)GetDarkInputBrush();
        }
        break;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_SELECT_ALL_NIC) {
            HWND lb = GetDlgItem(hDlg, IDC_NIC_LIST);
            int  cnt = (int)SendMessageW(lb, LB_GETCOUNT, 0, 0);
            for (int i = 0; i < cnt; ++i) SendMessageW(lb, LB_SETSEL, TRUE, i);
            return 0;
        }

        if (id == IDC_CLEAR_NIC) {
            HWND lb = GetDlgItem(hDlg, IDC_NIC_LIST);
            int  cnt = (int)SendMessageW(lb, LB_GETCOUNT, 0, 0);
            for (int i = 0; i < cnt; ++i) SendMessageW(lb, LB_SETSEL, FALSE, i);
            return 0;
        }

        if (id == IDC_CANCEL_BTN) { DestroyWindow(hDlg); return 0; }

        if (id == IDC_SAVE_BTN)
        {
            wchar_t portBuf[16]{};
            GetDlgItemTextW(hDlg, IDC_PORT_EDIT, portBuf, 16);
            int port = _wtoi(portBuf);
            if (port <= 0 || port > 65535) {
                SetDlgItemTextW(hDlg, IDC_STATUS_LABEL, L"Invalid port. Enter a value between 1 and 65535.");
                return 0;
            }

            int normalCount = 0; int fixedCount = 0;
            for (int i = 0; i < P_COUNT; ++i) {
                HWND chk = GetDlgItem(hDlg, IDC_PARAM_BASE + i);
                if (SendMessageW(chk, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    if (PARAMS[i].isFixedPair) ++fixedCount;
                    else                       ++normalCount;
                }
            }
            int colCount = (normalCount + 1) / 2 + fixedCount;
            if (colCount > MAX_COLS) {
                wchar_t msg[128];
                swprintf_s(msg, L"Selection would require %d columns (max %d). Please deselect some parameters.", colCount, MAX_COLS);
                SetDlgItemTextW(hDlg, IDC_STATUS_LABEL, msg);
                return 0;
            }

            ctx->s->cfg.lhmPort = port;
            for (int i = 0; i < P_COUNT; ++i) {
                HWND chk = GetDlgItem(hDlg, IDC_PARAM_BASE + i);
                ctx->s->cfg.enabled[i] = (SendMessageW(chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }

            HWND chkAuto = GetDlgItem(hDlg, IDC_AUTOSTART_CHK);
            ctx->s->cfg.autoStart = (SendMessageW(chkAuto, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (!ctx->gpus.empty()) {
                int sel = (int)SendDlgItemMessageW(hDlg, IDC_GPU_COMBO, CB_GETCURSEL, 0, 0);
                ctx->s->cfg.gpuPath = (sel > 0 && sel - 1 < (int)ctx->gpus.size()) ? ctx->gpus[sel - 1].path : "";
            }

            if (!ctx->disks.empty()) {
                int sel = (int)SendDlgItemMessageW(hDlg, IDC_DISK_COMBO, CB_GETCURSEL, 0, 0);
                ctx->s->cfg.diskPath = (sel > 0 && sel - 1 < (int)ctx->disks.size()) ? ctx->disks[sel - 1].path : "";
            }

            if (!ctx->nics.empty()) {
                HWND lb = GetDlgItem(hDlg, IDC_NIC_LIST);
                int  cnt = (int)SendMessageW(lb, LB_GETCOUNT, 0, 0);
                std::string combined;
                for (int i = 0; i < cnt && i < (int)ctx->nics.size(); ++i) {
                    if (SendMessageW(lb, LB_GETSEL, i, 0) > 0) {
                        if (!combined.empty()) combined += ';';
                        combined += ctx->nics[i].path;
                    }
                }
                ctx->s->cfg.nicPaths = combined;
            }

            SaveConfig(ctx->s);

            if (IsWindow(ctx->s->hwnd)) {
                RepositionWidget(ctx->s);
                ResizeSwapChain(ctx->s, (UINT)ctx->s->widgetW, (UINT)ctx->s->widgetH);
                InvalidateRect(ctx->s->hwnd, nullptr, FALSE);
            }

            DestroyWindow(hDlg);
            return 0;
        }
        return 0;
    }

    case WM_CLOSE: DestroyWindow(hDlg); return 0;
    case WM_DESTROY:
    {
        if (ctx) {
            if (ctx->s) ctx->s->hwndSettings = nullptr;
            delete ctx;
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
        }
        return 0;
    }
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

void ShowSettingsDialog(State* s)
{
    if (s->hwndSettings && IsWindow(s->hwndSettings)) { SetForegroundWindow(s->hwndSettings); return; }

    SettingsDlgCtx* ctx = new SettingsDlgCtx{};
    ctx->s = s;
    {
        std::lock_guard<std::mutex> lk(s->deviceMutex);
        ctx->gpus = s->gpuList; ctx->disks = s->diskList; ctx->nics = s->nicList;
    }

    static bool classReg = false;
    if (!classReg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = s->hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = WC_SETTINGS;
        RegisterClassExW(&wc);
        classReg = true;
    }

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW, WC_SETTINGS, L"SysBar \u2014 Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr, nullptr, s->hInst, ctx);
    if (hDlg) s->hwndSettings = hDlg;
    else      delete ctx;
}