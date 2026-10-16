#pragma once

#include <cstdint>
#include <vector>

#include "object.h"

class ShelfItem : public GameObject {
public:
    using GameObject::GameObject;

    bool create(const Vec3& pos, bool facing) override;
    void onLastFrame(Sprite& sprite) override;
    void onFrame() override;
    void onEvent(GameObject* from, int32_t value, int type) override;

private:
    struct ShelfRect {
        int left, top, right, bottom;
    };

    void startTimer();
    void expireTimer();

    std::vector<ShelfRect> shelves_;
    int minSeconds_ = 0;
    int maxSeconds_ = 0;
    int maxHeight_ = 0;
    bool timerRunning_ = false;
    uint32_t timerStart_ = 0;
    uint32_t timerDuration_ = 0;
};

class ShelfCarryItem final : public ShelfItem {
public:
    using ShelfItem::ShelfItem;

    bool create(const Vec3& pos, bool facing) override;
    void onMouseMove(Point2 point) override;
    void onEvent(GameObject* from, int32_t value, int type) override;

private:
    Vec3 home_;
};

class ShelfWrapperItem final : public ShelfItem {
public:
    using ShelfItem::ShelfItem;

    void onStateStart() override;
};
