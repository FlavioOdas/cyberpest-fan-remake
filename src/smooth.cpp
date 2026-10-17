#include "smooth.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

float FramePose::alpha(uint32_t nowMs) const {
    if (!valid || durationMs == 0) return 1.0f;
    if (nowMs <= atMs) return 0.0f;
    const uint32_t elapsed = nowMs - atMs;
    return elapsed >= durationMs ? 1.0f : float(elapsed) / float(durationMs);
}

Vec3 FramePose::positionAt(uint32_t nowMs) const {
    const float a = alpha(nowMs);
    return Vec3(from.x + int(std::lround((to.x - from.x) * a)),
                from.y + int(std::lround((to.y - from.y) * a)), to.z);
}

bool FramePose::smoothable() const {
    return valid && std::abs(to.x - from.x) <= kMaxStep && std::abs(to.y - from.y) <= kMaxStep;
}
