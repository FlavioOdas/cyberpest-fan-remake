// The "your pest needs you" pop-up shown over other applications (1000:47f6, window class
// 1010:343c, module -596). docs/re/engine-runtime.md has the evidence.
#pragma once

#include <windows.h>

#include <cstdint>
#include <span>

#include "adm.h"

namespace attention {

// Shows the pop-up for a reason picture id (-600 bored, -601 hungry, -223 ill, -222 rabies,
// -221 fleas, -602 too late, -603 constipated) unless one is already up or the game window
// is the active one. Returns whether it was shown.
bool request(HWND mainWindow, const adm::Database& db, int32_t reason);

// Headless check: draws the pop-up for a reason into a 24-bit BMP file.
bool snapshot(const adm::Database& db, int32_t reason, const wchar_t* path);

}  // namespace attention
