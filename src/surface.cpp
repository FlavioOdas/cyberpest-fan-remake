#include "surface.h"

#include <windows.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>

void Surface::fill(uint8_t index) {
    std::fill(pixels_.begin(), pixels_.end(), index);
    modRegion_ = {};
    modLaterBlits_.clear();
}

void Surface::fillRect(int left, int top, int right, int bottom, uint8_t index) {
    for (int y = std::max(0, top); y < std::min(height_, bottom); ++y) {
        const int x0 = std::max(0, left), x1 = std::min(width_, right);
        if (x0 < x1) std::fill(pixels_.begin() + size_t(y) * width_ + x0,
                               pixels_.begin() + size_t(y) * width_ + x1, index);
    }
}

void Surface::drawText(const std::string& value, int left, int top, int right, int bottom,
                       const std::string& fontFace, int fontSize, Rgb foreground, Rgb background,
                       const Palette& palette, bool autoFit, bool multiline, bool caret) {
    auto paletteIndex = [&](Rgb color) {
        int best = 0, bestDistance = INT_MAX;
        for (int index = 0; index < 256; ++index) {
            const Rgb& candidate = palette[size_t(index)];
            const int red = int(candidate.r) - color.r;
            const int green = int(candidate.g) - color.g;
            const int blue = int(candidate.b) - color.b;
            const int distance = red * red + green * green + blue * blue;
            if (distance < bestDistance) {
                bestDistance = distance;
                best = index;
            }
        }
        return uint8_t(best);
    };
    fillRect(left, top, right, bottom, paletteIndex(background));
    const int width = right - left, height = bottom - top;
    if ((value.empty() && !caret) || width <= 0 || height <= 0) return;

    BITMAPINFO bitmap = {};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = width;
    bitmap.bmiHeader.biHeight = -height;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;
    HBITMAP image = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!image || !pixels) {
        if (image) DeleteObject(image);
        DeleteDC(dc);
        return;
    }
    HGDIOBJ oldImage = SelectObject(dc, image);
    std::fill_n(static_cast<uint32_t*>(pixels), size_t(width) * height, 0x00ff00ff);
    auto createFont = [&](int size) {
        return CreateFontA(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                           FIXED_PITCH | FF_MODERN, fontFace.c_str());
    };
    HFONT font = createFont(fontSize);
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    SIZE extent = {};
    if (autoFit && font && !multiline) {
        while (fontSize > 8 &&
               GetTextExtentPoint32A(dc, value.data(), int(value.size()), &extent) && extent.cx > width) {
            SelectObject(dc, oldFont);
            DeleteObject(font);
            font = createFont(--fontSize);
            if (!font) break;
            SelectObject(dc, font);
        }
    }
    size_t start = 0;
    while (!multiline && start + 1 < value.size() &&
           GetTextExtentPoint32A(dc, value.data() + start, int(value.size() - start), &extent) &&
           extent.cx > width) ++start;
    RECT bounds = {0, 0, width, height};
    DrawTextA(dc, value.data() + start, int(value.size() - start), &bounds,
              DT_LEFT | DT_TOP | DT_NOPREFIX | (multiline ? DT_WORDBREAK : DT_SINGLELINE));
    if (caret && !multiline) {  // the edit control's caret after the last character
        TEXTMETRICA metrics = {};
        GetTextMetricsA(dc, &metrics);
        extent = {};
        if (start < value.size()) GetTextExtentPoint32A(dc, value.data() + start, int(value.size() - start), &extent);
        const int x = std::min<int>(extent.cx, width - 2);
        RECT bar = {x, 0, x + 2, std::min<int>(metrics.tmHeight, height)};
        FillRect(dc, &bar, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    }
    const uint8_t ink = paletteIndex(foreground);
    const auto* source = static_cast<const uint32_t*>(pixels);
    for (int y = std::max(0, top); y < std::min(height_, bottom); ++y) {
        for (int x = std::max(0, left); x < std::min(width_, right); ++x) {
            if (source[size_t(y - top) * width + (x - left)] != 0x00ff00ff)
                pixels_[size_t(y) * width_ + x] = ink;
        }
    }
    if (oldFont) SelectObject(dc, oldFont);
    if (font) DeleteObject(font);
    SelectObject(dc, oldImage);
    DeleteObject(image);
    DeleteDC(dc);
}

void Surface::blit(const uint8_t* src, int srcWidth, int srcHeight, int x, int y, int transparent,
                   bool mirror, const char* assetName) {
    if (experimentalMod_ && assetName && std::strcmp(assetName, "MEGA.FLC") == 0 &&
        srcWidth == 120 && srcHeight == 120 && !modRegion_.width) {
        modUnderlay_ = pixels_;
        modRegion_ = {x, y, srcWidth, srcHeight};
    } else if (modRegion_.width) {
        modLaterBlits_.push_back({src, srcWidth, srcHeight, x, y, transparent, mirror});
    }
    const int x0 = std::max(0, x), y0 = std::max(0, y);
    const int x1 = std::min(width_, x + srcWidth), y1 = std::min(height_, y + srcHeight);
    if (mirror) {
        for (int dy = y0; dy < y1; ++dy) {
            const uint8_t* row = src + size_t(dy - y) * srcWidth;
            uint8_t* d = &pixels_[size_t(dy) * width_ + x0];
            for (int dx = x0; dx < x1; ++dx, ++d) {
                const uint8_t c = row[srcWidth - 1 - (dx - x)];
                if (c != transparent) *d = c;
            }
        }
        return;
    }
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

bool writeBmp32(const wchar_t* path, const uint32_t* pixels, int width, int height) {
    const uint32_t imageSize = uint32_t(width) * uint32_t(height) * 4;
    uint8_t header[54] = {'B', 'M'};
    auto put32 = [&](int offset, uint32_t value) { std::memcpy(header + offset, &value, 4); };
    auto put16 = [&](int offset, uint16_t value) { std::memcpy(header + offset, &value, 2); };
    put32(2, uint32_t(sizeof header) + imageSize);
    put32(10, sizeof header);
    put32(14, 40);
    put32(18, uint32_t(width));
    put32(22, uint32_t(height));
    put16(26, 1);
    put16(28, 32);
    put32(34, imageSize);
    FILE* file = _wfopen(path, L"wb");
    if (!file) return false;
    fwrite(header, 1, sizeof header, file);
    for (int row = height - 1; row >= 0; --row)
        fwrite(pixels + size_t(row) * width, 4, size_t(width), file);
    return fclose(file) == 0;
}
