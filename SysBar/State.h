#pragma once

#pragma warning(disable: 4530)
#pragma warning(disable: 4577)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shellapi.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

#include "Config.h"

using Microsoft::WRL::ComPtr;

// ─────────────────────────────────────────────────────────────────────────────
// AppBar registration state
// ─────────────────────────────────────────────────────────────────────────────
struct AppBarState
{
    APPBARDATA abd{};
    bool       registered = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// Full application state — passed by pointer to all modules
// ─────────────────────────────────────────────────────────────────────────────
struct State
{
    // Window handles
    HWND hwnd{};  // main widget window
    HWND hwndSettings{};  // Settings dialog (in-app popup)
    HWND hwndAbout{};  // About & Attribution dialog (in-app popup)
    // User Guide, License and Tech Support open in the system browser —
    // no HWND tracking needed for those.

    // Widget geometry (pixels)
    int widgetH{};
    int widgetW{};

    // Shell integration
    AppBarState  appbar;
    UINT         wmTaskbarCreated{};  // RegisterWindowMessage result

    // Menu safety flag to prevent background repositioning from deadlocking input
    std::atomic<bool> isMenuOpen{ false };

    // Configuration (loaded from .ini, modified via Settings dialog)
    Config       cfg;
    std::wstring iniPath;

    // Discovered sensor ID map — populated by DiscoverSensors() in Sensor.cpp
    // Protected by dataMutex. Re-discovered periodically.
    SensorMap sensorMap;

    // Live sensor values indexed by ParamId; protected by dataMutex
    double       values[P_COUNT] = {};
    bool         sensorFound[P_COUNT] = {};  // true = LHM reported this sensor
    bool         dataValid = false;

    // Extra values for fixed-pair params (Net Down, Disk Write)
    // stored separately since they share a ParamId slot with their pair
    double netDown = 0; bool netDownFound = false;
    double diskWrite = 0; bool diskWriteFound = false;

    std::mutex   dataMutex;

    // Device lists discovered from LHM JSON; protected by deviceMutex
    std::vector<DeviceInfo> gpuList;
    std::vector<DeviceInfo> diskList;
    std::vector<DeviceInfo> nicList;  // non-virtual only
    std::mutex              deviceMutex;
    bool                    devicesReady = false;

    // Sensor thread control
    std::atomic<bool> running{ true };
    std::atomic<bool> drawPending{ false };

    // Win32 / HINSTANCE
    HINSTANCE hInst{};

    // DirectX / DirectComposition resources
    ComPtr<ID3D11Device>         d3d;
    ComPtr<IDXGIDevice>          dxgi;
    ComPtr<IDCompositionDevice>  dcomp;
    ComPtr<IDCompositionTarget>  target;
    ComPtr<IDCompositionVisual>  visual;
    ComPtr<IDXGISwapChain1>      swapChain;
    ComPtr<ID2D1Factory1>        d2dFactory;
    ComPtr<ID2D1Device>          d2dDevice;
    ComPtr<ID2D1DeviceContext>   ctx;
    ComPtr<ID2D1SolidColorBrush> textBrush;
    ComPtr<IDWriteFactory>       dwFactory;
    ComPtr<IDWriteTextFormat>    textFormat;
};