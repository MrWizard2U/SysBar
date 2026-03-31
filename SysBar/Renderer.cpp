#include "Renderer.h"
#include "Sensor.h"
#include "Config.h"
#include "ThemeUtil.h"

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
#pragma comment(lib,"dcomp.lib")
#pragma comment(lib,"gdi32.lib")

static const D2D1_COLOR_F BG_COLOR = { 0.0f, 0.0f, 0.0f, 0.0f };

static bool HR(HRESULT hr, HWND hwnd, const char* ctx = nullptr)
{
    if (SUCCEEDED(hr)) return true;
#ifdef _DEBUG
    wchar_t msg[256];
    swprintf_s(msg, L"HRESULT 0x%08X\n%hs", (unsigned)hr, ctx ? ctx : "");
    MessageBoxW(nullptr, msg, L"Renderer Error", MB_ICONERROR | MB_OK);
#endif
    if (hwnd && IsWindow(hwnd))
        DestroyWindow(hwnd);
    return false;
}

static bool RecreateTargetBitmap(State* s)
{
    s->ctx->SetTarget(nullptr);
    ComPtr<IDXGISurface> surface;
    if (!HR(s->swapChain->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(surface.GetAddressOf())), s->hwnd, "GetBuffer")) return false;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    ComPtr<ID2D1Bitmap1> bmp;
    if (!HR(s->ctx->CreateBitmapFromDxgiSurface(surface.Get(), &props, &bmp), s->hwnd, "CreateBitmapFromDxgiSurface")) return false;

    s->ctx->SetTarget(bmp.Get());
    s->ctx->SetDpi(96.0f, 96.0f);

    return true;
}

static bool BuildTextFormat(State* s, float heightPx)
{
    s->textFormat.Reset();
    float fontSize = heightPx * 0.28f;
    if (!HR(s->dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &s->textFormat), s->hwnd, "CreateTextFormat")) return false;

    if (!HR(s->textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP), s->hwnd, "SetWordWrapping")) return false;
    if (!HR(s->textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER), s->hwnd, "SetParagraphAlignment")) return false;

    return true;
}

bool InitDComp(State* s)
{
    D3D_FEATURE_LEVEL fl;
    HRESULT hrD3D = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &s->d3d, &fl, nullptr);
    if (FAILED(hrD3D)) hrD3D = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &s->d3d, &fl, nullptr);

    if (!HR(hrD3D, nullptr, "D3D11CreateDevice")) return false;
    if (!HR(s->d3d.As(&s->dxgi), nullptr, "QI IDXGIDevice")) return false;
    if (!HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr, reinterpret_cast<void**>(s->d2dFactory.GetAddressOf())), nullptr, "D2D1CreateFactory")) return false;
    if (!HR(s->d2dFactory->CreateDevice(s->dxgi.Get(), &s->d2dDevice), nullptr, "D2D CreateDevice")) return false;
    if (!HR(s->d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &s->ctx), nullptr, "CreateDeviceContext")) return false;
    if (!HR(DCompositionCreateDevice(s->dxgi.Get(), __uuidof(IDCompositionDevice), reinterpret_cast<void**>(s->dcomp.GetAddressOf())), nullptr, "DCompositionCreateDevice")) return false;

    if (!HR(s->dcomp->CreateTargetForHwnd(s->hwnd, FALSE, &s->target), s->hwnd, "CreateTargetForHwnd")) return false;
    if (!HR(s->dcomp->CreateVisual(&s->visual), s->hwnd, "CreateVisual")) return false;

    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (!HR(s->dxgi->GetAdapter(&adapter), s->hwnd, "GetAdapter")) return false;
    if (!HR(adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory.GetAddressOf())), s->hwnd, "GetParent")) return false;

    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width = (UINT)s->widgetW;
    scd.Height = (UINT)s->widgetH;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SampleDesc = { 1, 0 };
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    if (!HR(factory->CreateSwapChainForComposition(s->d3d.Get(), &scd, nullptr, &s->swapChain), s->hwnd, "CreateSwapChainForComposition")) return false;
    if (!HR(s->visual->SetContent(s->swapChain.Get()), s->hwnd, "SetContent")) return false;
    if (!HR(s->target->SetRoot(s->visual.Get()), s->hwnd, "SetRoot")) return false;
    if (!HR(s->dcomp->Commit(), s->hwnd, "Commit")) return false;

    if (!RecreateTargetBitmap(s)) return false;

    // Brush color initialized later in Draw()
    if (!HR(s->ctx->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &s->textBrush), s->hwnd, "CreateSolidColorBrush")) return false;
    if (!HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(s->dwFactory.GetAddressOf())), s->hwnd, "DWriteCreateFactory")) return false;

    return BuildTextFormat(s, (float)s->widgetH);
}

