#include "certificate.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <windows.h>

#include "pest.h"
#include "world.h"

namespace {

// 1000:4b9e: the user's [intl] sShortDate expanded as the original does: d/dd day, m/mm
// month, yy/yyyy year (any case), everything else copied. Windows still answers
// GetProfileString for [intl] from the regional settings.
std::string shortDate(const std::tm& t) {
    char pattern[20] = "";
    if (!GetProfileStringA("intl", "sShortDate", "", pattern, sizeof pattern)) return {};
    std::string out;
    char part[16];
    for (const char* p = pattern; *p;) {
        const int c = std::tolower(uint8_t(p[0])), next = std::tolower(uint8_t(p[1]));
        if (c == 'd' || c == 'm') {
            const int value = c == 'd' ? t.tm_mday : t.tm_mon + 1;
            std::snprintf(part, sizeof part, next == c ? "%.2d" : "%d", value);
            p += next == c ? 2 : 1;
        } else if (c == 'y' && next == 'y') {
            const bool four = _strnicmp(p, "yyyy", 4) == 0;
            std::snprintf(part, sizeof part, four ? "%.4d" : "%.2d", four ? t.tm_year + 1900 : t.tm_year % 100);
            p += four ? 4 : 2;
        } else {
            part[0] = *p++, part[1] = 0;
        }
        out += part;
    }
    return out;
}

// 1000:4ed6: hour (24-hour, no padding), [intl] sTime separator, two-digit minutes.
std::string shortTime(const std::tm& t) {
    char separator[20] = "";
    if (!GetProfileStringA("intl", "sTime", "", separator, sizeof separator)) return {};
    char out[48];
    std::snprintf(out, sizeof out, "%d%s%.2d", t.tm_hour, separator, t.tm_min);
    return out;
}

}  // namespace

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
    dateField_.autoFit = timeField_.autoFit = true;  // [inf] modern 4-digit-year dates are wider than Win3.x's
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
            dateField_.value = shortDate(*local);
            timeField_.value = shortTime(*local);
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
    nameField_.draw(surface, palette, position(), world_.caretVisible(this));
    dateField_.draw(surface, palette, position());
    timeField_.draw(surface, palette, position());
}
