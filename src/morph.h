// Render-only in-between frame generation for the experimental animation mode.
//
// Given two adjacent frames A and B of one FLC, produces intermediate pictures by *moving*
// the pixels that differ between the frames along a per-pixel displacement field (local patch
// matching, then a coherence fill); pixels that could not be matched keep their A colour until
// the halfway point and then show B. Colours are always copied, never blended, so the palette,
// the pixel-art edges and the transparency key all survive — which is why a plain cross-fade
// ghosts on these assets (adjacent pest poses change 10-17% of their pixels; see src/smooth.h).
//
// Frame indices, frame delays, cues, sounds and saves are untouched: this only changes what
// World::draw() puts on screen between the boundaries the logic already passed.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

class Flc;

class FrameMorpher {
public:
    // Picture of frame `a` moved fraction `t` (0..1) of the way to frame `b`, written into
    // `scratch` (resized as needed). Returns nullptr when there is no usable pair, in which
    // case the caller should draw frame `a` unchanged.
    const uint8_t* between(const Flc& flc, int a, int b, float t, std::vector<uint8_t>& scratch);

private:
    struct Field {  // per pixel of frame a: where its content sits in frame b
        std::vector<int8_t> dx, dy;
    };
    const Field* field(const Flc& flc, int a, int b);  // computed once per ordered pair
    std::unordered_map<uint64_t, Field> fields_;
    size_t bytes_ = 0;
};
