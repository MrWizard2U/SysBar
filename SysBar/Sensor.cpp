#include "Sensor.h"
#include "Config.h"

#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <set>
#include <cctype>

#include "json.hpp"

#pragma comment(lib,"winhttp.lib")

using json = nlohmann::json;

// Tightened network limits to prevent thread stalling if LHM dies
static const DWORD  HTTP_CONNECT_TIMEOUT = 1000;
static const DWORD  HTTP_SEND_TIMEOUT = 1000;
static const DWORD  HTTP_RECEIVE_TIMEOUT = 1000;
static const size_t MAX_BODY = 4 * 1024 * 1024;

// ─────────────────────────────────────────────────────────────────────────────
// String utilities
// ─────────────────────────────────────────────────────────────────────────────
static double ParseDouble(const std::string& s)
{
    if (s.empty()) return 0.0;
    try { return std::stod(s); }
    catch (...) { return 0.0; }
}

static std::wstring ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

static bool ContainsCI(const std::string& hay, const std::string& needle)
{
    if (needle.empty()) return true;
    if (needle.size() > hay.size()) return false;
    const size_t hLen = hay.size();
    const size_t nLen = needle.size();
    for (size_t i = 0; i <= hLen - nLen; ++i)
    {
        size_t j = 0;
        while (j < nLen &&
            ::tolower((unsigned char)hay[i + j]) ==
            ::tolower((unsigned char)needle[j]))
            ++j;
        if (j == nLen) return true;
    }
    return false;
}

static bool IsVirtualNic(const std::wstring& name)
{
    std::wstring lower = name;
    for (auto& c : lower) c = towlower(c);
    for (int i = 0; i < VIRTUAL_NIC_FRAGMENT_COUNT; ++i)
        if (lower.find(VIRTUAL_NIC_FRAGMENTS[i]) != std::wstring::npos)
            return true;
    return false;
}

static bool HwIdStartsWith(const std::string& id, const char* prefix)
{
    return id.find(prefix) == 0;
}

