#include "certificate.h"

#include <ctime>
#include <windows.h>

#include "pest.h"
#include "world.h"

bool Certificate::create(const Vec3& pos, bool facing) {  // 1010:47c6
    if (!GameObject::create(pos, facing)) return false;
    Vec3 placement = pos;
    moduleInt(-594, 0, &placement.x);
    moduleInt(-594, 1, &placement.y);
    moduleInt(-594, 2, &placement.z);
    setPosition(placement);
    nameField_.configure(*module_, -51, -54, -55, -56);
    timeField_.configure(*module_, -52, -54, -55, -56);
    dateField_.configure(*module_, -53, -54, -55, -56);
    dateField_.autoFit = timeField_.autoFit = true;  // [inf] preserve all characters in narrow date/time fields
    return true;
}

void Certificate::onStateStart() {  // 1010:513a
    if (state() == -108) {
        auto* pest = dynamic_cast<Pest*>(world_.find(-105));
        if (!pest) return;
        if (!pest->name().empty()) {
            cueSend(-204, -4);
            return;
        }
        const std::time_t born = std::time_t(pest->birthTime());
        if (const std::tm* local = std::localtime(&born)) {
            SYSTEMTIME time = {};
            time.wYear = WORD(local->tm_year + 1900);
            time.wMonth = WORD(local->tm_mon + 1);
            time.wDay = WORD(local->tm_mday);
            time.wHour = WORD(local->tm_hour);
            time.wMinute = WORD(local->tm_min);
            time.wSecond = WORD(local->tm_sec);
            char dateBuffer[64] = {}, timeBuffer[64] = {};
            GetDateFormatA(LOCALE_USER_DEFAULT, 0, &time, "dd/MM/yy", dateBuffer, sizeof dateBuffer);  // [inf] compact Win3.x-style date
            GetTimeFormatA(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &time, nullptr, timeBuffer, sizeof timeBuffer);
            dateField_.value = dateBuffer;
            timeField_.value = timeBuffer;
        }
        world_.focusText(this);
    } else if (state() == -595) {
        if (auto* pest = dynamic_cast<Pest*>(world_.find(-105))) pest->setName(nameField_.value);
    } else if (state() == -4) {
        nameField_.shown = dateField_.shown = timeField_.shown = false;
        world_.clearTextFocus(this);
    }
}

void Certificate::onLastFrame(Sprite&) {  // 1010:502a
    if (state() == -108) {
        nameField_.shown = dateField_.shown = timeField_.shown = true;
        world_.focusText(this);
    }
}

void Certificate::onEvent(GameObject* from, int32_t value, int type) {  // 1010:50e8
    if (type == 3 && value != 0) world_.focusText(this);
    GameObject::onEvent(from, value, type);
}

void Certificate::onTextChar(wchar_t character) {
    if (character == L'\r') sendMessage(-203, this);
    else nameField_.type(character);
}

void Certificate::drawOverlay(Surface& surface, const Palette& palette) const {
    nameField_.draw(surface, palette, position());
    dateField_.draw(surface, palette, position());
    timeField_.draw(surface, palette, position());
}
