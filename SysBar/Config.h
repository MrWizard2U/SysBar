#pragma once

#pragma warning(disable: 4530)
#pragma warning(disable: 4577)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>

static const int  COL_WIDTH    = 70;
static const int  MIN_COLS     = 1;
static const int  MAX_COLS     = 6;
static const int  MAX_PARAMS   = 10;
static const int  DEFAULT_PORT = 8085;

static const DWORD POLL_INTERVAL_MS = 2000;

inline const wchar_t* WC_WIDGET   = L"TaskbarWidget_v2";
inline const wchar_t* WC_SETTINGS = L"SysBar_Settings";
inline const wchar_t* WC_ABOUT    = L"SysBar_About";

extern const wchar_t* const VIRTUAL_NIC_FRAGMENTS[];
extern const int             VIRTUAL_NIC_FRAGMENT_COUNT;

enum ParamId
{
    P_CPU_LOAD  = 0,
    P_CPU_TEMP,     
    P_CPU_CLOCK,    
    P_CPU_POWER,    
    P_RAM_LOAD,     
    P_RAM_USED,     
    P_RAM_TEMP,     
    P_GPU_LOAD,     
    P_GPU_TEMP,     
    P_GPU_MEM,      
    P_GPU_POWER,    
    P_DISK_READWRITE,
    P_NET_UPDOWN,    
    P_COUNT
};

struct ParamDesc
{
    const wchar_t* label;
    const wchar_t* menuLabel;
    bool           isFixedPair;
};

extern const ParamDesc PARAMS[P_COUNT];

struct DeviceInfo
{
    std::string  path;
    std::wstring name;
};

struct SensorMap
{
    std::string ids[P_COUNT]; 
    std::string netDownId;   
    std::string diskWriteId; 
    bool        valid = false;
};

struct Config
{
    int  lhmPort          = DEFAULT_PORT;
    bool enabled[P_COUNT] = {};
    bool autoStart        = false; // Added Auto-Start

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

struct State;
std::wstring GetIniPath();
void         LoadConfig(State* s);
void         SaveConfig(State* s);