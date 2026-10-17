// 8-bit palettised drawing surface — the remake's stand-in for a WinG bitmap.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "flc.h"

class Surface {
public:
    struct BlitCommand {
        const uint8_t* pixels;
        int width, height, x, y, transparent;
        bool mirror;
    };
    struct ModRegion {
        int x = 0, y = 0, width = 0, height = 0;
    };
    Surface(int width, int height) : width_(width), height_(height), pixels_(size_t(width) * height, 0) {}

    int width() const { return width_; }
    int height() const { return height_; }
    uint8_t* pixels() { return pixels_.data(); }
    const uint8_t* pixels() const { return pixels_.data(); }
    void setExperimentalMod(bool enabled) { experimentalMod_ = enabled; }
    bool hasModRegion() const { return modRegion_.width > 0; }
    ModRegion modRegion() const { return modRegion_; }
    const std::vector<uint8_t>& modUnderlay() const { return modUnderlay_; }
    const std::vector<BlitCommand>& modLaterBlits() const { return modLaterBlits_; }

    void fill(uint8_t index);
    // Copies an indexed image at (x, y), clipped. Pixels equal to `transparent` are skipped
    // (the game's transparency key is palette index 0, pure green); pass -1 to copy all.
    // `mirror` flips the image horizontally (objects facing the other way).
    void blit(const uint8_t* src, int srcWidth, int srcHeight, int x, int y, int transparent = 0,
              bool mirror = false, const char* assetName = nullptr);
    void fillRect(int left, int top, int right, int bottom, uint8_t index);
    void drawText(const std::string& value, int left, int top, int right, int bottom,
                  const std::string& fontFace, int fontSize, Rgb foreground, Rgb background,
                  const Palette& palette, bool autoFit = false, bool multiline = false, bool caret = false);

private:
    int width_, height_;
    std::vector<uint8_t> pixels_;
    bool experimentalMod_ = false;
    ModRegion modRegion_;
    std::vector<uint8_t> modUnderlay_;
    std::vector<BlitCommand> modLaterBlits_;
};

// Writes an 8-bit indexed BMP (used by --snapshot to verify rendering headlessly).
bool writeBmp(const wchar_t* path, const Surface& surface, const Palette& palette);
bool writeBmp32(const wchar_t* path, const uint32_t* pixels, int width, int height);

// Reads the colour table of an 8-bit BMP (the game's master palette is PALETTE.BMP).
bool readBmpPalette(std::span<const uint8_t> bmp, Palette* out);
