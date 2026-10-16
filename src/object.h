// Room objects and their data-driven state machines. Mirrors the original classes:
// GameObject = generic object (class -107, vtable 1038:40de), Runner (1038:2c28),
// Sprite (1038:5570). Hook slots and field offsets: docs/re/engine-runtime.md.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "adm.h"
#include "flc.h"
#include "vec.h"
#include "wav.h"

class World;
class GameObject;
class Surface;

// A command scheduled on a sprite's timeline (1000:7be4).
struct Cue {
    enum Kind { Send, Move, Spawn, SetProperty, Remove, Hide, Show };
    int frame = 0;  // -1 = when the animation ends
    Kind kind = Send;
    std::vector<int32_t> args;
    Vec3 vector;  // Move
};

struct Sprite {
    const Flc* flc = nullptr;         // image list value 0
    const Flc* flcFacing = nullptr;   // value 1: pre-drawn variant used while facing (+0xb4) is set
    std::string name;
    const Sound* sound = nullptr;
    int frame = 0;               // +0x62 current frame
    bool started = false;
    bool loop = false;           // +0x94 (-183)
    int loopCount = 0;           // +0x98, 0 = forever
    int loopsDone = 0;           // +0x9a
    bool holdLastFrame = false;  // +0x96 (-112)
    bool ended = false;          // +0xc4
    Vec3 offset;                 // +0x9c draw offset (-548)
    Vec3 motion;                 // +0xa6 displacement over one cycle (-326)
    Vec3 endMove;                // +0xb0 displacement when the animation ends (-455)
    Vec3 applied;                // +0xba part of `motion` already applied this cycle
    std::vector<Cue> cues;       // +0xc6
    uint32_t nextFrameAt = 0;

    int frameCount() const { return flc ? flc->frameCount() : 0; }
    const Flc* image(bool facing) const { return facing && flcFacing ? flcFacing : flc; }
};

struct Event {
    int type = 0;            // 0 message, 1 button down, 2 button up, 3 hover, 4 ?
    int32_t value = 0;       // message symbol or hit area id
    int32_t state = 0;       // state to enter
    bool immediate = false;  // c: switch now instead of letting the animation finish
    bool anyFocus = true;    // d: fire even when another object has the focus
};

struct HitArea {
    int32_t id = 0;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;  // relative to the sprite's top-left
    int32_t cursor = 0, pressedCursor = 0;
};

// One entered state (1038:2c28): its sprites, events and hit areas.
struct Runner {
    int32_t state = 0;
    std::unique_ptr<Sprite> sprites[3];  // intro (-306) +0x2e, main (-111) +0x32, outro (-431) +0x36
    Sprite* current = nullptr;           // +0x3a
    int32_t nextState = 0;               // +0x46
    bool pending = false;                // +0x44
    bool done = false;                   // +0x42
    std::vector<Event> events;           // +0x20
    std::vector<HitArea> hitAreas;       // +0x12
    Point2 hitOffset;                    // +0x0a (-157), subtracted from the hit point

    Sprite* intro() const { return sprites[0].get(); }
    Sprite* main() const { return sprites[1].get(); }
    Sprite* outro() const { return sprites[2].get(); }
};

class GameObject {
public:
    GameObject(World& world, int32_t id, const adm::Node* module) : world_(world), id_(id), module_(module) {}
    virtual ~GameObject() = default;

    int32_t id() const { return id_; }
    const adm::Node* module() const { return module_; }
    World& world() const { return world_; }

    // ---- vtable hooks (original slot in brackets) ----
    virtual bool create(const Vec3& pos, bool facing);              // +0x14
    virtual bool activate();                                        // +0x18
    virtual void cueSend(int32_t target, int32_t message);          // +0x1c
    virtual void cueMove(int32_t target, const Vec3& delta);        // +0x20
    virtual void cueSpawn(int32_t module, const Vec3& offset);      // +0x24
    virtual void cueRemove(int32_t target);                         // +0x28
    virtual void cueHide(int32_t target);                           // +0x2c
    virtual void cueShow(int32_t target);                           // +0x30
    virtual void onStateStart() {}                                  // +0x34
    virtual void onStateEnd() {}                                    // +0x38
    virtual void onSpriteStart(Sprite&) {}                          // +0x3c
    virtual void onSpriteLooped(Sprite&) {}                         // +0x40
    virtual void onLastFrame(Sprite&) {}                            // +0x44
    virtual void onSpriteFinished(Sprite&) {}                       // +0x48
    virtual void onFrame();                                         // +0x4c
    virtual void onHover(Point2 p);                                 // +0x50
    virtual void onMouseMove(Point2 p);                             // +0x54
    virtual void onButtonDown(Point2 p);                            // +0x58
    virtual void onButtonUp(Point2 p);                              // +0x5c
    virtual void onCancel();                                        // +0x60
    virtual void sendMessage(int32_t message, GameObject* from);    // +0x64
    virtual void onEvent(GameObject* from, int32_t value, int type);// +0x68
    // Text typed into an object (megaphone speech bubble, 1038:6498); empty for most classes.
    virtual std::string text() const { return {}; }
    virtual void onTextChar(wchar_t) {}
    virtual void drawOverlay(Surface&, const Palette&) const {}

