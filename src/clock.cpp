#include "clock.h"

#include <ctime>

bool Clock::create(const Vec3& pos, bool facing) {  // 1008:83ca
    return GameObject::create(pos, facing);
}

void Clock::refreshTime() {  // 1008:844c
    const std::time_t now = std::time(nullptr);
    const std::tm* local = std::localtime(&now);
    if (!local) return;
    previousMinute_ = minute_;
    minute_ = local->tm_min;
    previousHour_ = hour_;
    hour_ = local->tm_hour % 12;
}

void Clock::strikeHour() {  // 1008:84ce
    if (hour_ != previousHour_ && previousHour_ >= 0 && minute_ == 0)
        cueSpawn(-521, Vec3());
}

void Clock::updateHourHand() {  // 1008:854c
    const int frame = hour_ * 2 + (minute_ < 50 ? minute_ > 10 : 2);
    if (frame != hourFrame_) {
        hourFrame_ = frame;
        enterStateAt(frame);
    }
}

void Clock::updateMinuteHand() {  // 1008:85ea
    const int frame = (minute_ / 5) * 2 + (minute_ % 5 != 0);
    if (frame != minuteFrame_) {
        minuteFrame_ = frame;
        enterStateAt(frame);
    }
}

void Clock::onLastFrame(Sprite& sprite) {  // 1008:8682
    if (id() == -521 && state() == -108) {
        refreshTime();
        setLoop(&sprite, true, hour_ == 0 ? 12 : hour_);
    }
}

void Clock::onEvent(GameObject* from, int32_t value, int type) {  // 1008:870c
    if (type == 0 && value == -519) {
        refreshTime();
        if (minute_ != previousMinute_) {
            if (id() == -90) strikeHour();
            else if (id() == -89) updateMinuteHand();
            else if (id() == -88) updateHourHand();
        }
    }
    GameObject::onEvent(from, value, type);
}
