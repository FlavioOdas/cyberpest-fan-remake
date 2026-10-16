#pragma once

#include "object.h"

class Clock final : public GameObject {
public:
    using GameObject::GameObject;

    bool create(const Vec3& pos, bool facing) override;
    void onLastFrame(Sprite& sprite) override;
    void onEvent(GameObject* from, int32_t value, int type) override;

private:
    void refreshTime();
    void strikeHour();
    void updateHourHand();
    void updateMinuteHand();

    int hour_ = -1;             // f_c2
    int minute_ = -1;           // f_c4
    int previousHour_ = -1;     // f_c6
    int previousMinute_ = -1;   // f_c8
    int hourFrame_ = -1;        // f_ca
    int minuteFrame_ = -1;      // f_cc
};
