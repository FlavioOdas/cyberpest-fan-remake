#include "mod.h"

#include <windows.h>
#include <wtypes.h>
#include <gdiplus.h>
#include <shlwapi.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "surface.h"
#include "upscale.h"

namespace {

constexpr int kMegaphoneResource = 101;
constexpr int kScale = HdScaler::kScale;

struct GdiplusSession {
    ULONG_PTR token = 0;
    GdiplusSession() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&token, &input, nullptr);
    }
    ~GdiplusSession() { if (token) Gdiplus::GdiplusShutdown(token); }
};

const std::vector<uint32_t>& megaphone() {
    static const std::vector<uint32_t> image = [] {
        GdiplusSession gdiplus;
        std::vector<uint32_t> result;
        if (!gdiplus.token) return result;
        HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(kMegaphoneResource), RT_RCDATA);
        if (!resource) return result;
        HGLOBAL loaded = LoadResource(nullptr, resource);
        if (!loaded) return result;
        const auto* bytes = static_cast<const BYTE*>(LockResource(loaded));
        IStream* stream = SHCreateMemStream(bytes, SizeofResource(nullptr, resource));
        if (!stream) return result;
        std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromStream(stream));
        if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok && bitmap->GetWidth() == 120 * kScale &&
            bitmap->GetHeight() == 120 * kScale) {
            Gdiplus::Rect bounds(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
            Gdiplus::BitmapData data = {};
            if (bitmap->LockBits(&bounds, Gdiplus::ImageLockModeRead,
                                 PixelFormat32bppARGB, &data) == Gdiplus::Ok) {
                result.resize(size_t(bounds.Width) * bounds.Height);
                for (int row = 0; row < bounds.Height; ++row)
                    std::copy_n(reinterpret_cast<const uint32_t*>(static_cast<const BYTE*>(data.Scan0) +
                                                                   row * data.Stride),
                                bounds.Width, result.data() + size_t(row) * bounds.Width);
                bitmap->UnlockBits(&data);
            }
        }
        bitmap.reset();
        stream->Release();
        return result;
    }();
    return image;
}

void blend(uint32_t* target, uint32_t source) {
    const uint32_t alpha = source >> 24;
    if (alpha == 255) {
        *target = source;
    } else if (alpha) {
        const uint32_t old = *target, inverse = 255 - alpha;
        const uint32_t red = ((source >> 16 & 255) * alpha + (old >> 16 & 255) * inverse + 127) / 255;
        const uint32_t green = ((source >> 8 & 255) * alpha + (old >> 8 & 255) * inverse + 127) / 255;
        const uint32_t blue = ((source & 255) * alpha + (old & 255) * inverse + 127) / 255;
        *target = 0xff000000u | red << 16 | green << 8 | blue;
    }
}

void replay(const Surface::BlitCommand& command, const Palette& palette, uint32_t* target,
            int roomWidth, int roomHeight, const Surface::ModRegion& region) {
    if (command.x + command.width <= region.x || command.x >= region.x + region.width ||
        command.y + command.height <= region.y || command.y >= region.y + region.height) return;
    HdScaler scaler;
    scaler.scale(command.pixels, command.width, command.height, palette);
    const int targetWidth = roomWidth * kScale, sourceWidth = command.width * kScale;
    const int left = std::max({0, command.x * kScale, region.x * kScale});
    const int top = std::max({0, command.y * kScale, region.y * kScale});
    const int right = std::min({targetWidth, (command.x + command.width) * kScale,
                                (region.x + region.width) * kScale});
    const int bottom = std::min({roomHeight * kScale, (command.y + command.height) * kScale,
                                 (region.y + region.height) * kScale});
    for (int row = top; row < bottom; ++row) {
        const int sourceRow = row - command.y * kScale;
        for (int column = left; column < right; ++column) {
            const int sourceColumn = command.mirror ? sourceWidth - 1 - (column - command.x * kScale)
                                                    : column - command.x * kScale;
            if (command.transparent >= 0 &&
                command.pixels[size_t(sourceRow / kScale) * command.width + sourceColumn / kScale] ==
                    command.transparent) continue;
            target[size_t(row) * targetWidth + column] =
                scaler.pixels()[size_t(sourceRow) * sourceWidth + sourceColumn];
        }
    }
}

} // namespace

void applyExperimentalMod(const Surface& surface, const Palette& palette, uint32_t* pixels) {
    if (!surface.hasModRegion() || megaphone().empty()) return;
    HdScaler underlay;
    underlay.scale(surface.modUnderlay().data(), surface.width(), surface.height(), palette);
    const Surface::ModRegion region = surface.modRegion();
    const int width = surface.width() * kScale, height = surface.height() * kScale;
    const int left = std::max(0, region.x * kScale), top = std::max(0, region.y * kScale);
    const int right = std::min(width, (region.x + region.width) * kScale);
    const int bottom = std::min(height, (region.y + region.height) * kScale);
    for (int row = top; row < bottom; ++row) {
        const int spriteRow = row - region.y * kScale;
        for (int column = left; column < right; ++column) {
            const int spriteColumn = column - region.x * kScale;
            uint32_t* target = pixels + size_t(row) * width + column;
            *target = underlay.pixels()[size_t(row) * width + column];
            blend(target, megaphone()[size_t(spriteRow) * region.width * kScale + spriteColumn]);
        }
    }
    for (const Surface::BlitCommand& command : surface.modLaterBlits())
        replay(command, palette, pixels, surface.width(), surface.height(), region);
}
