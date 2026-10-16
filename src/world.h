// The room ("screen" in the original, segment 1): owns the objects, dispatches input to them
// in depth order with focus/cursor handling, and provides shared services (assets, config).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "adm.h"
#include "flc.h"
#include "object.h"
#include "surface.h"
#include "vec.h"
#include "wav.h"

struct GameConfig {  // config module -42 (App_LoadConfig 1000:3584)
    int floorMaxY = 359;  // -44 (theApp+0x6ce)
    int floorMinY = 224;  // -45 (theApp+0x6d0)
    int config46 = 50;    // -46 (theApp+0x6d2) [meaning ?]
    int gravity = 40;     // -47 (theApp+0x6d4), multiplied by frameScale [inf]
    int frameScale = 1;   // -49 (theApp+0x6d6): 1 high spec, 2 low spec
};

class World {
public:
    using SoundPlayer = std::function<void(const Sound*)>;
    using AppMessage = std::function<void(int32_t)>;  // messages sent to target -117

    bool load(const adm::Database& db, int32_t roomId, std::string* error);
    void setSoundPlayer(SoundPlayer play) { playSound_ = std::move(play); }
    void setAppMessageHandler(AppMessage handler) { appMessage_ = std::move(handler); }

    // Per tick: advance animations, run per-frame hooks, deliver the hover event (1000:07c6).
    void tick(uint32_t nowMs);
    void draw(Surface& target) const;

    // Input in 640x480 coordinates (1000:103c / 1000:1206 / 1000:135c).
    void mouseMove(int x, int y);
    void buttonDown(int x, int y);
    void buttonUp(int x, int y);
    void mouseLeave();
    int32_t cursor() const { return cursor_; }
    bool buttonIsDown() const { return buttonDown_; }
    Point2 mouse() const { return mouse_; }

    // Services for objects.
    GameObject* find(int32_t id) const;
    GameObject* spawn(int32_t moduleId, const Vec3& pos, bool facing);
    void remove(GameObject* obj);
    void sendToApp(int32_t message) {
        if (appMessage_) appMessage_(message);
    }
    void send(int32_t target, int32_t message);  // deliver a message (type-0 event)
    const adm::Database& db() const { return *db_; }
    const GameConfig& config() const { return config_; }
    uint32_t now() const { return now_; }
    GameObject* focus() const { return focus_; }
    void takeFocus(GameObject* obj) { focus_ = obj; }
    void setCursor(int32_t id) { cursor_ = id; }
    const Flc* image(const adm::FileRef& file);
    const Sound* sound(const adm::FileRef& file);
    void playSound(const Sound* s) {
        if (playSound_ && s) playSound_(s);
    }
    std::string describe() const;

private:
    std::unique_ptr<GameObject> makeObject(int32_t moduleId, const adm::Node* module);  // 1000:5604
    void sortByDepth();
    template <typename F>
    void broadcast(F&& f);

    const adm::Database* db_ = nullptr;
    GameConfig config_;
    std::vector<std::unique_ptr<GameObject>> objects_;  // front (highest z) first
    std::vector<std::unique_ptr<GameObject>> removed_;
    std::unordered_map<uint32_t, std::unique_ptr<Flc>> images_;
    std::unordered_map<uint32_t, std::unique_ptr<Sound>> sounds_;
    SoundPlayer playSound_;
    AppMessage appMessage_;
    GameObject* focus_ = nullptr;
    int32_t cursor_ = 0;
    Point2 mouse_{-1, -1};
    bool buttonDown_ = false;
    uint32_t now_ = 0;
};
