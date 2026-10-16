#pragma once

#include <cstddef>
#include <string>

#include "adm.h"
#include "surface.h"
#include "vec.h"

class TextField {
public:
    void configure(const adm::Node& module, int32_t rectId, int32_t fontId,
                   int32_t foregroundId, int32_t backgroundId);
    void draw(Surface& surface, const Palette& palette, const Vec3& origin) const;
    void type(wchar_t character);

    std::string value;
    bool shown = false;
    bool autoFit = false;
    size_t maxLength = 400;

private:
    int left_ = 0, top_ = 0, right_ = 0, bottom_ = 0;
    int fontSize_ = 16;
    std::string fontFace_ = "Courier New";
    Rgb foreground_{0, 0, 0};
    Rgb background_{255, 255, 255};
};
