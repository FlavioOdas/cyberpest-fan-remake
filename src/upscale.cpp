#include "upscale.h"

namespace {

void scale2x(const uint32_t* source, int width, int height, uint32_t* target) {
    const int targetWidth = width * 2;
    for (int row = 0; row < height; ++row) {
        const int aboveRow = row > 0 ? row - 1 : row;
        const int belowRow = row + 1 < height ? row + 1 : row;
        for (int column = 0; column < width; ++column) {
            const int leftColumn = column > 0 ? column - 1 : column;
            const int rightColumn = column + 1 < width ? column + 1 : column;
            const uint32_t center = source[row * width + column];
            const uint32_t above = source[aboveRow * width + column];
            const uint32_t below = source[belowRow * width + column];
            const uint32_t left = source[row * width + leftColumn];
            const uint32_t right = source[row * width + rightColumn];
            const bool split = above != below && left != right;
            const int topLeft = (row * 2) * targetWidth + column * 2;
            const int bottomLeft = topLeft + targetWidth;
            target[topLeft] = split && left == above ? left : center;
            target[topLeft + 1] = split && above == right ? right : center;
            target[bottomLeft] = split && left == below ? left : center;
            target[bottomLeft + 1] = split && below == right ? right : center;
        }
    }
}

void softenEdges(const uint32_t* source, int width, int height, uint32_t* target) {
    for (int row = 0; row < height; ++row) {
        const int aboveRow = row > 0 ? row - 1 : row;
        const int belowRow = row + 1 < height ? row + 1 : row;
        for (int column = 0; column < width; ++column) {
            const int leftColumn = column > 0 ? column - 1 : column;
            const int rightColumn = column + 1 < width ? column + 1 : column;
            const uint32_t center = source[row * width + column];
            const uint32_t above = source[aboveRow * width + column];
            const uint32_t below = source[belowRow * width + column];
            const uint32_t left = source[row * width + leftColumn];
            const uint32_t right = source[row * width + rightColumn];
            if (center == above && center == below && center == left && center == right) {
                target[row * width + column] = center;
                continue;
            }
            const uint32_t redBlue = ((center & 0x00ff00ffu) * 12 + (above & 0x00ff00ffu) +
                                      (below & 0x00ff00ffu) + (left & 0x00ff00ffu) +
                                      (right & 0x00ff00ffu)) >> 4 & 0x00ff00ffu;
            const uint32_t green = ((center & 0x0000ff00u) * 12 + (above & 0x0000ff00u) +
                                    (below & 0x0000ff00u) + (left & 0x0000ff00u) +
                                    (right & 0x0000ff00u)) >> 4 & 0x0000ff00u;
            target[row * width + column] = 0xff000000u | redBlue | green;
        }
    }
}

}

void HdScaler::scale(const uint8_t* source, int width, int height, const Palette& palette) {
    source_.resize(size_t(width) * height);
    for (size_t index = 0; index < source_.size(); ++index) {
        const Rgb& color = palette[source[index]];
        source_[index] = 0xff000000u | uint32_t(color.r) << 16 | uint32_t(color.g) << 8 | color.b;
    }
    enlarge(width, height);
}

void HdScaler::scaleRgb(const uint32_t* source, int width, int height) {
    source_.assign(source, source + size_t(width) * height);
    enlarge(width, height);
}

void HdScaler::enlarge(int width, int height) {
    width_ = width * kScale;
    height_ = height * kScale;
    doubled_.resize(size_t(width * 2) * (height * 2));
    enlarged_.resize(size_t(width_) * height_);
    pixels_.resize(size_t(width_) * height_);
    scale2x(source_.data(), width, height, doubled_.data());
    scale2x(doubled_.data(), width * 2, height * 2, enlarged_.data());
    softenEdges(enlarged_.data(), width_, height_, pixels_.data());
}