    // ---- engine services used by object classes ----
    void enterState(int32_t state);  // 1000:6100
    void enterStateAt(int ordinal);
    int32_t state() const { return state_; }
    Runner* runner() const { return runner_.get(); }
    void setProperty(int32_t symbol, int32_t value);  // 1000:63ee
    int32_t property(int32_t symbol) const;           // 1000:67c8
    const Vec3& position() const { return pos_; }
    void setPosition(const Vec3& p) { pos_ = p; }
    void moveBy(const Vec3& d);                       // 1038:2a10 (moves attached children too)
    bool facing() const { return facing_; }           // +0xb4, selects the pre-drawn facing images
    void setFacing(bool f) { facing_ = f; }           // 1038:5b24
    bool held() const { return held_; }               // +0x54
    bool visible() const { return visible_; }         // +0xb2 (1000:7b80)
    GameObject* parent() const { return parent_; }
    void setParent(GameObject* p) { parent_ = p; }
    int depthFromY(int y) const;                      // 1000:77b6
    int yFromDepth(int z) const;                      // 1000:77ea

    // Walking (+0x76 walking, +0x78 target, +0x82 velocity).
    bool walking() const { return walking_; }                    // 1038:5a86
    const Vec3& walkTarget() const { return walkTarget_; }       // 1038:5ad2
    void setWalkTarget(const Vec3& t);                           // 1000:73f6
    void setWalkTargetOnly(const Vec3& t) { walkTarget_ = t; }   // 1038:5aa4
    void stopWalking() { walking_ = false; }                     // 1038:5a66
    void walkStep();                                             // 1000:7460
    void setAttached(bool a) { attached_ = a; }                  // 1038:5b8e (+0xc0)
    // Loop control of a sprite (1038:56ae): enabled, count (0 = forever).
    static void setLoop(Sprite* s, bool enabled, int count);
    Sprite* mainSprite() const { return runner_ ? runner_->main() : nullptr; }
    Sprite* currentSprite() const { return runner_ ? runner_->current : nullptr; }
    bool playingMain() const { return runner_ && runner_->current && runner_->current == runner_->main(); }
    // Module data: value `index` of the top-level list with id `listId` (Mod_GetInt).
    bool moduleInt(int32_t listId, int index, int* out) const;
    // First int of a child module (1018:8f6c with a one-element path).
    bool childFirstInt(int32_t childId, int* out) const;
    int32_t heightAboveFloor() const;                            // 1000:781e

    // Drawing/hit testing helpers for World.
    const Sprite* shownSprite() const { return shown_; }
    bool hitTest(Point2 p, const HitArea** area) const;
    void step(uint32_t nowMs);  // advance the current sprite (1000:7df8)
    void retireRunners() { retired_.clear(); }

protected:
    GameObject* findTarget(int32_t id);  // 1000:6982
    void runMouse(Point2 p, int type);  // 1000:acc0
    // Runner callbacks
    void runnerFinishedSprite(Runner& r);  // 1000:a4f2
    void startSprite(Runner& r, Sprite* s);

    World& world_;
    int32_t id_;
    const adm::Node* module_;
    Vec3 pos_;                            // +0xa8
    int32_t state_ = 0;                   // +0x52
    std::unique_ptr<Runner> runner_;      // +0x4e
    std::vector<std::unique_ptr<Runner>> retired_;  // +0x9a, freed after the tick
    std::unordered_map<int32_t, int32_t> properties_;  // +0x8c
    bool held_ = false;       // +0x54 (-182)
    bool noFloorClamp_ = false;  // +0x56 (-181)
    bool falling_ = false;    // +0x6a (-309)
    int fallStartY_ = 0;      // +0x6c
    bool visible_ = true;     // +0xb2 (-322)
    bool facing_ = false;     // +0xb4 (-323)
    bool flagB6_ = false;     // +0xb6 (-180)
    bool fallsWhenReleased_ = false;  // +0xb8 (-279)
    bool flagBA_ = false;     // +0xba (-122)
    Point2 grabOffset_;       // +0x62
    Vec3 dragOrigin_;         // +0x58, position saved when picked up
    GameObject* parent_ = nullptr;  // +0xbc
    bool attached_ = false;   // +0xc0
    bool walking_ = false;    // +0x76
    Vec3 walkTarget_;         // +0x78
    Vec3 walkVelocity_;       // +0x82
    const Sprite* shown_ = nullptr;
    int enterDepth_ = 0;

private:
    void enterStateModule(const adm::Node& module, int32_t state);
    void runCommands(Runner& r, const adm::Node& state);  // State_Run 1000:8d10
    void runCue(const Cue& c);
    void fireEvent(const Event& e);  // 1000:b44c
};
