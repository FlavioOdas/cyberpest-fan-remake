#include "room_objects.h"

#include <cstdlib>

#include "world.h"

namespace {
int rnd(int lo, int hi) { return lo + std::rand() % (hi - lo + 1); }  // Random 1038:5546: rand() % (hi + 1 - lo) + lo
}  // namespace

// ---------------------------------------------------------------------------------------------
// Mouse (-91)

bool Mouse::create(const Vec3& pos, bool facing) {  // 1008:77da
    if (!ShelfItem::create(pos, facing)) return false;
    first_ = true;
    moduleInt(-318, 0, &leftX_);
    moduleInt(-318, 1, &rightX_);
    moduleInt(-319, 0, &start_.x), moduleInt(-319, 1, &start_.y), moduleInt(-319, 2, &start_.z);
    moduleInt(-320, 0, &hole_.x), moduleInt(-320, 1, &hole_.y), moduleInt(-320, 2, &hole_.z);
    return true;
}

Vec3 Mouse::nextTarget() const {  // 1008:7992
    const GameConfig& cfg = world_.config();
    Vec3 t;
    if (position().x == rightX_ || first_) {  // out of the hole or off the right: run off left
        t.y = rnd(cfg.floorMinY, cfg.floorMaxY);
        t.z = depthFromY(t.y);
        t.x = leftX_;
    } else if (position().x == leftX_) {  // off the left: home, or across to the right
        if (rnd(0, 1) == 0) {
            t = hole_;
        } else {
            t.y = rnd(cfg.floorMinY, cfg.floorMaxY);
            t.z = depthFromY(t.y);
            t.x = rightX_;
        }
    } else {  // somewhere in the room: keep going the way it faces
        t = position();
        t.x = facing() ? rightX_ : leftX_;
    }
    return t;
}

void Mouse::runTo(const Vec3& target) {  // 1008:7b9c
    setFacing(position().x < target.x);
    setWalkTarget(target);
    enterState(-325);
}

void Mouse::onStateStart() {  // 1008:7c2e
    if (state() == -324) {
        runTo(nextTarget());
        first_ = false;
    }
}

void Mouse::onStateEnd() {  // 1008:7c9e
    if (state() != -325) return;
    if (position() == hole_) enterState(-328);  // back in the hole
    else runTo(nextTarget());
}

void Mouse::onSpriteStart(Sprite&) {  // 1008:7d54
    if (walking() && playingMain()) walkStep();
}

void Mouse::onLastFrame(Sprite& s) {  // 1008:7db4
    if (walking() && playingMain()) {
        if (position() != walkTarget()) {
            walkStep();
            setLoop(currentSprite(), true, 0);
        } else {
            setLoop(currentSprite(), false, 0);
            stopWalking();
        }
    }
    ShelfItem::onLastFrame(s);
}

void Mouse::onEvent(GameObject* from, int32_t value, int type) {  // 1008:7eae
    if (type == 0 && value == -327) {  // caught by the pest
        stopWalking();
    } else if (type == 0 && value == -1) {  // gone: back to waiting in the hole
        enterState(-321);
        return;
    }
    ShelfItem::onEvent(from, value, type);
}

// ---------------------------------------------------------------------------------------------
// Audio button (-94)

void AudioButton::onStateStart() {  // 1010:06f8
    switch (state()) {
    case -164: world_.setAudioEnabled(false); break;
    case -163: world_.setAudioEnabled(true); break;
    case -162: enterState(world_.audioEnabled() ? -163 : -164); break;  // initial: show the setting
    default: break;
    }
}
