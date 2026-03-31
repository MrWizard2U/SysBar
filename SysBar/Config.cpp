#include "Config.h"
#include "State.h"
#include <shlobj.h>
#include <objbase.h>

#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"user32.lib")

const wchar_t* const VIRTUAL_NIC_FRAGMENTS[] = {
    L"loopback", L"virtual", L"hyper-v", L"vmware", L"vethernet",
    L"bluetooth", L"tunnel",  L"isatap",  L"teredo", L"6to4"
};
const int VIRTUAL_NIC_FRAGMENT_COUNT = (int)(sizeof(VIRTUAL_NIC_FRAGMENTS) / sizeof(VIRTUAL_NIC_FRAGMENTS[0]));

const ParamDesc PARAMS[P_COUNT] =
{
    { L"CPU", L"CPU Load %          [CPU]", false },
    { L"Tmp", L"CPU Temp \u00B0C         [Tmp]", false },
    { L"MHz", L"CPU Clock MHz       [MHz]", false },
    { L"CPW", L"CPU Power (W)[CPW]", false },
    { L"RAM", L"RAM Load %          [RAM]", false },
    { L"RmG", L"RAM Used GB         [RmG]", false },
    { L"RmT", L"RAM Temp \u00B0C[RmT]", false },
    { L"GL",  L"GPU Load %          [GL ]", false },
    { L"GT",  L"GPU Temp \u00B0C         [GT ]", false },
    { L"GM",  L"GPU Memory MB[GM ]", false },
    { L"GW",  L"GPU Power (W)       [GW ]", false },
    { L"Dsk", L"Disk Read+Write[DkR / DkW]", true  },
    { L"Net", L"Network Up+Down   [Up  / Dn ]", true  },
};

static std::wstring NarrowToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

static std::string WideToNarrow(const wchar_t* w)
{
    if (!w || w[0] == L'\0') return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

std::wstring GetIniPath()
{
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData)))
    {
        std::wstring path = localAppData;
        CoTaskMemFree(localAppData);
        path += L"\\TaskbarWidget";
        CreateDirectoryW(path.c_str(), nullptr);
        path += L"\\TaskbarWidget.ini";
        return path;
    }
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring wpath = path;
    size_t dot = wpath.find_last_of(L'.');
    if (dot != std::wstring::npos) wpath.replace(dot, std::wstring::npos, L".ini");
    else wpath += L".ini";
    return wpath;
}

void SaveConfig(State* s)
{
    const std::wstring& p = s->iniPath;

    wchar_t buf[32];
    swprintf_s(buf, L"%d", s->cfg.lhmPort);
    WritePrivateProfileStringW(L"Settings", L"Port", buf, p.c_str());
    WritePrivateProfileStringW(L"Settings", L"AutoStart", s->cfg.autoStart ? L"1" : L"0", p.c_str());

    for (int i = 0; i < P_COUNT; ++i)
    {
        wchar_t key[32];
        swprintf_s(key, L"Param%d", i);
        WritePrivateProfileStringW(L"Params", key, s->cfg.enabled[i] ? L"1" : L"0", p.c_str());
    }

    WritePrivateProfileStringW(L"Devices", L"GPU", NarrowToWide(s->cfg.gpuPath).c_str(), p.c_str());
    WritePrivateProfileStringW(L"Devices", L"Disk", NarrowToWide(s->cfg.diskPath).c_str(), p.c_str());
    WritePrivateProfileStringW(L"Devices", L"NICs", NarrowToWide(s->cfg.nicPaths).c_str(), p.c_str());

    // Write AutoStart to Windows Registry with the delay flag
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        if (s->cfg.autoStart) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring runCmd = std::wstring(L"\"") + exePath + L"\" --autostart";
            DWORD dataSize = static_cast<DWORD>((runCmd.length() + 1) * sizeof(wchar_t));
            RegSetValueExW(hKey, L"SysBar", 0, REG_SZ, reinterpret_cast<const BYTE*>(runCmd.c_str()), dataSize);
        } else {
            RegDeleteValueW(hKey, L"SysBar");
        }
        RegCloseKey(hKey);
    }
}

void LoadConfig(State* s)
{
    const std::wstring& p = s->iniPath;

    s->cfg.lhmPort = (int)GetPrivateProfileIntW(L"Settings", L"Port", DEFAULT_PORT, p.c_str());
    if (s->cfg.lhmPort <= 0 || s->cfg.lhmPort > 65535) s->cfg.lhmPort = DEFAULT_PORT;

    s->cfg.autoStart = GetPrivateProfileIntW(L"Settings", L"AutoStart", 0, p.c_str()) != 0;

    Config def;
    for (int i = 0; i < P_COUNT; ++i)
    {
        wchar_t key[32];
        swprintf_s(key, L"Param%d", i);
        int defVal = def.enabled[i] ? 1 : 0;
        s->cfg.enabled[i] = GetPrivateProfileIntW(L"Params", key, defVal, p.c_str()) != 0;
    }

    wchar_t wbuf[512]{};
    GetPrivateProfileStringW(L"Devices", L"GPU", L"", wbuf, 512, p.c_str());
    s->cfg.gpuPath = WideToNarrow(wbuf);

    GetPrivateProfileStringW(L"Devices", L"Disk", L"", wbuf, 512, p.c_str());
    s->cfg.diskPath = WideToNarrow(wbuf);

    wchar_t wbufNics[2048]{};
    GetPrivateProfileStringW(L"Devices", L"NICs", L"", wbufNics, 2048, p.c_str());
    s->cfg.nicPaths = WideToNarrow(wbufNics);
}