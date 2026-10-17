// Render-only animation smoothing for the experimental 60 FPS mode.
//
// The game logic is untouched: GameObject::step() only *records* where an object was when the
// animation frame it is now showing began (FramePose). World::draw() then draws the object
// between the two frame boundaries the logic already passed — the picture gains real
// in-between positions at the full redraw rate, while frame advances, cues, sounds, AI and
// saves keep the original 85 ms timing.
//
// Frame blending (tweening the poses themselves) was implemented and reviewed against every
// walk asset in the ADM: adjacent frames of PMWALK, PMTIMID, PMSTALK, PMBACK, PMTIPTOE and
// PMHOPBAK change 10-17% of their pixels, so a blend ghosts (double bodies/heads) — the look
// the user rejected. Genuine new poses therefore need authored frames, not interpolation;
// see docs/CHECKPOINTS.md.
#pragma once

#include <cstdint>

#include "vec.h"

// Where an object was when the animation frame it is currently showing began. `from` is the
// position at the previous frame boundary, `to` the position after this one; the renderer
// lerps between them across atMs..atMs+durationMs. Like classic sprite interpolation this
// shows the transition one frame late (never predicts), so no logic state is needed.
struct FramePose {
    // A boundary that moved the object further than any animation does is a jump (an endMove
    // or teleport), not motion: such a frame keeps the original pop instead of gliding.
    static constexpr int kMaxStep = 64;

    Vec3 from, to;
    uint32_t atMs = 0;        // tick time the current frame started
    uint32_t durationMs = 0;  // how long the current frame is held (nextFrameAt - atMs)
    bool valid = false;

    float alpha(uint32_t nowMs) const;      // 0 at the boundary, 1 when the next frame is due
    Vec3 positionAt(uint32_t nowMs) const;  // lerp(from, to, alpha)
    bool smoothable() const;                // valid, and the step is plausible motion
};
