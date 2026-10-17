#pragma once

#include <cstdint>
#include <vector>

#include "flc.h"

class HdScaler {
public:
    static constexpr int kScale = 4;

    void scale(const uint8_t* source, int width, int height, const Palette& palette);
    void scaleRgb(const uint32_t* source, int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    const uint32_t* pixels() const { return pixels_.data(); }
    uint32_t* mutablePixels() { return pixels_.data(); }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint32_t> source_;
    std::vector<uint32_t> doubled_;
    std::vector<uint32_t> enlarged_;
    std::vector<uint32_t> pixels_;

    void enlarge(int width, int height);
};
