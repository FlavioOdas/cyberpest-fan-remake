// The room's live objects: each runs a data-defined state machine (docs/re/state-commands.md).
// Mirrors the original's object (1000:5604 factory), runner (vtable 1038:3832) and sprite
// (vtable 1038:5054) classes, without the 16-bit plumbing.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "adm.h"
#include "flc.h"
#include "surface.h"
#include "wav.h"

class World;
struct Object;

// One command list from a state module, kept for later execution as a cue.
struct Command {
    int32_t symbol = 0;
    std::vector<int32_t> ints;
    const adm::FileRef* file = nullptr;
};

struct Sprite {
    const Flc* flc = nullptr;
    std::string name;
    const Sound* sound = nullptr;
    int frame = -1;          // -1 = not started
    bool loop = false;       // -183
    int loopCount = 0;       // -183 argument; 0 = forever
    int loopsDone = 0;
    bool holdLastFrame = false;  // -112 1
    bool finished = false;
    std::vector<std::pair<int, Command>> cues;  // (frame, command); frame -1 = at end
};

struct Event {
    int type = 0;   // 0 message, 1 click, 2/3 other mouse actions
    int32_t value = 0;
    int32_t state = 0;
    bool immediate = false;  // c
    bool anyFocus = true;    // d
};

struct HitArea {
    int32_t id = 0;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;  // relative to the sprite's top-left
    int32_t cursor = 0, pressedCursor = 0;
};

struct Runner {
    int32_t state = 0;
    Sprite sprites[3];      // intro (-306), main (-111), outro (-431)
    int current = -1;       // index into sprites of the playing sprite
    int32_t nextState = 0;  // -132 / -151
    bool pending = false;   // an event is waiting for the animation to end
    bool done = false;
    std::vector<Event> events;
    std::vector<HitArea> hitAreas;
};

struct Object {
    int32_t id = 0;
    const adm::Node* module = nullptr;
    int x = 0, y = 0, z = 0;
    bool paused = false;
    std::unordered_map<int32_t, int32_t> properties;
    std::unique_ptr<Runner> runner;
    std::vector<std::unique_ptr<Runner>> retired;  // replaced runners, freed after the tick
    const Flc* shown = nullptr;  // image drawn (persists after the runner ends)
    std::string shownName;
    int shownFrame = 0;
    uint32_t lastStep = 0;
};

class World {
public:
    using SoundPlayer = std::function<void(const Sound*)>;

    bool load(const adm::Database& db, int32_t roomId, int frameScale, std::string* error);
    void setSoundPlayer(SoundPlayer play) { playSound_ = std::move(play); }

    // Advances every object's animation to time `nowMs`.
    void tick(uint32_t nowMs);
    void draw(Surface& target) const;

    // Mouse: returns the cursor id to show at (x, y) (0 = default).
    int32_t hover(int x, int y) const;
    void click(int x, int y, uint32_t nowMs);
    // Delivers a message (type-0 event) to an object, as the -119 command does.
    void send(int32_t target, int32_t message, uint32_t nowMs);

    Object* find(int32_t id);
    // Debug listing: one line per object (id, state, position, depth, image, frame).
    std::string describe() const;

private:
    Object* spawn(int32_t moduleId, int x, int y, int z, uint32_t nowMs);
    void remove(int32_t id);
    void enterState(Object& obj, int32_t state, uint32_t nowMs, int depth = 0);
    void runCommand(Object& obj, Runner& r, const Command& c, uint32_t nowMs);
    void step(Object& obj, uint32_t nowMs);
    void startSprite(Object& obj, Runner& r, int index);
    void runnerFinished(Object& obj, uint32_t nowMs);
    void sendEvent(Object& obj, int type, int32_t value, uint32_t nowMs);
    const Flc* image(const adm::FileRef& file);
    const Sound* sound(const adm::FileRef& file);
    bool hitTest(const Object& obj, int x, int y, const HitArea** area) const;

    const adm::Database* db_ = nullptr;
    int frameScale_ = 1;
    std::vector<std::unique_ptr<Object>> objects_;
    std::vector<int32_t> removed_;
    std::unordered_map<uint32_t, std::unique_ptr<Flc>> images_;
    std::unordered_map<uint32_t, std::unique_ptr<Sound>> sounds_;
    SoundPlayer playSound_;
};
