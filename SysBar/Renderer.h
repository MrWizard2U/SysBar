#pragma once

#include "State.h"

// ─────────────────────────────────────────────────────────────────────────────
// Initialisation
// ─────────────────────────────────────────────────────────────────────────────

// Create D3D11 device, DXGI swap chain, D2D device context, DirectComposition
// target, and DirectWrite text format. Must be called once after the widget
// HWND is created and s->widgetW / s->widgetH are set.
// Returns false if a critical step fails; the caller should return 1 from
// wWinMain (the window does not yet exist so cleanup happens via wWinMain
// unwinding the mutex/event handles).
bool InitDComp(State* s);

// ─────────────────────────────────────────────────────────────────────────────
// Resize
// ─────────────────────────────────────────────────────────────────────────────

// Resize the DXGI swap chain and recreate the D2D render target bitmap to
// match the new pixel dimensions. Also rebuilds the DWrite text format at the
// new DPI-aware font size. Called from WM_SIZE and WM_DPICHANGED_AFTERPARENT.
// On device-lost the function calls DestroyWindow(s->hwnd) and returns early.
void ResizeSwapChain(State* s, UINT newW, UINT newH);

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

// Reads s->values[] (under dataMutex), builds column layout from enabled
// params, and renders a frame via D2D + DComp. Handles D2DERR_RECREATE_TARGET
// and DXGI device-lost by calling DestroyWindow(s->hwnd).
void Draw(State* s);
