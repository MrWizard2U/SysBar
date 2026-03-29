#pragma once

#pragma warning(disable: 4530)
#pragma warning(disable: 4577)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Compile-time constants
// ─────────────────────────────────────────────────────────────────────────────
static const int  COL_WIDTH    = 70;
static const int  MIN_COLS     = 1;
static const int  MAX_COLS     = 6;
static const int  MAX_PARAMS   = 10;
static const int  DEFAULT_PORT = 8085;

static const DWORD POLL_INTERVAL_MS = 2000;

// Window class names — one constant per registered class.
inline const wchar_t* WC_WIDGET   = L"TaskbarWidget_v2";
inline const wchar_t* WC_SETTINGS = L"SysBar_Settings";
inline const wchar_t* WC_ABOUT    = L"SysBar_About";    // About & Attribution combined

extern const wchar_t* const VIRTUAL_NIC_FRAGMENTS[];
extern const int             VIRTUAL_NIC_FRAGMENT_COUNT;

// ─────────────────────────────────────────────────────────────────────────────
// Sensor parameter identifiers
// Canonical display order — params are always rendered in this sequence.
// Related params are adjacent so they naturally pair into columns.
// ─────────────────────────────────────────────────────────────────────────────
enum ParamId
{
    // ── CPU group ─────────────────────────────────────────────────────────────
    P_CPU_LOAD  = 0,  // pairs with P_CPU_TEMP
    P_CPU_TEMP,       // pairs with P_CPU_LOAD
    P_CPU_CLOCK,      // pairs with P_CPU_POWER
    P_CPU_POWER,      // pairs with P_CPU_CLOCK

    // ── Memory group ──────────────────────────────────────────────────────────
    P_RAM_LOAD,       // pairs with P_RAM_USED
    P_RAM_USED,       // RAM Used GB — pairs with P_RAM_LOAD
    P_RAM_TEMP,       // DIMM temperature

    // ── GPU group ─────────────────────────────────────────────────────────────
    P_GPU_LOAD,       // pairs with P_GPU_TEMP
    P_GPU_TEMP,       // pairs with P_GPU_LOAD
    P_GPU_MEM,        // pairs with P_GPU_POWER
    P_GPU_POWER,      // pairs with P_GPU_MEM

    // ── Disk — fixed pair, second from right ─────────────────────────────────
    P_DISK_READWRITE, // combined Read+Write in one column — single toggle

    // ── Network — fixed pair, ALWAYS rightmost column ─────────────────────────
    P_NET_UPDOWN,     // combined Up+Down in one column — single toggle

    P_COUNT
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-parameter metadata
// ─────────────────────────────────────────────────────────────────────────────
struct ParamDesc
{
    const wchar_t* label;      // top line shown in widget (for single params)
    const wchar_t* menuLabel;  // label shown in Settings checkboxes
    bool           isFixedPair;// true = occupies one full column (Up+Dn / R+W)
};

extern const ParamDesc PARAMS[P_COUNT];

// ─────────────────────────────────────────────────────────────────────────────
// Device descriptor
// ─────────────────────────────────────────────────────────────────────────────
struct DeviceInfo
{
    std::string  path;
    std::wstring name;
};

// ─────────────────────────────────────────────────────────────────────────────
// Discovered sensor ID map
// ─────────────────────────────────────────────────────────────────────────────
struct SensorMap
{
    std::string ids[P_COUNT];  // SensorId per ParamId; empty = sensor absent
    // For P_NET_UPDOWN:     ids[P_NET_UPDOWN]      = upload suffix
    // For P_DISK_READWRITE: ids[P_DISK_READWRITE]  = read SensorId
    std::string netDownId;     // download sensor suffix
    std::string diskWriteId;   // disk write SensorId
    bool        valid = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// Persistent configuration
// ─────────────────────────────────────────────────────────────────────────────
struct Config
{
    int  lhmPort          = DEFAULT_PORT;
    bool enabled[P_COUNT] = {};

    std::string gpuPath;
    std::string diskPath;
    std::string nicPaths;

    Config()
    {
        enabled[P_CPU_LOAD]       = true;
        enabled[P_CPU_TEMP]       = true;
        enabled[P_CPU_CLOCK]      = true;
        enabled[P_CPU_POWER]      = false;
        enabled[P_RAM_LOAD]       = true;
        enabled[P_RAM_USED]       = true;
        enabled[P_RAM_TEMP]       = false;
        enabled[P_GPU_LOAD]       = false;
        enabled[P_GPU_TEMP]       = false;
        enabled[P_GPU_MEM]        = false;
        enabled[P_GPU_POWER]      = false;
        enabled[P_DISK_READWRITE] = false;
        enabled[P_NET_UPDOWN]     = true;
    }

    int enabledCount() const
    {
        int n = 0;
        for (int i = 0; i < P_COUNT; ++i) if (enabled[i]) ++n;
        return n;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations — defined in Config.cpp
// ─────────────────────────────────────────────────────────────────────────────
struct State;
std::wstring GetIniPath();
void         LoadConfig(State* s);
void         SaveConfig(State* s);
