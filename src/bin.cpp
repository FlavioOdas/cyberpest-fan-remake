#include "bin.h"

#include "world.h"

bool Bin::create(const Vec3& pos, bool facing) {  // 1008:7514
    return GameObject::create(pos, facing);
}

void Bin::onEvent(GameObject*, int32_t value, int type) {  // 1008:755c
    if (type != 3 || value == 0) return;
    bool caught = false;
    for (GameObject* object : world_.objectSnapshot()) {
        if (object == this) continue;
        const HitArea* area = nullptr;
        const Vec3& position = object->position();
        if (!hitTest({position.x, position.y}, &area) || area->id != -158 ||
            !object->held() || !object->property(-280)) continue;
        captured_.push_back(object);
        object->setProperty(-180, 0);
        object->setProperty(-182, 0);
        object->setProperty(-1, 1);
        Vec3 inside = object->position();
        inside.z = pos_.z - 1;
        object->setPosition(inside);
        caught = true;
    }
    if (caught) enterState(-160);
}

void Bin::onStateEnd() {  // 1008:773a
    if (state() != -160) return;
    for (GameObject* object : captured_) object->sendMessage(-1, this);
    captured_.clear();
    enterState(-161);
}
