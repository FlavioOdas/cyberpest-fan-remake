#pragma once

#include "object.h"
#include "text.h"

class MegaphonePanel final : public GameObject {
public:
    using GameObject::GameObject;

    bool create(const Vec3& pos, bool facing) override;
    void onStateStart() override;
    void onLastFrame(Sprite& sprite) override;
    void onEvent(GameObject* from, int32_t value, int type) override;
    void onTextChar(wchar_t character) override;
    void drawOverlay(Surface& surface, const Palette& palette) const override;
    std::string text() const override { return words_.value; }

private:
    TextField words_;
};
