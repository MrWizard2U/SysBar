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

struct AppBarState
{
    APPBARDATA abd{};
    bool       registered = false;
};

struct State
{
    HWND hwnd{};
    HWND hwndSettings{};
    HWND hwndAbout{};

    int widgetH{};
    int widgetW{};

    AppBarState  appbar;
    UINT         wmTaskbarCreated{};

    std::atomic<bool> isMenuOpen{ false };
    bool isVisible = true; // Tracks startup delay visibility

    Config       cfg;
    std::wstring iniPath;

    SensorMap sensorMap;

    double       values[P_COUNT] = {};
    bool         sensorFound[P_COUNT] = {};
    bool         dataValid = false;

    double netDown = 0; bool netDownFound = false;
    double diskWrite = 0; bool diskWriteFound = false;

    std::mutex   dataMutex;

    std::vector<DeviceInfo> gpuList;
    std::vector<DeviceInfo> diskList;
    std::vector<DeviceInfo> nicList;
    std::mutex              deviceMutex;
    bool                    devicesReady = false;

    std::atomic<bool> running{ true };
    std::atomic<bool> drawPending{ false };

    HINSTANCE hInst{};

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