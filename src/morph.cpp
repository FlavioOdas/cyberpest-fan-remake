#include "morph.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "flc.h"

namespace {

// Adjacent pest poses move parts of the body by up to ~8 px; a 5x5 patch keeps thin
// outlines from matching sideways. Ties prefer smaller shifts so flat areas stay put.
constexpr int kSearch = 8;
constexpr int kPatch = 2;
constexpr float kMaxCost = 0.30f;        // accepted mismatch fraction (SAD scaled by 16)
constexpr float kMovePenalty = 0.15f;
constexpr int kMinSamples = 4;           // opaque patch pixels needed to trust a pixel
constexpr int kFillRounds = 2;
constexpr int kFillVotes = 5;            // neighbours that must agree before a pixel joins
constexpr size_t kFieldBudget = 8u * 1024 * 1024;  // cached displacement bytes before a reset

void matchFrames(const uint8_t* A, const uint8_t* B, int w, int h,
                 std::vector<int8_t>& dx, std::vector<int8_t>& dy) {
    const size_t n = size_t(w) * h;
    dx.assign(n, 0);
    dy.assign(n, 0);
    std::vector<uint8_t> changed(n);
    bool any = false;
    for (size_t i = 0; i < n; ++i) {
        changed[i] = A[i] != B[i];
        any = any || changed[i];
    }
    if (!any) return;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * w + x;
            if (!changed[i]) continue;
            int bestDx = 0, bestDy = 0;
            float bestCost = 1e9f;
            auto tryShift = [&](int sx, int sy) {
                int sad = 0, cnt = 0;
                for (int py = -kPatch; py <= kPatch; ++py) {
                    const int yy = y + py;
                    if (yy < 0 || yy >= h) continue;
                    for (int px = -kPatch; px <= kPatch; ++px) {
                        const int xx = x + px;
                        if (xx < 0 || xx >= w) continue;
                        const uint8_t a = A[size_t(yy) * w + xx];
                        if (a == 0) continue;
                        ++cnt;
                        const int bx = xx + sx, by = yy + sy;
                        if (bx < 0 || bx >= w || by < 0 || by >= h) {
                            sad += 2;
                            continue;
                        }
                        if (B[size_t(by) * w + bx] != a) ++sad;
                    }
                }
                if (cnt < kMinSamples) return;
                const float cost = float(sad) * 16.0f / float(cnt) +
                                   kMovePenalty * float(std::abs(sx) + std::abs(sy));
                if (cost < bestCost) {
                    bestCost = cost;
                    bestDx = sx;
                    bestDy = sy;
                }
            };
            // Shifts are tried ring by ring (increasing |sx|+|sy|); once the best match so far
            // is cheaper than the *smallest* cost any farther shift could have, no remaining
            // shift can win (ties prefer the smaller shift already kept), so stop — the result
            // is exactly the exhaustive search's, only faster.
            for (int d = 0; d <= 2 * kSearch; ++d) {
                for (int sx = -kSearch; sx <= kSearch; ++sx) {
                    const int rem = d - std::abs(sx);
                    if (rem < 0 || rem > kSearch) continue;
                    tryShift(sx, rem);
                    if (rem != 0) tryShift(sx, -rem);
                }
                if (bestCost <= kMovePenalty * float(d + 1)) break;
            }
            if (bestCost <= kMaxCost * 16.0f) {
                dx[i] = int8_t(bestDx);
                dy[i] = int8_t(bestDy);
            }
        }
    }

    // Coherence fill: a changed pixel the matcher left alone joins the plurality shift of its
    // neighbours, so limbs travel as one piece instead of dotting.
    for (int pass = 0; pass < kFillRounds; ++pass) {
        std::vector<int8_t> nx = dx, ny = dy;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const size_t i = size_t(y) * w + x;
                if (!changed[i] || dx[i] || dy[i]) continue;
                int ax = 0, ay = 0, an = 0, bx = 0, by = 0, bn = 0;
                for (int py = -1; py <= 1; ++py) {
                    for (int px = -1; px <= 1; ++px) {
                        if (!px && !py) continue;
                        const int xx = x + px, yy = y + py;
                        if (xx < 0 || xx >= w || yy < 0 || yy >= h) continue;
                        const size_t j = size_t(yy) * w + xx;
                        if (!dx[j] && !dy[j]) continue;
                        if (an && dx[j] == ax && dy[j] == ay) ++an;
                        else if (bn && dx[j] == bx && dy[j] == by) ++bn;
                        else if (!an) { ax = dx[j]; ay = dy[j]; an = 1; }
                        else { bx = dx[j]; by = dy[j]; bn = 1; }
                    }
                }
                const bool first = an >= bn;
                if ((first ? an : bn) >= kFillVotes) {
                    nx[i] = first ? int8_t(ax) : int8_t(bx);
                    ny[i] = first ? int8_t(ay) : int8_t(by);
                }
            }
        }
        dx.swap(nx);
        dy.swap(ny);
    }
}

