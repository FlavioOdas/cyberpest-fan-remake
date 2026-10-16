#include "panels.h"

#include "pest.h"
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

bool PhoneBubble::create(const Vec3& pos, bool facing) {  // 1010:4062
    if (!GameObject::create(pos, facing)) return false;
    message_.configure(*module_, -218, -54, -55, -56);
    message_.multiline = true;
    return true;
}

void PhoneBubble::onStateStart() {  // 1010:45f8
    if (state() == -4) {
        message_.shown = false;
    } else if (state() >= -210 && state() <= -206) {
        if (auto* pest = dynamic_cast<Pest*>(world_.find(-105))) message_.value = pest->callerText(state());
    }
}

void PhoneBubble::onLastFrame(Sprite&) {  // 1010:4488
    if (state() == -108) message_.shown = true;
}

void PhoneBubble::drawOverlay(Surface& surface, const Palette& palette) const {
    message_.draw(surface, palette, position());
}

bool Phone::create(const Vec3& pos, bool facing) {  // 1010:3efa
    return GameObject::create(pos, facing);
}

void Phone::onEvent(GameObject* from, int32_t value, int type) {  // 1010:3f42
    if (type == 3 && state() == -108 && (world_.focus() == this || value == 0)) {
        cueSend(-99, value >= -210 && value <= -206 ? value : -205);
    }
    GameObject::onEvent(from, value, type);
}
