#include "panels.h"

#include "world.h"

bool MegaphonePanel::create(const Vec3& pos, bool facing) {  // 1010:0f6e
    if (!GameObject::create(pos, facing)) return false;
    words_.configure(*module_, -51, -192, -193, -194);
    return true;
}

void MegaphonePanel::onStateStart() {  // 1010:18de
    if (state() == -4) {
        words_.shown = false;
        world_.clearTextFocus(this);
    }
}

void MegaphonePanel::onLastFrame(Sprite&) {  // 1010:17f4
    if (state() == -108) {
        words_.shown = true;
        world_.focusText(this);
    }
}

void MegaphonePanel::onEvent(GameObject* from, int32_t value, int type) {  // 1010:188c
    if (type == 3 && value != 0) world_.focusText(this);
    GameObject::onEvent(from, value, type);
}

void MegaphonePanel::onTextChar(wchar_t character) {
    if (character == L'\r') sendMessage(-203, this);
    else words_.type(character);
}

void MegaphonePanel::drawOverlay(Surface& surface, const Palette& palette) const {
    words_.draw(surface, palette, position());
}
