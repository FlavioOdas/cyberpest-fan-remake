#include "text.h"

#include <algorithm>

void TextField::configure(const adm::Node& module, int32_t rectId, int32_t fontId,
                          int32_t foregroundId, int32_t backgroundId) {
    for (const adm::Node& list : module.children) {
        if (list.tag != adm::Tag::List) continue;
        const std::vector<int32_t> values = adm::intValues(list);
        if (list.id == rectId && values.size() >= 4) {
            left_ = values[0];
            top_ = values[1];
            right_ = values[2];
            bottom_ = values[3];
        } else if (list.id == fontId) {
            for (const adm::Node& child : list.children) {
                if (child.tag == adm::Tag::String) fontFace_ = child.text;
            }
            if (!values.empty()) fontSize_ = values[0];
        } else if (list.id == foregroundId && values.size() >= 3) {
            foreground_ = {uint8_t(std::clamp(values[0], 0, 255)), uint8_t(std::clamp(values[1], 0, 255)),
                           uint8_t(std::clamp(values[2], 0, 255))};
        } else if (list.id == backgroundId && values.size() >= 3) {
            background_ = {uint8_t(std::clamp(values[0], 0, 255)), uint8_t(std::clamp(values[1], 0, 255)),
                           uint8_t(std::clamp(values[2], 0, 255))};
        }
    }
}

void TextField::draw(Surface& surface, const Palette& palette, const Vec3& origin) const {
    if (!shown) return;
    surface.drawText(value, origin.x + left_, origin.y + top_, origin.x + right_, origin.y + bottom_,
                     fontFace_, fontSize_, foreground_, background_, palette, autoFit, multiline);
}

void TextField::type(wchar_t character) {
    if (character == L'\b') {
        if (!value.empty()) value.pop_back();
    } else if (character >= 32 && character <= 255 && value.size() < maxLength) {
        value.push_back(char(character));
    }
}
