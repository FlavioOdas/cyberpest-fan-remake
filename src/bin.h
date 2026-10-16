#pragma once

#include <vector>

#include "object.h"

class Bin final : public GameObject {
public:
    using GameObject::GameObject;

    bool create(const Vec3& pos, bool facing) override;
    void onStateEnd() override;
    void onEvent(GameObject* from, int32_t value, int type) override;

private:
    std::vector<GameObject*> captured_;
};
