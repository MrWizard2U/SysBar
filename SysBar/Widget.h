#pragma once

#include "State.h"

// ─────────────────────────────────────────────────────────────────────────────
// Widget geometry
// ─────────────────────────────────────────────────────────────────────────────

// Returns the correct widget pixel width based on how many params are enabled.
// Formula: ceil(enabledCount / 2) columns * COL_WIDTH px.
int ComputeWidgetWidth(State* s);

// ─────────────────────────────────────────────────────────────────────────────
// Shell / AppBar integration
// ─────────────────────────────────────────────────────────────────────────────

// Register the widget HWND as a new AppBar with the shell.
// Safe to call multiple times — no-op if already registered.
void AppBarRegister(State* s);

// Remove the AppBar registration.
// Must be called before the HWND is destroyed.
void AppBarUnregister(State* s);

// Re-query Shell_TrayWnd and TrayNotifyWnd positions and move the widget
// to sit immediately left of the notification area.
// Also calls SHAppBarMessage(ABM_SETPOS) to inform the shell of our rect.
// Safe to call from any thread via PostMessage(WM_USER+1).
void RepositionWidget(State* s);
