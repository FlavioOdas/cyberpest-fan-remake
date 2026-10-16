#include "wav.h"

#include <cstring>

namespace {
uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
}  // namespace

bool loadWav(std::span<const uint8_t> data, Sound* out) {
    const uint8_t* p = data.data();
    if (data.size() < 12 || std::memcmp(p, "RIFF", 4) || std::memcmp(p + 8, "WAVE", 4)) return false;
    int channels = 0, bits = 0;
    const uint8_t* end = p + data.size();
    for (const uint8_t* c = p + 12; end - c >= 8;) {
        const uint32_t size = u32(c + 4);
        const uint8_t* body = c + 8;
        if (size > uint32_t(end - body)) return false;
        if (!std::memcmp(c, "fmt ", 4) && size >= 16) {
            if (u16(body) != 1) return false;  // PCM only
            channels = u16(body + 2);
            out->rate = int(u32(body + 4));
            bits = u16(body + 14);
        } else if (!std::memcmp(c, "data", 4) && channels > 0) {
            const int frameBytes = channels * bits / 8;
            if (frameBytes <= 0 || (bits != 8 && bits != 16)) return false;
            const size_t frames = size / frameBytes;
            out->samples.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                int sum = 0;
                for (int ch = 0; ch < channels; ++ch) {
                    const uint8_t* s = body + i * frameBytes + ch * (bits / 8);
                    sum += bits == 8 ? (int(*s) - 128) << 8 : int16_t(u16(s));
                }
                out->samples[i] = int16_t(sum / channels);
            }
            return true;
        }
        c = body + size + (size & 1);
    }
    return false;
}
