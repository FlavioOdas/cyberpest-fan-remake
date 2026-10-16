#include "surface.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

void Surface::fill(uint8_t index) { std::fill(pixels_.begin(), pixels_.end(), index); }

void Surface::blit(const uint8_t* src, int srcWidth, int srcHeight, int x, int y, int transparent) {
    const int x0 = std::max(0, x), y0 = std::max(0, y);
    const int x1 = std::min(width_, x + srcWidth), y1 = std::min(height_, y + srcHeight);
    for (int dy = y0; dy < y1; ++dy) {
        const uint8_t* s = src + size_t(dy - y) * srcWidth + (x0 - x);
        uint8_t* d = &pixels_[size_t(dy) * width_ + x0];
        if (transparent < 0) {
            std::memcpy(d, s, size_t(x1 - x0));
            continue;
        }
        for (int dx = x0; dx < x1; ++dx, ++s, ++d) {
            if (*s != transparent) *d = *s;
        }
    }
}

bool readBmpPalette(std::span<const uint8_t> bmp, Palette* out) {
    if (bmp.size() < 54 || bmp[0] != 'B' || bmp[1] != 'M') return false;
    uint32_t headerSize, colors;
    uint16_t bits;
    std::memcpy(&headerSize, &bmp[14], 4);
    std::memcpy(&bits, &bmp[28], 2);
    std::memcpy(&colors, &bmp[46], 4);
    if (bits != 8) return false;
    colors = colors ? std::min<uint32_t>(colors, 256) : 256;
    const size_t at = 14 + size_t(headerSize);
    if (bmp.size() < at + colors * 4) return false;
    for (uint32_t i = 0; i < colors; ++i) (*out)[i] = {bmp[at + 4 * i + 2], bmp[at + 4 * i + 1], bmp[at + 4 * i]};
    return true;
}

bool writeBmp(const wchar_t* path, const Surface& surface, const Palette& palette) {
    const int w = surface.width(), h = surface.height();
    const int stride = (w + 3) & ~3;
    const uint32_t offset = 14 + 40 + 1024, size = offset + uint32_t(stride) * h;
    uint8_t header[14 + 40] = {'B', 'M'};
    auto put32 = [&](int at, uint32_t v) { std::memcpy(header + at, &v, 4); };
    auto put16 = [&](int at, uint16_t v) { std::memcpy(header + at, &v, 2); };
    put32(2, size);
    put32(10, offset);
    put32(14, 40);
    put32(18, uint32_t(w));
    put32(22, uint32_t(h));
    put16(26, 1);
    put16(28, 8);
    put32(46, 256);
    FILE* f = _wfopen(path, L"wb");
    if (!f) return false;
    fwrite(header, 1, sizeof header, f);
    for (const Rgb& c : palette) {
        const uint8_t quad[4] = {c.b, c.g, c.r, 0};
        fwrite(quad, 1, 4, f);
    }
    const uint8_t pad[3] = {};
    for (int y = h - 1; y >= 0; --y) {  // bottom-up rows
        fwrite(surface.pixels() + size_t(y) * w, 1, size_t(w), f);
        fwrite(pad, 1, size_t(stride - w), f);
    }
    return fclose(f) == 0;
}
