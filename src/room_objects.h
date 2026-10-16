// Room objects with their own code: the mouse (-91) and the audio button (-94).
#pragma once

#include "items.h"

// The mouse (class -91): a shelf item that runs between its hole and the room edges.
class Mouse final : public ShelfItem {
public:
    using ShelfItem::ShelfItem;
    bool create(const Vec3& pos, bool facing) override;           // 1008:77da
    void onStateStart() override;                                 // 1008:7c2e
    void onStateEnd() override;                                   // 1008:7c9e
    void onSpriteStart(Sprite& s) override;                       // 1008:7d54
    void onLastFrame(Sprite& s) override;                         // 1008:7db4
    void onEvent(GameObject* from, int32_t value, int type) override;  // 1008:7eae

private:
    Vec3 nextTarget() const;           // 1008:7992
    void runTo(const Vec3& target);    // 1008:7b9c
    bool first_ = true;                // +0x102
    int leftX_ = -100, rightX_ = 740;  // +0xe0, +0xe2 (list -318)
    Vec3 start_;                       // +0xee (-319)
    Vec3 hole_;                        // +0xf8 (-320)
};

// Audio on/off button (class -94).
class AudioButton final : public GameObject {
public:
    using GameObject::GameObject;
    void onStateStart() override;  // 1010:06f8
};
