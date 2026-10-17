#pragma once

#include <cstdint>

#include "flc.h"

class Surface;

// Optional, unfinished fan-art experiment. Replaces only authored sprites;
// everything else keeps the regular HD renderer.
void applyExperimentalMod(const Surface& surface, const Palette& palette, uint32_t* pixels);
