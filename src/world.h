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
#include "morph.h"
#include "object.h"
#include "surface.h"
#include "vec.h"
#include "wav.h"

struct GameConfig {  // config module -42 (App_LoadConfig 1000:3584)
    int floorMaxY = 359;  // -44 (theApp+0x6ce)
    int floorMinY = 224;  // -45 (theApp+0x6d0)
    int config46 = 50;    // -46 (theApp+0x6d2): falls shorter than this land softly (1000:6c92)
    int gravity = 40;     // -47 (theApp+0x6d4): fall step per frame = gravity * frameScale (1000:6c92)
    int frameScale = 1;   // -49 (theApp+0x6d6): 1 high spec, 2 low spec
};

// PEST.INI [Cheats] values that change the pest (App_ReadIniSettings 1000:28ce; theApp at
// DS:0x26dc, so these are DAT_1050_2992..29a6). The developer keys and trace switches are
// not ported; docs/re/engine-overview.md lists them.
struct Cheats {
    int neuroticism = -1, extroversion = -1;  // +0x2b6/+0x2b8: override when >= 0
    int goodStomach = 50, junkStomach = 0, bowels = 0, mentalState = 0;  // +0x2be..+0x2c4
    int gotFleas = 0, gotRabies = 0, onHeat = 0;                          // +0x2c6..+0x2ca
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
    void draw(Surface& target, const Palette& palette) const;

    // Input in 640x480 coordinates (1000:103c / 1000:1206 / 1000:135c).
    void mouseMove(int x, int y);
    void buttonDown(int x, int y);
    void buttonUp(int x, int y);
    void mouseLeave();
    void inputChar(wchar_t character);
    int32_t cursor() const { return cursor_; }
    bool buttonIsDown() const { return buttonDown_; }
    Point2 mouse() const { return mouse_; }

    // Services for objects.
    GameObject* find(int32_t id) const;
    std::vector<GameObject*> objectSnapshot() const;
    int count(int32_t id) const;                       // 1000:0cdc
    bool anyOtherWithProperty(const GameObject* object, int32_t property) const;
    void requestAttention(int32_t reason) {            // 1000:47f6: pop up over other apps
        if (attention_) attention_(reason);
    }
    void setAttentionHandler(std::function<void(int32_t)> h) { attention_ = std::move(h); }
    // Sound on/off (theApp+0xcc, INI [Options] Audio; 1038:67a6/6788/1d48).
    bool audioEnabled() const { return audio_; }
    void setAudioEnabled(bool on) {
        audio_ = on;
        if (audioHandler_) audioHandler_(on);
    }
    void setAudioHandler(std::function<void(bool)> h) { audioHandler_ = std::move(h); }
    GameObject* spawn(int32_t moduleId, const Vec3& pos, bool facing);
    void remove(GameObject* obj);
    void sendToApp(int32_t message) {
        if (appMessage_) appMessage_(message);
    }
    void send(int32_t target, int32_t message);  // deliver a message (type-0 event)
    const adm::Database& db() const { return *db_; }
    const GameConfig& config() const { return config_; }
    const Cheats& cheats() const { return cheats_; }
    // Where the pest is saved; empty = %APPDATA%\\CyberPest. Headless runs use their own
    // folder so they never touch a player's pest.
    const std::wstring& saveDirectory() const { return saveDirectory_; }
    void setSaveDirectory(std::wstring dir) { saveDirectory_ = std::move(dir); }

    // Window activation (OnActivate 1000:2192 → 1000:4abc). Idle time counts only while the
    // game window is inactive or minimised; while it is active the player counts as present
    // (1038:6112). Coming back from another application sends -120 to the room background
    // -70 (which turns the light back on).
    void setActive(bool active, bool fromOtherApp, long now);
    long idleSince(long now) const { return active_ ? now : inactiveSince_; }  // 1038:6112
    void resetIdle(long now) { inactiveSince_ = now; }                         // 1038:1da0
    void setCheats(const Cheats& c) { cheats_ = c; }
    uint32_t now() const { return now_; }
    // Render-only smoothing for the experimental 60 FPS mode: draw objects between the frame
    // boundaries the logic already passed (see src/smooth.h). Pure presentation.
    void setSmoothAnimation(bool on) { smoothAnimation_ = on; }
    bool smoothAnimation() const { return smoothAnimation_; }
    // Experimental animation generation: opt-in objects are drawn with generated in-between
    // pictures between the poses the logic shows (render-only; see src/morph.h).
    void setMorphAnimation(bool on) { morphAnimation_ = on; }
    bool morphAnimation() const { return morphAnimation_; }
    GameObject* focus() const { return focus_; }
    void takeFocus(GameObject* obj) { focus_ = obj; }
    void focusText(GameObject* obj) { textFocus_ = obj; }
    // The focused text field shows a blinking caret (Windows' default blink, 530 ms).
    bool caretVisible(const GameObject* obj) const { return textFocus_ == obj && now_ / 530 % 2 == 0; }
    void clearTextFocus(GameObject* obj) { if (textFocus_ == obj) textFocus_ = nullptr; }
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
    Cheats cheats_;
    std::wstring saveDirectory_;
    bool active_ = true;       // theApp+0x1d6
    long inactiveSince_ = 0;   // theApp+0x1d8
    std::vector<std::unique_ptr<GameObject>> objects_;  // front (highest z) first
    std::vector<std::unique_ptr<GameObject>> removed_;
    std::unordered_map<uint32_t, std::unique_ptr<Flc>> images_;
    std::unordered_map<uint32_t, std::unique_ptr<Sound>> sounds_;
    SoundPlayer playSound_;
    AppMessage appMessage_;
    std::function<void(int32_t)> attention_;
    std::function<void(bool)> audioHandler_;
    bool audio_ = true;
    GameObject* focus_ = nullptr;
    GameObject* textFocus_ = nullptr;
    int32_t cursor_ = 0;
    Point2 mouse_{-1, -1};
    bool buttonDown_ = false;
    uint32_t now_ = 0;
    bool smoothAnimation_ = false;
    bool morphAnimation_ = false;
    // Render caches for the generated in-betweens (presentation only, hence mutable).
    mutable FrameMorpher morpher_;
    mutable std::vector<uint8_t> morphScratch_;
};
