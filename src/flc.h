// Autodesk Animator FLC/FLI decoder (8-bit). Sprites in PEST.ADM are FLC files.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
};
using Palette = std::array<Rgb, 256>;

class Flc {
public:
    // Parses the header; returns false if the data is not an FLC/FLI file.
    bool open(std::span<const uint8_t> data);

    int width() const { return width_; }
    int height() const { return height_; }
    int frameCount() const { return frameCount_; }
    int frameDelayMs() const { return delayMs_; }

    // Decodes every frame up front (sprites are small; backgrounds are single frames).
    // frames()[i] is width*height palette indices; palette() is the palette after frame 0.
    bool decodeAll();
    const std::vector<std::vector<uint8_t>>& frames() const { return frames_; }
    const Palette& palette() const { return palette_; }
    bool hasPalette() const { return hasPalette_; }

private:
    bool applyChunk(uint16_t type, const uint8_t* p, const uint8_t* end, std::vector<uint8_t>& px);

    std::span<const uint8_t> data_;
    int width_ = 0, height_ = 0, frameCount_ = 0, delayMs_ = 0;
    uint32_t firstFrame_ = 128;
    Palette palette_{};
    bool hasPalette_ = false;
    std::vector<std::vector<uint8_t>> frames_;
};
