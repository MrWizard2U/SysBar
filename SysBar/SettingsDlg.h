#pragma once

#include "State.h"

// Shows the Settings window. If already open, brings it to the foreground.
// On Save, updates s->cfg, calls SaveConfig(s), calls RepositionWidget(s),
// and triggers a redraw. The window is non-modal.
void ShowSettingsDialog(State* s);