void warpInto(std::vector<uint8_t>& out, const uint8_t* src, const std::vector<int8_t>& dx,
              const std::vector<int8_t>& dy, int w, int h, float frac) {
    const size_t n = size_t(w) * h;
    if (out.size() != n) out.resize(n);
    std::memcpy(out.data(), src, n);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * w + x;
            if (!dx[i] && !dy[i]) continue;
            const int sx = x - int(std::lround(frac * dx[i]));
            const int sy = y - int(std::lround(frac * dy[i]));
            if (sx < 0 || sx >= w || sy < 0 || sy >= h) continue;
            const uint8_t c = src[size_t(sy) * w + sx];
            if (c) out[i] = c;  // a sample landing on transparency keeps the base pixel
        }
    }
}

}  // namespace

const FrameMorpher::Field* FrameMorpher::field(const Flc& flc, int a, int b) {
    const uint64_t key = (uint64_t(uintptr_t(&flc)) << 20) ^ (uint64_t(uint32_t(a)) << 10) ^
                         uint64_t(uint32_t(b));
    const auto it = fields_.find(key);
    if (it != fields_.end()) return &it->second;

    const auto& frames = flc.frames();
    if (a < 0 || b < 0 || a == b || size_t(a) >= frames.size() || size_t(b) >= frames.size())
        return nullptr;
    const int w = flc.width(), h = flc.height();
    if (w <= 0 || h <= 0 || frames[size_t(a)].size() != size_t(w) * h ||
        frames[size_t(b)].size() != size_t(w) * h)
        return nullptr;

    if (bytes_ + size_t(w) * h * 2 > kFieldBudget) {  // simple cap: recompute rather than grow
        fields_.clear();
        bytes_ = 0;
    }
    Field f;
    matchFrames(frames[size_t(a)].data(), frames[size_t(b)].data(), w, h, f.dx, f.dy);
    bytes_ += f.dx.size() * 2;
    return &fields_.emplace(key, std::move(f)).first->second;
}

const uint8_t* FrameMorpher::between(const Flc& flc, int a, int b, float t,
                                     std::vector<uint8_t>& scratch) {
    const int w = flc.width(), h = flc.height();
    if (w <= 0 || h <= 0 || size_t(w) * h > 128u * 128u) return nullptr;  // keep the cost bounded
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const Field* f;
    if (t <= 0.5f) {  // grow A towards B ...
        f = field(flc, a, b);
        if (!f) return nullptr;
        warpInto(scratch, flc.frames()[size_t(a)].data(), f->dx, f->dy, w, h, t);
    } else {  // ... then pull B back towards A; both agree at t = 0.5
        f = field(flc, b, a);
        if (!f) return nullptr;
        warpInto(scratch, flc.frames()[size_t(b)].data(), f->dx, f->dy, w, h, 1.0f - t);
    }
    return scratch.data();
}
