#pragma once

#include "State.h"

// ─────────────────────────────────────────────────────────────────────────────
// Popup dialogs (remain as in-app windows)
// ─────────────────────────────────────────────────────────────────────────────

// Shows the About + Attribution window. If already open, brings it to the foreground.
void ShowAboutDialog(State* s);

// ─────────────────────────────────────────────────────────────────────────────
// Browser-launched pages (ShellExecute to mrwizard2u.github.io)
// ─────────────────────────────────────────────────────────────────────────────

// Opens sysbar-guide.html on the GitHub Pages site in the default browser.
void OpenUserGuide();

// Opens sysbar-license.html on the GitHub Pages site in the default browser.
void OpenLicense();

// Opens the donation page in the default browser.
void OpenDonationPage();

// Opens the tech support contact page / mailto in the default browser.
void OpenTechSupport();