void ResizeSwapChain(State* s, UINT newW, UINT newH)
{
    if (!s->swapChain || newW == 0 || newH == 0) return;
    if (s->widgetW == (int)newW && s->widgetH == (int)newH) return;

    s->ctx->SetTarget(nullptr);
    if (!HR(s->swapChain->ResizeBuffers(0, newW, newH, DXGI_FORMAT_UNKNOWN, 0), s->hwnd, "ResizeBuffers")) return;
    if (!RecreateTargetBitmap(s)) return;
    BuildTextFormat(s, (float)newH);
    s->widgetH = (int)newH;
    s->widgetW = (int)newW;

    if (s->dcomp) s->dcomp->Commit();
}

static std::wstring FormatSpeed(double bps)
{
    wchar_t b[32]{};
    double gb = bps / (1024.0 * 1024.0 * 1024.0);
    double mb = bps / (1024.0 * 1024.0);
    double kb = bps / 1024.0;

    if (gb >= 9.95)       swprintf_s(b, L"%.0fG", gb);
    else if (gb >= 0.995) swprintf_s(b, L"%.1fG", gb);
    else if (mb >= 999.5) swprintf_s(b, L"%.1fG", gb);
    else if (mb >= 9.95)  swprintf_s(b, L"%.0fM", mb);
    else if (mb >= 0.995) swprintf_s(b, L"%.1fM", mb);
    else if (kb >= 999.5) swprintf_s(b, L"%.1fM", mb);
    else if (kb >= 9.95)  swprintf_s(b, L"%.0fK", kb);
    else if (kb >= 0.995) swprintf_s(b, L"%.1fK", kb);
    else if (bps >= 999.5)swprintf_s(b, L"%.1fK", kb);
    else                  swprintf_s(b, L"%dB", (int)bps);

    return b;
}

struct Column { std::wstring top; std::wstring bottom; };

static std::vector<Column> BuildColumns(State* s, const double vals[], const bool found[], double netUp, bool netUpFound, double netDown, bool netDownFound, double diskRead, bool diskReadFound, double diskWrite, bool diskWriteFound)
{
    std::vector<Column> cols;
    struct Cell { std::wstring label; std::wstring value; };
    std::vector<Cell> cells;

    for (int i = 0; i < P_COUNT; ++i) {
        if (!s->cfg.enabled[i] || PARAMS[i].isFixedPair) continue;
        cells.push_back({ PARAMS[i].label, FormatValue((ParamId)i, vals[i], found[i]) });
    }

    for (int i = 0; i < (int)cells.size(); i += 2) {
        Column col;
        col.top = cells[i].label + L": " + cells[i].value;
        if (i + 1 < (int)cells.size()) col.bottom = cells[i + 1].label + L": " + cells[i + 1].value;
        cols.push_back(col);
    }

    if (s->cfg.enabled[P_DISK_READWRITE]) {
        Column col;
        col.top = std::wstring(L"DkR: ") + (diskReadFound ? FormatSpeed(diskRead) : L"N/A");
        col.bottom = std::wstring(L"DkW: ") + (diskWriteFound ? FormatSpeed(diskWrite) : L"N/A");
        cols.push_back(col);
    }

    if (s->cfg.enabled[P_NET_UPDOWN]) {
        Column col;
        col.top = std::wstring(L"Up: ") + (netUpFound ? FormatSpeed(netUp) : L"N/A");
        col.bottom = std::wstring(L"Dn: ") + (netDownFound ? FormatSpeed(netDown) : L"N/A");
        cols.push_back(col);
    }

    if (cols.empty()) cols.push_back({ L"OFF", L"" });
    return cols;
}

