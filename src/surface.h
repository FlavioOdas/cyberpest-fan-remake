// 8-bit palettised drawing surface — the remake's stand-in for a WinG bitmap.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "flc.h"

class Surface {
public:
    Surface(int width, int height) : width_(width), height_(height), pixels_(size_t(width) * height, 0) {}

    int width() const { return width_; }
    int height() const { return height_; }
    uint8_t* pixels() { return pixels_.data(); }
    const uint8_t* pixels() const { return pixels_.data(); }

    void fill(uint8_t index);
    // Copies an indexed image at (x, y), clipped. Pixels equal to `transparent` are skipped
    // (the game's transparency key is palette index 0, pure green); pass -1 to copy all.
    // `mirror` flips the image horizontally (objects facing the other way).
    void blit(const uint8_t* src, int srcWidth, int srcHeight, int x, int y, int transparent = 0, bool mirror = false);
    void fillRect(int left, int top, int right, int bottom, uint8_t index);
    void drawText(const std::string& value, int left, int top, int right, int bottom,
                  const std::string& fontFace, int fontSize, Rgb foreground, Rgb background,
                  const Palette& palette, bool autoFit = false, bool multiline = false, bool caret = false);

private:
    int width_, height_;
    std::vector<uint8_t> pixels_;
};

// Writes an 8-bit indexed BMP (used by --snapshot to verify rendering headlessly).
bool writeBmp(const wchar_t* path, const Surface& surface, const Palette& palette);

// Reads the colour table of an 8-bit BMP (the game's master palette is PALETTE.BMP).
bool readBmpPalette(std::span<const uint8_t> bmp, Palette* out);
