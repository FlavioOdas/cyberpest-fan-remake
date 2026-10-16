#include "flc.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr uint16_t kFrameChunk = 0xF1FA;
enum : uint16_t { Color256 = 4, DeltaFlc = 7, Color64 = 11, DeltaFli = 12, Black = 13, ByteRun = 15, Copy = 16 };

uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }

}  // namespace

bool Flc::open(std::span<const uint8_t> data) {
    if (data.size() < 128) return false;
    const uint8_t* h = data.data();
    const uint16_t magic = u16(h + 4);
    if (magic != 0xAF12 && magic != 0xAF11) return false;
    data_ = data;
    frameCount_ = u16(h + 6);
    width_ = u16(h + 8);
    height_ = u16(h + 10);
    const uint32_t speed = u32(h + 16);
    delayMs_ = magic == 0xAF11 ? int(speed * 1000 / 70) : int(speed);
    firstFrame_ = magic == 0xAF12 && u32(h + 80) ? u32(h + 80) : 128;
    return width_ > 0 && height_ > 0;
}

bool Flc::decodeAll() {
    frames_.clear();
    std::vector<uint8_t> px(size_t(width_) * height_, 0);
    const uint8_t* const base = data_.data();
    const uint8_t* const end = base + data_.size();
    const uint8_t* p = base + firstFrame_;
    for (int f = 0; f < frameCount_; ++f) {
        if (end - p < 16) return false;
        const uint32_t size = u32(p);
        if (size < 16 || size > uint32_t(end - p)) return false;
        if (u16(p + 4) == kFrameChunk) {
            const uint16_t chunks = u16(p + 6);
            const uint8_t* c = p + 16;
            const uint8_t* frameEnd = p + size;
            for (int i = 0; i < chunks; ++i) {
                if (frameEnd - c < 6) return false;
                const uint32_t csize = u32(c);
                if (csize < 6 || csize > uint32_t(frameEnd - c)) return false;
                if (!applyChunk(u16(c + 4), c + 6, c + csize, px)) return false;
                c += csize;
            }
        } else {
            --f;  // prefix chunk (0xF100) does not count as a frame
        }
        frames_.push_back(px);
        p += size;
    }
    return true;
}

bool Flc::applyChunk(uint16_t type, const uint8_t* p, const uint8_t* end, std::vector<uint8_t>& px) {
    const int w = width_, h = height_;
    auto need = [&](size_t n) { return size_t(end - p) >= n; };
    switch (type) {
    case Color256:
    case Color64: {
        const int scale = type == Color256 ? 1 : 4;
        if (!need(2)) return false;
        int packets = u16(p);
        p += 2;
        int index = 0;
        while (packets--) {
            if (!need(2)) return false;
            index += p[0];
            int count = p[1] ? p[1] : 256;
            p += 2;
            if (!need(size_t(count) * 3)) return false;
            for (; count > 0; --count, ++index, p += 3) {
                palette_[index & 255] = {uint8_t(std::min(p[0] * scale, 255)), uint8_t(std::min(p[1] * scale, 255)),
                                         uint8_t(std::min(p[2] * scale, 255))};
            }
        }
        hasPalette_ = true;
        return true;
    }
    case Black:
        std::fill(px.begin(), px.end(), 0);
        return true;
    case Copy:
        if (!need(px.size())) return false;
        std::memcpy(px.data(), p, px.size());
        return true;
    case ByteRun:
        for (int y = 0; y < h; ++y) {
            if (!need(1)) return false;
            ++p;  // obsolete packet count
            uint8_t* row = &px[size_t(y) * w];
            int x = 0;
            while (x < w) {
                if (!need(1)) return false;
                const int n = int8_t(*p++);
                if (n < 0) {
                    const int len = std::min(-n, w - x);
                    if (!need(-n)) return false;
                    std::memcpy(row + x, p, len);
                    p += -n;
                    x += -n;
                } else {
                    if (!need(1)) return false;
                    std::memset(row + x, *p++, std::min(n, w - x));
                    x += n;
                }
            }
        }
        return true;
    case DeltaFli: {
        if (!need(4)) return false;
        int y = u16(p), lines = u16(p + 2);
        p += 4;
        for (; lines > 0 && y < h; --lines, ++y) {
            if (!need(1)) return false;
            int packets = *p++;
            int x = 0;
            uint8_t* row = &px[size_t(y) * w];
            while (packets--) {
                if (!need(2)) return false;
                x += p[0];
                const int n = int8_t(p[1]);
                p += 2;
                if (n > 0) {
                    if (!need(n) || x + n > w) return false;
                    std::memcpy(row + x, p, n);
                    p += n;
                    x += n;
                } else {
                    if (!need(1) || x - n > w) return false;
                    std::memset(row + x, *p++, -n);
                    x -= n;
                }
            }
        }
        return true;
    }
    case DeltaFlc: {
        if (!need(2)) return false;
        int lines = u16(p);
        p += 2;
        int y = 0;
        while (lines > 0) {
            if (!need(2)) return false;
            const uint16_t word = u16(p);
            p += 2;
            if ((word & 0xC000) == 0xC000) {  // skip lines
                y += 0x10000 - word;
                continue;
            }
            if (y >= h) return false;
            uint8_t* row = &px[size_t(y) * w];
            if ((word & 0xC000) == 0x8000) {  // last pixel of the line
                row[w - 1] = uint8_t(word);
                continue;
            }
            int x = 0;
            for (int packets = word; packets > 0; --packets) {
                if (!need(2)) return false;
                x += p[0];
                const int n = int8_t(p[1]);
                p += 2;
                if (n > 0) {
                    if (!need(size_t(n) * 2) || x + 2 * n > w) return false;
                    std::memcpy(row + x, p, size_t(n) * 2);
                    p += 2 * n;
                    x += 2 * n;
                } else {
                    if (!need(2) || x - 2 * n > w) return false;
                    for (int i = 0; i < -n; ++i, x += 2) {
                        row[x] = p[0];
                        row[x + 1] = p[1];
                    }
                    p += 2;
                }
            }
            ++y;
            --lines;
        }
        return true;
    }
    default:
        return true;  // PSTAMP (18) and unknown chunks are ignored
    }
}