void Draw(State* s)
{
    s->drawPending = false;

    double vals[P_COUNT]; bool found[P_COUNT];
    double netUp, netDown, diskRead, diskWrite;
    bool   netUpFound, netDownFound, diskReadFound, diskWriteFound, valid;

    {
        std::lock_guard<std::mutex> lk(s->dataMutex);
        memcpy(vals, s->values, sizeof(vals));
        memcpy(found, s->sensorFound, sizeof(found));
        netUp = s->values[P_NET_UPDOWN]; netUpFound = s->sensorFound[P_NET_UPDOWN];
        netDown = s->netDown; netDownFound = s->netDownFound;
        diskRead = s->values[P_DISK_READWRITE]; diskReadFound = s->sensorFound[P_DISK_READWRITE];
        diskWrite = s->diskWrite; diskWriteFound = s->diskWriteFound;
        valid = s->dataValid;
    }

    if (!valid) {
        for (int i = 0; i < P_COUNT; ++i) { vals[i] = 0; found[i] = false; }
        netUp = netDown = diskRead = diskWrite = 0;
        netUpFound = netDownFound = diskReadFound = diskWriteFound = false;
    }

    auto cols = BuildColumns(s, vals, found, netUp, netUpFound, netDown, netDownFound, diskRead, diskReadFound, diskWrite, diskWriteFound);
    int numCols = (int)cols.size();
    if (numCols < MIN_COLS) numCols = MIN_COLS;
    if (numCols > MAX_COLS) numCols = MAX_COLS;

    float w = (float)s->widgetW;
    float h = (float)s->widgetH;
    float colW = w / (float)numCols;
    static const float MARGIN = 2.0f;

    s->ctx->BeginDraw();
    s->ctx->Clear(BG_COLOR);

    // Apply Theme Awareness: Dark text on light taskbars, White text on dark taskbars
    if (IsSystemLightTheme()) {
        s->textBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
    } else {
        s->textBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    }

    for (int col = 0; col < numCols && col < (int)cols.size(); ++col)
    {
        float cx = (float)col * colW + MARGIN;
        float cw = colW - MARGIN * 2.0f;

        float fontSize = h * 0.28f;
        float fontPx = fontSize * (96.0f / 72.0f);
        float lineH = fontPx * 1.2f;
        float blockH = lineH * 2.0f;
        float topY = (h - blockH) * 0.5f;

        if (!cols[col].top.empty()) {
            D2D1_RECT_F r = D2D1::RectF(cx, topY, cx + cw, topY + lineH);
            s->ctx->DrawText(cols[col].top.c_str(), (UINT32)cols[col].top.size(), s->textFormat.Get(), &r, s->textBrush.Get());
        }
        if (!cols[col].bottom.empty()) {
            D2D1_RECT_F r = D2D1::RectF(cx, topY + lineH, cx + cw, topY + lineH * 2.0f);
            s->ctx->DrawText(cols[col].bottom.c_str(), (UINT32)cols[col].bottom.size(), s->textFormat.Get(), &r, s->textBrush.Get());
        }
    }

    HRESULT hr = s->ctx->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET || FAILED(hr)) {
        DestroyWindow(s->hwnd); return;
    }

    DXGI_PRESENT_PARAMETERS pp{};
    hr = s->swapChain->Present1(0, 0, &pp);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) DestroyWindow(s->hwnd);
}