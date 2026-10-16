// Punishments and their selector: the punishment display (-92), 16-ton weight (-531), bomb
// (-534), cannonball (-550), cannon (-172) and flea powder (-591). docs/re/classes.md has the
// evidence; each method names the original function.
#pragma once

#include <cstdint>
#include <vector>

#include "object.h"

// Selector screen above the TV (class -92): cycles punishment pictures and spawns the chosen
// punishment at the pointer, already held.
class PunishmentDisplay final : public GameObject {
public:
    using GameObject::GameObject;
    bool create(const Vec3& pos, bool facing) override;           // 1008:eaac
    void onEvent(GameObject* from, int32_t value, int type) override;  // 1008:ebce

private:
    std::vector<int32_t> choices_;  // +0xc2/+0xc6 (child module -529)
    int index_ = 0;                 // +0xc8
};

// 16-ton weight (class -531): squashes the pest when it lands on it.
class Weight final : public GameObject {
public:
    using GameObject::GameObject;
    bool create(const Vec3& pos, bool facing) override;           // 1010:0000
    void onFrame() override;                                      // 1010:0144
    void onEvent(GameObject* from, int32_t value, int type) override;  // 1010:0080

private:
    bool aboveHead() const;     // 1010:05d4
    bool wasAbove_ = false;     // +0xc2
    bool squashing_ = false;    // +0xc4
    int restHeight_ = 0;        // +0xc6 (list -546)
};

// Bomb (class -534): blows the pest up when its explosion covers the pest. The cannon fires by
// spawning one at the pointer and detonating it at once (-446).
class Bomb final : public GameObject {
public:
    using GameObject::GameObject;
    void onEvent(GameObject* from, int32_t value, int type) override;  // 1010:07ea
};

// Cannonball (class -550): flies in and hits the pest when it passes it.
class Cannonball final : public GameObject {
public:
    using GameObject::GameObject;
    bool create(const Vec3& pos, bool facing) override;           // 1010:0b8e
    void onFrame() override;                                      // 1010:0d10

private:
    bool leftOfPest_ = false;  // +0xc2
    int depthTolerance_ = 0;   // +0xc4 (list -551)
    Vec3 hitOffset_;           // +0xc6 (list -552)
};

// Cannon (class -172): follows the pointer and fires at it.
class Cannon final : public GameObject {
public:
    using GameObject::GameObject;
    void onEvent(GameObject* from, int32_t value, int type) override;  // 1010:6986
};

// Flea powder (class -591): sprinkles while held above the pest.
class Powder final : public GameObject {
public:
    using GameObject::GameObject;
    void onFrame() override;  // 1010:3c0a

private:
    bool abovePest() const;  // 1010:3d0a
};
