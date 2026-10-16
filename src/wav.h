// RIFF/WAVE PCM loader. The game's sounds are 8-bit mono 11,025 Hz.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

struct Sound {
    int rate = 0;
    std::vector<int16_t> samples;  // mono, signed 16-bit
};

// Returns false for anything but uncompressed 8/16-bit PCM.
bool loadWav(std::span<const uint8_t> data, Sound* out);
