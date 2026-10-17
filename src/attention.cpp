#include "attention.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "surface.h"
#include "upscale.h"

namespace attention {
namespace {

constexpr int32_t kModule = -596, kFrame = -57;
constexpr int32_t kPictureRect = -599, kGoButton = -598, kIgnoreButton = -597;

struct State {
    HWND popup = nullptr;    // theApp+0x1de
    HWND main = nullptr;
    HWND previous = nullptr; // theApp+0x1e2: the window that was active before
    std::span<const uint8_t> frame, picture;
    RECT pictureRect{}, goRect{}, ignoreRect{};
    HdScaler scaler;
    bool smoothGraphics = false;
    int sourceWidth = 0, sourceHeight = 0;
    int popupWidth = 0, popupHeight = 0;
} state;

RECT rectOf(const adm::Node& module, int32_t id) {
    for (const adm::Node& list : module.children) {
        if (list.tag != adm::Tag::List || list.id != id) continue;
        const std::vector<int32_t> v = adm::intValues(list);
        if (v.size() >= 4) return {LONG(v[0]), LONG(v[1]), LONG(v[2]), LONG(v[3])};
    }
    return {};
}

std::span<const uint8_t> fileOf(const adm::Database& db, const adm::Node& module, int32_t id) {
    for (const adm::Node& list : module.children) {
        if (list.tag != adm::Tag::List || list.id != id) continue;
        if (const adm::FileRef* file = adm::fileValue(list)) return db.bytes(*file);
    }
    return {};
}

// A .BMP file's header and pixels, as the original passes them to StretchDIBits (1010:318e).
// GDI decodes the BI_RLE8 data itself.
bool dibSize(std::span<const uint8_t> bmp, int* w, int* h) {
    if (bmp.size() < 54 || bmp[0] != 'B' || bmp[1] != 'M') return false;
    const auto* info = reinterpret_cast<const BITMAPINFOHEADER*>(bmp.data() + 14);
    *w = info->biWidth, *h = std::abs(info->biHeight);
    return true;
}

void drawDib(HDC dc, std::span<const uint8_t> bmp, const RECT& to) {
    int w = 0, h = 0;
    if (!dibSize(bmp, &w, &h)) return;
    const DWORD bits = *reinterpret_cast<const DWORD*>(bmp.data() + 10);
    if (bits >= bmp.size() || bits < 14 + sizeof(BITMAPINFOHEADER)) return;
    // Header and colour table copied so biSizeImage can be filled in: the files leave it 0,
    // which Win16 accepted but Win32 GDI rejects for BI_RLE8.
    std::vector<uint8_t> info(bmp.begin() + 14, bmp.begin() + bits);
    auto* header = reinterpret_cast<BITMAPINFOHEADER*>(info.data());
    if (header->biSizeImage == 0) header->biSizeImage = DWORD(bmp.size() - bits);
    StretchDIBits(dc, to.left, to.top, to.right - to.left, to.bottom - to.top, 0, 0, w, h,
                  bmp.data() + bits, reinterpret_cast<const BITMAPINFO*>(info.data()), DIB_RGB_COLORS, SRCCOPY);
}

bool renderHd() {
    BITMAPINFO bitmap = {};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = state.sourceWidth;
    bitmap.bmiHeader.biHeight = -state.sourceHeight;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    void* pixels = nullptr;
    HBITMAP surface = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!surface) return DeleteDC(dc), false;
    HGDIOBJ old = SelectObject(dc, surface);
    const RECT frame{0, 0, state.sourceWidth, state.sourceHeight};
    drawDib(dc, state.frame, frame);
    drawDib(dc, state.picture, state.pictureRect);
    GdiFlush();
    state.scaler.scaleRgb(static_cast<const uint32_t*>(pixels), state.sourceWidth, state.sourceHeight);
    SelectObject(dc, old);
    DeleteObject(surface);
    DeleteDC(dc);
    return true;
}

void close() {
    if (state.popup) DestroyWindow(state.popup);
    state.popup = nullptr;
}

void goToPest() {  // 1000:4938
    close();
    ShowWindow(state.main, SW_RESTORE);
    SetForegroundWindow(state.main);
    FlashWindow(state.main, FALSE);
}

void ignore() {  // 1000:49ec: back behind, the previous window active again
    close();
    SetWindowPos(state.main, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    if (state.previous && IsWindow(state.previous)) SetForegroundWindow(state.previous);
    FlashWindow(state.main, FALSE);
}

LRESULT CALLBACK popupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {  // 1010:3a78
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        if (state.smoothGraphics) {
            BITMAPINFO bitmap = {};
            bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmap.bmiHeader.biWidth = state.scaler.width();
            bitmap.bmiHeader.biHeight = -state.scaler.height();
            bitmap.bmiHeader.biPlanes = 1;
            bitmap.bmiHeader.biBitCount = 32;
            SetStretchBltMode(dc, HALFTONE);
            SetBrushOrgEx(dc, 0, 0, nullptr);
            StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0,
                          state.scaler.width(), state.scaler.height(), state.scaler.pixels(),
                          &bitmap, DIB_RGB_COLORS, SRCCOPY);
        } else {
            drawDib(dc, state.frame, client);
            drawDib(dc, state.picture, state.pictureRect);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONUP: {  // 1010:3b10
        const POINT p{MulDiv(short(LOWORD(lp)), state.sourceWidth, state.popupWidth),
                      MulDiv(short(HIWORD(lp)), state.sourceHeight, state.popupHeight)};
        if (PtInRect(&state.ignoreRect, p)) ignore();
        else if (PtInRect(&state.goRect, p)) goToPest();
        return 0;
    }
    case WM_CLOSE: ignore(); return 0;  // 1010:3ba0
    case WM_DESTROY:
        if (state.popup == hwnd) state.popup = nullptr;
        return 0;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

bool load(const adm::Database& db, int32_t reason) {  // 1010:357c
    const adm::Node* module = db.findModule(kModule);
    if (!module) return false;
    state.frame = fileOf(db, *module, kFrame);
    state.picture = fileOf(db, *module, reason);
    state.pictureRect = rectOf(*module, kPictureRect);
    state.goRect = rectOf(*module, kGoButton);
    state.ignoreRect = rectOf(*module, kIgnoreButton);
    int w = 0, h = 0;
    return dibSize(state.frame, &w, &h) && !state.picture.empty();
}

}  // namespace

bool request(HWND mainWindow, const adm::Database& db, int32_t reason, bool smoothGraphics) {  // 1000:47f6
    if (!mainWindow || state.popup || GetForegroundWindow() == mainWindow || !load(db, reason)) return false;
    state.main = mainWindow;
    state.previous = GetForegroundWindow();
    int w = 0, h = 0;
    dibSize(state.frame, &w, &h);
    state.sourceWidth = w;
    state.sourceHeight = h;
    state.smoothGraphics = smoothGraphics && renderHd();
    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const double scale = state.smoothGraphics
        ? std::min({2.0, double(workArea.right - workArea.left) * 0.9 / w,
                    double(workArea.bottom - workArea.top) * 0.9 / h}) : 1.0;
    state.popupWidth = int(w * scale);
    state.popupHeight = int(h * scale);

    static bool registered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = popupProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CyberPestAttention";
        registered = RegisterClassW(&wc) != 0;
    }
    // A WS_POPUP the size of the frame bitmap, centred on the screen (1010:2e6e). Topmost and
    // unowned so that it stays visible while the game window is minimised.
    const int x = state.smoothGraphics
        ? workArea.left + (workArea.right - workArea.left - state.popupWidth) / 2
        : (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    const int y = state.smoothGraphics
        ? workArea.top + (workArea.bottom - workArea.top - state.popupHeight) / 2
        : (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    state.popup = CreateWindowExW(WS_EX_TOPMOST, L"CyberPestAttention", L"CyberPest", WS_POPUP,
                                  x, y, state.popupWidth, state.popupHeight,
                                  nullptr, nullptr, instance, nullptr);
    if (!state.popup) return false;
    ShowWindow(state.popup, SW_SHOW);
    SetForegroundWindow(state.popup);
    UpdateWindow(state.popup);
    MessageBeep(0);
    FlashWindow(mainWindow, TRUE);
    return true;
}

bool snapshot(const adm::Database& db, int32_t reason, const wchar_t* path, bool smoothGraphics) {
    int w = 0, h = 0;
    if (!load(db, reason) || !dibSize(state.frame, &w, &h)) return false;
    if (smoothGraphics) {
        state.sourceWidth = w;
        state.sourceHeight = h;
        if (!renderHd()) return false;
        return writeBmp32(path, state.scaler.pixels(), state.scaler.width(), state.scaler.height());
    }
    BITMAPINFOHEADER info{sizeof(info), w, h, 1, 24, BI_RGB};
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) return DeleteDC(dc), false;
    HGDIOBJ old = SelectObject(dc, bmp);
    const RECT client{0, 0, w, h};
    drawDib(dc, state.frame, client);
    drawDib(dc, state.picture, state.pictureRect);
    GdiFlush();
    const DWORD stride = DWORD((w * 3 + 3) & ~3), size = stride * DWORD(h);
    BITMAPFILEHEADER file{0x4d42, DWORD(sizeof(file) + sizeof(info) + size), 0, 0, DWORD(sizeof(file) + sizeof(info))};
    bool ok = false;
    if (FILE* f = _wfopen(path, L"wb")) {
        ok = fwrite(&file, sizeof file, 1, f) == 1 && fwrite(&info, sizeof info, 1, f) == 1 &&
             fwrite(bits, 1, size, f) == size;
        fclose(f);
    }
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    return ok;
}

}  // namespace attention
