#pragma once

#include "object.h"
#include "text.h"

class Certificate final : public GameObject {
public:
    using GameObject::GameObject;

    bool create(const Vec3& pos, bool facing) override;
    void onStateStart() override;
    void onLastFrame(Sprite& sprite) override;
    void onEvent(GameObject* from, int32_t value, int type) override;
    void onTextChar(wchar_t character) override;
    void drawOverlay(Surface& surface, const Palette& palette) const override;

private:
    TextField nameField_;
    TextField dateField_;
    TextField timeField_;
};
