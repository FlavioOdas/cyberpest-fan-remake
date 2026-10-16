#include "object.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "symbols.h"
#include "world.h"

namespace {

// State command symbols (docs/re/state-commands.md).
enum : int32_t {
    kSet = -2, kSetAt = -3, kRemove = -4, kRemoveAt = -5, kSpawn = -6, kSpawnAt = -7,
    kHide = -8, kHideAt = -9, kShow = -10, kShowAt = -11, kOnClick = -12, kOnRelease = -13,
    kOnHover = -170, kOnMessage = -121, kIntro = -306, kMain = -111, kOutro = -431,
    kHoldLast = -112, kLoop = -183, kHitArea = -114, kSound = -116, kSend = -119, kSendAt = -176,
    kNext = -132, kRandomNext = -151, kMotion = -326, kEndMove = -455, kOffset = -548, kMoveRange = -493,
    kHitOffset = -157,
};

// Messages and properties used by the base object.
enum : int32_t {
    kTargetApp = -117, kTargetSelf = -204, kTargetParent = -485,
    kMsgRemoved = -41, kMsgRemoveSelf = -1, kMsgDropped = -184, kMsgLandedSoft = -449, kMsgLandedHard = -450,
    kMsgCleaned = -136, kHoverClean = -133, kSponge = -77,
    kPropHeld = -182, kPropNoFloorClamp = -181, kPropFalling = -309, kPropVisible = -322, kPropFacing = -323,
    kPropPickable = -180, kPropFallsWhenReleased = -279, kPropCleanable = -122,
};

struct Args {
    std::vector<int32_t> ints;
    const adm::FileRef* file = nullptr;
    int operator[](size_t i) const { return i < ints.size() ? ints[i] : 0; }
    int get(size_t i, int fallback) const { return i < ints.size() ? ints[i] : fallback; }
};

int scaled(int value, int numerator, int denominator) {
    return denominator ? int(int64_t(value) * numerator / denominator) : 0;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Creation and state entry

bool GameObject::create(const Vec3& pos, bool facing) {  // 1000:5e1a
    pos_ = pos;
    facing_ = facing;
    return true;
}

bool GameObject::activate() {  // 1000:5f02
    for (const adm::Node& c : module_->children) {
        if (c.tag == adm::Tag::Map && c.ref == sym::ClassState) {
            enterState(c.id);
            return true;
        }
        if (c.tag == adm::Tag::List && c.ref == kSet) {
            const std::vector<int32_t> p = adm::intValues(c);
            if (p.size() >= 2) setProperty(p[0], p[1]);
        }
    }
    return false;
}

void GameObject::enterState(int32_t state) {  // 1000:6100
    const adm::Node* module = adm::childModule(*module_, state);
    if (module) enterStateModule(*module, state);
}

void GameObject::enterStateAt(int ordinal) {
    for (const adm::Node& module : module_->children) {
        if (module.tag != adm::Tag::Map || module.ref != sym::ClassState) continue;
        if (ordinal-- == 0) {
            enterStateModule(module, module.id);
            return;
        }
    }
}

void GameObject::enterStateModule(const adm::Node& module, int32_t state) {
    if (enterDepth_ > 16) return;  // states without sprites can chain
    ++enterDepth_;
    if (runner_) {
        shown_ = nullptr;
        retired_.push_back(std::move(runner_));
    }
    state_ = state;
    runner_ = std::make_unique<Runner>();
    Runner& r = *runner_;
    r.state = state;
    runCommands(r, module);

    // End of State_Run (1000:a27c): start the first sprite, or move straight on.
    Sprite* first = r.intro() ? r.intro() : r.main() ? r.main() : r.outro();
    if (!first) {
        if (r.nextState) {
            onStateStart();
            r.done = true;
            onStateEnd();
            if (runner_.get() == &r) enterState(r.nextState);
        } else {
            onStateStart();
        }
        --enterDepth_;
        return;
    }
    startSprite(r, first);
    onStateStart();
    if (runner_.get() == &r && r.current) onSpriteStart(*r.current);
    --enterDepth_;
}

void GameObject::startSprite(Runner& r, Sprite* s) {
    r.current = s;
    s->frame = 0;
    s->started = false;
    s->ended = false;
    s->applied = Vec3();
    s->nextFrameAt = world_.now();
    shown_ = s;
}

void GameObject::runCommands(Runner& r, const adm::Node& state) {  // State_Run 1000:8d10
    const int frameScale = world_.config().frameScale;
    Sprite* s = nullptr;  // commands after a sprite-creating command attach to that sprite
    for (const adm::Node& list : state.children) {
        if (list.tag != adm::Tag::List) continue;
        Args a{adm::intValues(list), adm::fileValue(list)};
        const int32_t cmd = list.ref;
        auto timed = [&](Cue::Kind kind, size_t argOffset) {
            if (!s || a.ints.empty()) return;
            Cue c;
            c.frame = a[0] / frameScale;
            c.kind = kind;
            c.args.assign(a.ints.begin() + argOffset, a.ints.end());
            s->cues.push_back(std::move(c));
        };
        auto untimed = [&](Cue::Kind kind) {
            Cue c;
            c.frame = -1;
            c.kind = kind;
            c.args = a.ints;
            if (s) s->cues.push_back(std::move(c));  // after a sprite: when the animation ends
            else runCue(c);
        };
        switch (cmd) {
        case kIntro:
        case kMain:
        case kOutro: {
            const int slot = cmd == kIntro ? 0 : cmd == kMain ? 1 : 2;
            r.sprites[slot] = std::make_unique<Sprite>();
            s = r.sprites[slot].get();
            if (a.file) {
                s->flc = world_.image(*a.file);
                s->name = a.file->name;
            }
            if (const adm::FileRef* f2 = adm::fileValue(list, 1)) s->flcFacing = world_.image(*f2);
            break;
        }
        case kHoldLast:
            if (s) s->holdLastFrame = a[0] != 0;
            break;
        case kLoop:
            if (s) {
                s->loop = true;
                s->loopCount = a[0];
                s->loopsDone = 0;
            }
            break;
        case kSound:
            if (s && a.file) s->sound = world_.sound(*a.file);
            break;
        case kMotion:
            if (s) s->motion = Vec3(a[0], a[1], a.get(2, 0));
            break;
        case kEndMove:
            if (s) s->endMove = Vec3(a[0], a[1], a.get(2, 0));
            break;
        case kOffset:
            if (s) s->offset = Vec3(a[0], a[1], a.get(2, 0));
            break;
        case kMoveRange: {  // 1000:8d10 → 850a: [t0, t1, target, dx, dy, (dz)], spread over frames t0..t1
            if (!s) break;
            const int t0 = a[0] / frameScale, t1 = a[1] / frameScale;
            const int target = a[2];
            Vec3 total(a[3], a[4], a.get(5, 0));
            if (facing_) total.x = -total.x;  // 1038:52b8, at state entry
            const int n = std::max(1, t1 - t0 + 1);
            Vec3 done;
            for (int i = 0; i < n; ++i) {
                const Vec3 upto(scaled(total.x, i + 1, n), scaled(total.y, i + 1, n), scaled(total.z, i + 1, n));
                Cue c;
                c.frame = t0 + i;
                c.kind = Cue::Move;
                c.args = {target};
                c.vector = upto - done;
                done = upto;
                s->cues.push_back(std::move(c));
            }
            break;
        }
        case kHitOffset:
            r.hitOffset = {a[0], a[1]};
            break;
        case kHitArea:
            r.hitAreas.push_back({a[0], a[1], a[2], a[3], a[4], a[5], a[6]});
            break;
        case kOnMessage:
            r.events.push_back({0, a[0], a[1], a[2] != 0, true});
            break;
        case kOnClick:
        case kOnRelease:
        case kOnHover:
            r.events.push_back({cmd == kOnClick ? 1 : cmd == kOnRelease ? 2 : 3, a[0], a[1], a[2] != 0, a.get(3, 1) != 0});
            break;
        case kNext:
            r.nextState = a[0];
            break;
        case kRandomNext:
            if (!a.ints.empty()) r.nextState = a.ints[size_t(std::rand()) % a.ints.size()];
            break;
        case kSet: untimed(Cue::SetProperty); break;
        case kRemove: untimed(Cue::Remove); break;
        case kSpawn: untimed(Cue::Spawn); break;
        case kHide: untimed(Cue::Hide); break;
        case kShow: untimed(Cue::Show); break;
        case kSend: untimed(Cue::Send); break;
        case kSetAt: timed(Cue::SetProperty, 1); break;
        case kRemoveAt: timed(Cue::Remove, 1); break;
        case kSpawnAt: timed(Cue::Spawn, 1); break;
        case kHideAt: timed(Cue::Hide, 1); break;
        case kShowAt: timed(Cue::Show, 1); break;
        case kSendAt: timed(Cue::Send, 1); break;
        default:
            break;  // the original loader (1000:8d10) knows no other commands
        }
    }
}

void GameObject::runCue(const Cue& c) {  // 1000:7be4
    auto arg = [&](size_t i) { return i < c.args.size() ? c.args[i] : 0; };
    switch (c.kind) {
    case Cue::Send: cueSend(arg(0), arg(1)); break;
    case Cue::Move: cueMove(arg(0), c.vector); break;
    case Cue::Spawn: cueSpawn(arg(0), Vec3(arg(1), arg(2), arg(3))); break;
    case Cue::SetProperty: setProperty(arg(0), arg(1)); break;
    case Cue::Remove: cueRemove(arg(0)); break;
    case Cue::Hide: cueHide(arg(0)); break;
    case Cue::Show: cueShow(arg(0)); break;
    }
}

// ---------------------------------------------------------------------------------------------
// Cue/command hooks

GameObject* GameObject::findTarget(int32_t id) {  // 1000:6982
    if (id == kTargetSelf) return this;
    if (id == kTargetParent) return parent_;
    return world_.find(id);
}

void GameObject::cueSend(int32_t target, int32_t message) {  // 1000:69e0
    if (target == kTargetApp) {
        world_.sendToApp(message);
        return;
    }
    if (GameObject* t = findTarget(target)) t->sendMessage(message, this);
}

void GameObject::cueMove(int32_t target, const Vec3& delta) {  // 1000:6a52
    if (GameObject* t = findTarget(target)) t->moveBy(delta);
}

void GameObject::cueSpawn(int32_t module, const Vec3& offset) {  // 1000:6aa0
    Vec3 d = offset;
    if (facing_) d.x = -d.x;  // offsets follow the spawner's facing (1038:298e)
    world_.spawn(module, pos_ + d, facing_);
}

void GameObject::cueRemove(int32_t target) {  // 1000:6b9a
    if (GameObject* t = findTarget(target)) {
        t->sendMessage(kMsgRemoved, this);
        world_.remove(t);
    }
}

void GameObject::cueHide(int32_t target) {  // 1000:6c02
    if (GameObject* targetObject = findTarget(target)) targetObject->setProperty(kPropVisible, 0);
}

void GameObject::cueShow(int32_t target) {  // 1000:6c4a
    if (GameObject* targetObject = findTarget(target)) targetObject->setProperty(kPropVisible, 1);
}

// ---------------------------------------------------------------------------------------------
// Properties and position

void GameObject::setProperty(int32_t symbol, int32_t value) {  // 1000:63ee
    switch (symbol) {
    case sym::X: pos_.x = value; break;
    case sym::Y: pos_.y = value; break;
    case sym::Z: pos_.z = value; break;
    case kPropFacing: facing_ = value != 0; break;
    case kPropVisible: visible_ = value != 0; break;
    case kPropFalling:
        falling_ = value != 0;
        if (falling_) fallStartY_ = pos_.y;
        break;
    case kPropFallsWhenReleased: fallsWhenReleased_ = value != 0; break;
    case kPropHeld:
        held_ = value != 0;
        if (held_) {
            dragOrigin_ = pos_;
            grabOffset_ = {world_.mouse().x - pos_.x, world_.mouse().y - pos_.y};  // +0x62
        }
        break;
    case kPropNoFloorClamp: noFloorClamp_ = value != 0; break;
    case kPropPickable: flagB6_ = value != 0; break;
    case kPropCleanable: flagBA_ = value != 0; break;
    default: properties_[symbol] = value; break;
    }
}

int32_t GameObject::property(int32_t symbol) const {  // 1000:67c8
    switch (symbol) {
    case sym::X: return pos_.x;
    case sym::Y: return pos_.y;
    case sym::Z: return pos_.z;
    case kPropFacing: return facing_;
    case kPropVisible: return visible_;
    case kPropFalling: return falling_;
    case kPropFallsWhenReleased: return fallsWhenReleased_;
    case kPropHeld: return held_;
    case kPropNoFloorClamp: return noFloorClamp_;
    case kPropPickable: return flagB6_;
    case kPropCleanable: return flagBA_;
    default: {
        auto it = properties_.find(symbol);
        return it == properties_.end() ? 0 : it->second;
    }
    }
}

void GameObject::moveBy(const Vec3& d) {  // 1038:2a10: an attached object drags its parent along
    pos_ += d;
    if (attached_ && parent_) parent_->moveBy(d);
}

void GameObject::setWalkTarget(const Vec3& t) {  // 1000:73f6
    walking_ = true;
    walkTarget_ = t;
    walkVelocity_ = Vec3();
}

void GameObject::walkStep() {  // 1000:7460
    if (!walking_ || !playingMain()) return;
    Sprite& s = *runner_->current;
    if (walkVelocity_ == Vec3()) walkVelocity_ = s.motion;  // one cycle of the walk animation
    const Vec3 d = walkTarget_ - pos_;
    const int vx = std::abs(walkVelocity_.x);
    const int cycles = vx ? (std::abs(d.x) - 1) / vx + 1 : 1;
    Vec3 step(d.x / std::max(cycles, 1), d.y / std::max(cycles, 1), d.z / std::max(cycles, 1));
    // step() mirrors x while facing; store the motion so that it points at the target.
    if (facing_) step.x = -step.x;
    s.motion = step;
}

void GameObject::setLoop(Sprite* s, bool enabled, int count) {  // 1038:56ae
    if (!s) return;
    s->loop = enabled;
    s->loopCount = count;
    s->loopsDone = 0;
}

bool GameObject::moduleInt(int32_t listId, int index, int* out) const {
    for (const adm::Node& c : module_->children) {
        if (c.tag == adm::Tag::List && c.id == listId) {
            const std::vector<int32_t> v = adm::intValues(c);
            if (size_t(index) >= v.size()) return false;
            *out = v[size_t(index)];
            return true;
        }
    }
    return false;
}

bool GameObject::childFirstInt(int32_t childId, int* out) const {
    for (const adm::Node& c : module_->children) {
        if (c.id != childId) continue;
        if (c.tag == adm::Tag::List) {
            const std::vector<int32_t> v = adm::intValues(c);
            if (v.empty()) return false;
            *out = v[0];
            return true;
        }
        for (const adm::Node& l : c.children) {  // a state: its first plain data list
            if (l.tag == adm::Tag::List && l.ref == 0) {
                const std::vector<int32_t> v = adm::intValues(l);
                if (v.empty()) return false;
                *out = v[0];
                return true;
            }
        }
        return false;
    }
    return false;
}

Point2 GameObject::spriteSize() const {  // 1000:6088
    const Sprite* s = runner_ && runner_->current ? runner_->current : nullptr;
    const Flc* f = s ? s->image(facing_) : nullptr;
    return f ? Point2{f->width(), f->height()} : Point2{0, 0};
}

int32_t GameObject::heightAboveFloor() const {  // 1000:781e
    return world_.config().floorMinY - pos_.y + pos_.z - 3000;
}

int GameObject::depthFromY(int y) const { return y + 3000 - world_.config().floorMinY; }
int GameObject::yFromDepth(int z) const { return z - 3000 + world_.config().floorMinY; }

// ---------------------------------------------------------------------------------------------
// Per-frame animation

void GameObject::step(uint32_t nowMs) {  // Sprite_OnFrame 1000:7df8
    Runner* rp = runner_.get();
    if (!rp || !rp->current || rp->done) {
        onFrame();
        return;
    }
    Runner& r = *rp;
    Sprite& s = *r.current;
    if (!s.flc || nowMs < s.nextFrameAt) return;
    s.nextFrameAt = nowMs + uint32_t(std::max(s.flc->frameDelayMs(), 20));
    if (s.started) ++s.frame;
    s.started = true;
    const int n = s.frameCount();
    if (s.frame >= n) s.frame = n - 1;
    shown_ = &s;
    auto stillCurrent = [&] { return runner_.get() == &r && r.current == &s; };

    if (s.frame == 0) world_.playSound(s.sound);  // sound restarts with every cycle
    for (size_t i = 0; i < s.cues.size() && stillCurrent(); ++i) {
        if (s.cues[i].frame == s.frame) runCue(s.cues[i]);
    }
    if (!stillCurrent()) return;
    if (s.motion != Vec3()) {  // spread the cycle's displacement over its frames
        Vec3 upto(scaled(s.motion.x, s.frame + 1, n), scaled(s.motion.y, s.frame + 1, n), scaled(s.motion.z, s.frame + 1, n));
        if (facing_) upto.x = -upto.x;
        moveBy(upto - s.applied);
        s.applied = upto;
    }
    onFrame();
    if (!stillCurrent() || s.frame < n - 1 || s.ended) return;

    s.ended = true;
    onLastFrame(s);
    if (!stillCurrent()) return;
    if (s.loop && (s.loopCount == 0 || ++s.loopsDone < s.loopCount)) {
        s.ended = false;
        s.started = false;
        s.frame = 0;
        s.applied = Vec3();
        onSpriteLooped(s);
        return;
    }
    for (size_t i = 0; i < s.cues.size() && stillCurrent(); ++i) {
        if (s.cues[i].frame == -1 || s.cues[i].frame == n) runCue(s.cues[i]);
    }
    if (!stillCurrent()) return;
    if (s.endMove != Vec3()) {
        Vec3 d = s.endMove;
        if (facing_) d.x = -d.x;
        moveBy(d);
    }
    runnerFinishedSprite(r);
}

void GameObject::runnerFinishedSprite(Runner& r) {  // Runner_OnSpriteFinished 1000:a4f2
    Sprite* finished = r.current;
    onSpriteFinished(*finished);
    if (runner_.get() != &r) return;
    Sprite* next = nullptr;
    if (finished == r.intro()) next = r.main() ? r.main() : r.outro();
    else if (finished == r.main()) next = r.outro();
    if (next) {
        startSprite(r, next);
        onSpriteStart(*next);
        return;
    }
    r.done = true;
    onStateEnd();
    if (runner_.get() == &r && r.nextState) enterState(r.nextState);
}

void GameObject::onFrame() {  // 1000:6c92: falling under gravity
    if (!falling_) return;
    const GameConfig& cfg = world_.config();
    pos_.y += cfg.gravity * cfg.frameScale;
    const int floorY = yFromDepth(pos_.z);
    if (floorY < pos_.y) {
        const int fallen = floorY - fallStartY_;
        pos_.y = floorY;
        setProperty(kPropFalling, 0);
        sendMessage(fallen < cfg.config46 ? kMsgLandedSoft : kMsgLandedHard, this);
    }
}

// ---------------------------------------------------------------------------------------------
// Input and events

bool GameObject::hitTest(Point2 p, const HitArea** area) const {  // transform from 1000:acc0
    if (!runner_) return false;
    const Sprite* s = runner_->main() ? runner_->main() : runner_->current;
    if (!s || !s->flc) return false;
    int lx = p.x - pos_.x, ly = p.y - pos_.y;  // relative to the anchor
    if (facing_) lx = -lx;
    lx -= runner_->hitOffset.x;
    ly -= runner_->hitOffset.y;
    lx += s->flc->width() / 2;  // to sprite top-left
    ly += (shown_ && shown_->flc ? shown_->flc->height() : s->flc->height());
    for (const HitArea& h : runner_->hitAreas) {
        if (lx >= h.x1 && lx < h.x2 && ly >= h.y1 && ly < h.y2) {
            *area = &h;
            return true;
        }
    }
    return false;
}

void GameObject::runMouse(Point2 p, int type) {  // 1000:acc0
    const HitArea* hit = nullptr;
    if (hitTest(p, &hit)) {
        if (!world_.focus()) world_.takeFocus(this);
        if (world_.focus() == this) {
            const int32_t c = world_.buttonIsDown() && hit->pressedCursor ? hit->pressedCursor : hit->cursor;
            if (c) world_.setCursor(c);
        }
        onEvent(nullptr, hit->id, type);
    } else {
        onEvent(nullptr, 0, type);
    }
}

void GameObject::onHover(Point2 p) { runMouse(p, 3); }  // 1000:718c

void GameObject::onMouseMove(Point2 p) {  // 1000:6f84: drag a held object
    if (held_) {
        const GameConfig& cfg = world_.config();
        Vec3 np(p.x - grabOffset_.x, p.y - grabOffset_.y, dragOrigin_.z);
        if (!noFloorClamp_) {
            if (np.y > cfg.floorMaxY) {  // dragged below the floor: drop it there
                np.z = cfg.floorMaxY - cfg.floorMinY + 3000;
                np.y = yFromDepth(np.z);
                pos_ = np;
                setProperty(kPropHeld, 0);
                sendMessage(kMsgDropped, this);
                sendMessage(kMsgLandedSoft, this);
            } else if (np.y > cfg.floorMinY) {
                np.z = depthFromY(np.y);
            }
        }
        pos_ = np;
    }
    runMouse(p, 3);
}

void GameObject::onButtonDown(Point2 p) { runMouse(p, 1); }  // 1000:71fa

void GameObject::onButtonUp(Point2 p) {  // 1000:7268
    if (held_) {
        setProperty(kPropHeld, 0);
        if (fallsWhenReleased_) setProperty(kPropFalling, 1);
        sendMessage(kMsgDropped, this);
    }
    runMouse(p, 2);
}

void GameObject::onCancel() {  // 1000:7324
    if (held_) {
        setProperty(kPropHeld, 0);
        if (fallsWhenReleased_) setProperty(kPropFalling, 1);
        sendMessage(kMsgDropped, this);
    }
}

void GameObject::sendMessage(int32_t message, GameObject* from) { onEvent(from, message, 0); }  // 1000:73b0

void GameObject::onEvent(GameObject* from, int32_t value, int type) {  // 1000:6e5c
    if (type == 3 && flagBA_ && value == kHoverClean) {
        GameObject* sponge = world_.find(kSponge);
        if (sponge && sponge->held()) sendMessage(kMsgCleaned, this);
    }
    if (type == 1 && flagB6_ && world_.focus() == this && value != 0) {  // pick it up
        sendMessage(kPropHeld, this);
        setProperty(kPropHeld, 1);
    }
    if (type == 0 && value == kMsgRemoveSelf) cueRemove(kTargetSelf);
    (void)from;
    if (!runner_) return;
    for (const Event& e : runner_->events) {  // Runner_MatchEvent 1000:b3b4
        if (e.type == type && e.value == value) {
            fireEvent(e);
            return;
        }
    }
}

void GameObject::fireEvent(const Event& e) {  // Runner_FireEvent 1000:b44c
    if (!e.anyFocus && world_.focus() != this) return;
    Runner& r = *runner_;
    if (!e.immediate && !r.done && r.current) {
        r.pending = true;
        r.nextState = e.state;
        r.current->loop = false;  // 1038:56ae(sprite, 0, 0)
        r.current->loopCount = 0;
        r.current->loopsDone = 0;
    } else {
        enterState(e.state);
    }
}