static std::string ClassifyHardware(const std::string& hwId)
{
    if (HwIdStartsWith(hwId, "/intelcpu/") ||
        HwIdStartsWith(hwId, "/amdcpu/"))           return "cpu";
    if (HwIdStartsWith(hwId, "/gpu-intel-integrated/") ||
        HwIdStartsWith(hwId, "/gpu-nvidia/") ||
        HwIdStartsWith(hwId, "/gpu-amd/"))           return "gpu";
    if (HwIdStartsWith(hwId, "/ram") ||
        HwIdStartsWith(hwId, "/vram")) return "ram";
    if (HwIdStartsWith(hwId, "/memory/dimm/"))       return "dimm";
    if (HwIdStartsWith(hwId, "/nic/"))               return "nic";
    if (HwIdStartsWith(hwId, "/nvme/") ||
        HwIdStartsWith(hwId, "/hdd/") ||
        HwIdStartsWith(hwId, "/ssd/"))               return "disk";
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP fetch
// ─────────────────────────────────────────────────────────────────────────────
std::string FetchJsonRaw(int port)
{
    std::string result;
    auto closeH = [](HINTERNET h) { if (h) WinHttpCloseHandle(h); };

    HINTERNET hSession = WinHttpOpen(L"TaskbarWidget/2.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};

    WinHttpSetTimeouts(hSession,
        (int)HTTP_CONNECT_TIMEOUT, (int)HTTP_CONNECT_TIMEOUT,
        (int)HTTP_SEND_TIMEOUT, (int)HTTP_RECEIVE_TIMEOUT);

    HINTERNET hConnect = WinHttpConnect(
        hSession, L"127.0.0.1", (INTERNET_PORT)port, 0);
    if (!hConnect) { closeH(hSession); return {}; }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", L"/data.json", nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { closeH(hConnect); closeH(hSession); return {}; }

    bool ok =
        WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr);

    if (ok)
    {
        result.reserve(64 * 1024);
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(hRequest, &available) && available)
        {
            if (result.size() + available > MAX_BODY) break;
            size_t offset = result.size();
            result.resize(offset + available);
            DWORD downloaded = 0;
            if (!WinHttpReadData(hRequest,
                reinterpret_cast<LPVOID>(&result[offset]),
                available, &downloaded))
            {
                result.clear(); break;
            }
            result.resize(offset + downloaded);
        }
    }

    closeH(hRequest); closeH(hConnect); closeH(hSession);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON parse
// ─────────────────────────────────────────────────────────────────────────────
ParseResult ParseLhmJson(const std::string& rawJson)
{
    ParseResult out;
    json root;
    try { root = json::parse(rawJson); }
    catch (...) { return out; }

    struct Frame { const json* node; int hwIdx; };
    std::vector<Frame> stack;
    stack.reserve(256);
    stack.push_back({ &root, -1 });

    while (!stack.empty())
    {
        Frame frame = stack.back();
        stack.pop_back();

        const json* node = frame.node;
        int         hwIdx = frame.hwIdx;

        if (!node->is_object()) continue;

        if (node->contains("HardwareId"))
        {
            std::string thisHwId = (*node)["HardwareId"].get<std::string>();
            std::string nameStr = node->value("Text", "");
            std::string category = ClassifyHardware(thisHwId);
            std::wstring wname = ToWide(nameStr);

            HardwareNode hw;
            hw.hwId = thisHwId;
            hw.name = nameStr;
            hw.hwType = category;

            if (category == "gpu")
                out.gpus.push_back({ thisHwId, wname });
            else if (category == "disk")
                out.disks.push_back({ thisHwId, wname });
            else if (category == "nic" && !IsVirtualNic(wname))
                out.nics.push_back({ thisHwId, wname });

            out.hardware.push_back(std::move(hw));
            hwIdx = (int)out.hardware.size() - 1;
        }

        if (node->contains("SensorId"))
        {
            std::string sid = (*node)["SensorId"].get<std::string>();
            std::string type = node->value("Type", "");
            std::string text = node->value("Text", "");
            std::string rv = node->value("RawValue", "");
            double      val = ParseDouble(rv);

            out.sensorValues[sid] = val;

            if (hwIdx >= 0 && hwIdx < (int)out.hardware.size())
                out.hardware[hwIdx].sensors.push_back({ sid, type, text, val });
        }

        if (node->contains("Children"))
        {
            const json& ch = (*node)["Children"];
            if (ch.is_array())
                for (auto it = ch.rbegin(); it != ch.rend(); ++it)
                    stack.push_back({ &(*it), hwIdx });
        }
    }

    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Discovery helpers
// ─────────────────────────────────────────────────────────────────────────────
static std::string FindSensor(
    const HardwareNode& hw,
    const std::string& type,
    const std::vector<std::string>& textFragments,
    const std::vector<std::string>& excludeFragments = {})
{
    for (auto& frag : textFragments)
    {
        for (auto& s : hw.sensors)
        {
            if (s.type != type) continue;
            if (!ContainsCI(s.text, frag)) continue;
            bool excluded = false;
            for (auto& ex : excludeFragments)
                if (ContainsCI(s.text, ex)) { excluded = true; break; }
            if (!excluded) return s.sensorId;
        }
    }
    return {};
}

static std::string FindFirstSensor(const HardwareNode& hw, const std::string& type)
{
    for (auto& s : hw.sensors)
        if (s.type == type) return s.sensorId;
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Sensor discovery
// ─────────────────────────────────────────────────────────────────────────────
SensorMap DiscoverSensors(const ParseResult& pr, const Config& cfg)
{
    SensorMap sm;

    for (auto& hw : pr.hardware)
    {
        if (hw.hwType != "cpu") continue;
        sm.ids[P_CPU_LOAD] = FindSensor(hw, "Load", { "Total" });
        sm.ids[P_CPU_TEMP] = FindSensor(hw, "Temperature", { "P-Core #1", "Tdie", "Core #1", "CPU Core", "Package" });
        sm.ids[P_CPU_CLOCK] = FindSensor(hw, "Clock", { "P-Core #1", "Core #1", "CPU Core" }, { "Bus" });
        sm.ids[P_CPU_POWER] = FindSensor(hw, "Power", { "Package" });
        break;
    }

    for (auto& hw : pr.hardware)
    {
        if (!HwIdStartsWith(hw.hwId, "/ram")) continue;
        sm.ids[P_RAM_LOAD] = FindFirstSensor(hw, "Load");
        sm.ids[P_RAM_USED] = FindSensor(hw, "Data", { "Memory Used", "Used" });
        break;
    }

    for (auto& hw : pr.hardware)
    {
        if (hw.hwType != "dimm") continue;
        sm.ids[P_RAM_TEMP] = FindFirstSensor(hw, "Temperature");
        if (!sm.ids[P_RAM_TEMP].empty()) break;
    }

    for (auto& hw : pr.hardware)
    {
        if (hw.hwType != "gpu") continue;
        if (!cfg.gpuPath.empty() && hw.hwId != cfg.gpuPath) continue;

        sm.ids[P_GPU_LOAD] = FindSensor(hw, "Load", { "3D" });
        if (sm.ids[P_GPU_LOAD].empty())
            sm.ids[P_GPU_LOAD] = FindFirstSensor(hw, "Load");

        sm.ids[P_GPU_TEMP] = FindFirstSensor(hw, "Temperature");
        sm.ids[P_GPU_MEM] = FindFirstSensor(hw, "SmallData");
        sm.ids[P_GPU_POWER] = FindFirstSensor(hw, "Power");
        break;
    }

    for (auto& hw : pr.hardware)
    {
        if (hw.hwType != "nic") continue;

        std::string up = FindSensor(hw, "Throughput", { "Upload",   "Up" });
        std::string down = FindSensor(hw, "Throughput", { "Download", "Down" });

        if (!up.empty() && !down.empty())
        {
            auto suffix = [](const std::string& full,
                const std::string& prefix) -> std::string
                {
                    if (full.find(prefix) == 0) return full.substr(prefix.size());
                    return full;
                };
            sm.ids[P_NET_UPDOWN] = suffix(up, hw.hwId);
            sm.netDownId = suffix(down, hw.hwId);
            break;
        }
    }

    for (auto& hw : pr.hardware)
    {
        if (hw.hwType != "disk") continue;
        if (!cfg.diskPath.empty() && hw.hwId != cfg.diskPath) continue;

        sm.ids[P_DISK_READWRITE] = FindSensor(hw, "Throughput", { "Read" });
        sm.diskWriteId = FindSensor(hw, "Throughput", { "Write" });
        break;
    }

    sm.valid = true;
    return sm;
}

// ─────────────────────────────────────────────────────────────────────────────
// Value extraction
// ─────────────────────────────────────────────────────────────────────────────
void ExtractValues(State* s, const ParseResult& pr)
{
    double vals[P_COUNT] = {};
    bool   found[P_COUNT] = {};

    auto lookup = [&](const std::string& sid) -> std::pair<bool, double>
        {
            if (sid.empty()) return { false, 0.0 };
            auto it = pr.sensorValues.find(sid);
            if (it != pr.sensorValues.end()) return { true, it->second };
            return { false, 0.0 };
        };

    std::lock_guard<std::mutex> lk(s->dataMutex);

    for (int i = 0; i < P_COUNT; ++i)
    {
        if (i == P_NET_UPDOWN || i == P_DISK_READWRITE) continue;
        auto [f, v] = lookup(s->sensorMap.ids[i]);
        found[i] = f;
        vals[i] = v;
    }

    const std::string& upSfx = s->sensorMap.ids[P_NET_UPDOWN];
    const std::string& downSfx = s->sensorMap.netDownId;

    auto aggregateNics = [&](const std::string& paths)
        {
            size_t pos = 0;
            while (pos < paths.size())
            {
                size_t semi = paths.find(';', pos);
                std::string prefix = paths.substr(pos,
                    semi == std::string::npos ? std::string::npos : semi - pos);
                pos = (semi == std::string::npos) ? paths.size() : semi + 1;
                if (prefix.empty()) continue;
                if (!upSfx.empty())
                {
                    auto [f, v] = lookup(prefix + upSfx);
                    if (f) { found[P_NET_UPDOWN] = true; vals[P_NET_UPDOWN] += v; }
                }
                if (!downSfx.empty())
                {
                    auto [f, v] = lookup(prefix + downSfx);
                    if (f) { s->netDownFound = true; s->netDown += v; }
                }
            }
        };

    s->netDown = 0;
    s->netDownFound = false;

    if (!s->cfg.nicPaths.empty())
        aggregateNics(s->cfg.nicPaths);
    else
        for (auto& nic : pr.nics)
        {
            if (!upSfx.empty())
            {
                auto [f, v] = lookup(nic.path + upSfx);
                if (f) { found[P_NET_UPDOWN] = true; vals[P_NET_UPDOWN] += v; }
            }
            if (!downSfx.empty())
            {
                auto [f, v] = lookup(nic.path + downSfx);
                if (f) { s->netDownFound = true; s->netDown += v; }
            }
        }

    s->diskWrite = 0;
    s->diskWriteFound = false;

    if (!s->cfg.diskPath.empty())
    {
        {
            auto [f, v] = lookup(s->sensorMap.ids[P_DISK_READWRITE]);
            found[P_DISK_READWRITE] = f; vals[P_DISK_READWRITE] = v;
        }
        {
            auto [f, v] = lookup(s->sensorMap.diskWriteId);
            s->diskWriteFound = f; s->diskWrite = v;
        }
    }

    for (int i = 0; i < P_COUNT; ++i)
    {
        s->values[i] = vals[i];
        s->sensorFound[i] = found[i];
    }
    s->dataValid = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Value formatting (strict maximum 4 characters)
// ─────────────────────────────────────────────────────────────────────────────
std::wstring FormatValue(ParamId id, double v, bool sensorPresent)
{
    if (!sensorPresent) return L"N/A";

    wchar_t b[32]{};
    switch (id)
    {
    case P_CPU_LOAD:  swprintf_s(b, L"%d%%", (int)v); break;
    case P_CPU_TEMP:  swprintf_s(b, L"%d\u00B0", (int)v); break;
    case P_CPU_CLOCK:
        if (v >= 9999.0) swprintf_s(b, L"9999");
        else             swprintf_s(b, L"%d", (int)v);
        break;
    case P_CPU_POWER:
        if (v >= 999.5)  swprintf_s(b, L"999W");
        else             swprintf_s(b, L"%dW", (int)v);
        break;

    case P_RAM_LOAD:  swprintf_s(b, L"%d%%", (int)v); break;
    case P_RAM_USED:
        if (v >= 99.5)   swprintf_s(b, L"99GB");
        else             swprintf_s(b, L"%.0fGB", v);
        break;
    case P_RAM_TEMP:  swprintf_s(b, L"%d\u00B0", (int)v); break;

    case P_GPU_LOAD:  swprintf_s(b, L"%d%%", (int)v); break;
    case P_GPU_TEMP:  swprintf_s(b, L"%d\u00B0", (int)v); break;
    case P_GPU_MEM:
        if (v >= 10234.88)   swprintf_s(b, L"%.0fG", v / 1024.0); // 10G
        else if (v >= 999.5) swprintf_s(b, L"%.1fG", v / 1024.0); // 1.0G
        else                 swprintf_s(b, L"%.0fM", v);          // 999M
        break;
    case P_GPU_POWER:
        if (v >= 999.5)      swprintf_s(b, L"999W");
        else if (v >= 99.5)  swprintf_s(b, L"%dW", (int)v);
        else                 swprintf_s(b, L"%.1fW", v);
        break;

    default: swprintf_s(b, L"--"); break;
    }
    return b;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sensor thread
// ─────────────────────────────────────────────────────────────────────────────
void SensorThreadProc(State* s, HANDLE shutdownEvent)
{
    unsigned int pollCount = 0;

    while (s->running)
    {
        std::string raw = FetchJsonRaw(s->cfg.lhmPort);

        if (!raw.empty())
        {
            ParseResult pr = ParseLhmJson(raw);

            if (pollCount % 5 == 0)
            {
                std::lock_guard<std::mutex> lk(s->deviceMutex);
                s->gpuList = pr.gpus;
                s->diskList = pr.disks;
                s->nicList = pr.nics;
                s->devicesReady = true;
            }

            if (s->cfg.nicPaths.empty() && !pr.nics.empty())
            {
                std::string combined;
                for (auto& n : pr.nics)
                {
                    if (!combined.empty()) combined += ';';
                    combined += n.path;
                }
                {
                    std::lock_guard<std::mutex> lk(s->dataMutex);
                    s->cfg.nicPaths = combined;
                }
                if (IsWindow(s->hwnd))
                    PostMessageW(s->hwnd, WM_USER + 3, 0, 0);
            }

            bool needDiscover = false;
            {
                std::lock_guard<std::mutex> lk(s->dataMutex);
                needDiscover = !s->sensorMap.valid || (pollCount % 30 == 0);
            }

            if (needDiscover)
            {
                Config cfgSnapshot;
                {
                    std::lock_guard<std::mutex> lk(s->dataMutex);
                    cfgSnapshot = s->cfg;
                }
                auto newMap = DiscoverSensors(pr, cfgSnapshot);
                {
                    std::lock_guard<std::mutex> lk(s->dataMutex);
                    s->sensorMap = newMap;
                }
            }

            ExtractValues(s, pr);
        }
        else
        {
            std::lock_guard<std::mutex> lk(s->dataMutex);
            s->dataValid = false;
        }

        bool expected = false;
        if (s->drawPending.compare_exchange_strong(expected, true) &&
            IsWindow(s->hwnd))
            InvalidateRect(s->hwnd, nullptr, FALSE);

        if (pollCount % (5000 / POLL_INTERVAL_MS) == 0 && IsWindow(s->hwnd))
            PostMessageW(s->hwnd, WM_USER + 1, 0, 0);

        ++pollCount;

        if (WaitForSingleObject(shutdownEvent, POLL_INTERVAL_MS) == WAIT_OBJECT_0)
            break;
    }
}