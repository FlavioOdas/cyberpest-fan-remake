// CyberPest remake. Loads the user's original PEST.ADM and runs the room defined in it.
// Options:
//   --adm <path>        PEST.ADM to load (default: next to the exe, then the current folder)
//   --snapshot <bmp>    render a frame to an 8-bit BMP and exit (no window, no sound)
//   --time <ms>         with --snapshot: simulated time before the frame is taken
//   --click <x,y,ms>    with --snapshot: simulated left click (640x480 coordinates); repeatable
//   --drag <x1,y1,x2,y2,ms>  with --snapshot: hold and move over 400 ms; repeatable
//   --type <text,ms>   with --snapshot: type into the active text field; repeatable
//   --dump              with --snapshot: print every object's state to stdout
//   --popup <reason>    with --snapshot: draw the attention pop-up (e.g. -601) instead (24-bit BMP)
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "adm.h"
#include "attention.h"
#include "audio.h"
#include "flc.h"
#include "surface.h"
#include "symbols.h"
#include "world.h"

namespace {

constexpr int kWidth = 640, kHeight = 480;
constexpr int32_t kAppClose = -156, kAppMinimize = -118;
constexpr int32_t kCursorTable = -58, kDefaultCursor = -59;

struct Cursor {
    const Flc* flc = nullptr;
    int hotX = 0, hotY = 0;
};

struct App {
    adm::Database db;
    Palette palette{};
    Surface screen{kWidth, kHeight};
    World world;
    Mixer mixer;
    std::unordered_map<int32_t, Cursor> cursors;
    std::vector<std::unique_ptr<Flc>> cursorImages;
    std::wstring error;
    HWND hwnd = nullptr;
} app;

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::wstring exeDirectory() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path, n);
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
}

// PEST.INI next to the exe (the original reads it from the current directory, 1000:28ce).
std::wstring iniPath() { return exeDirectory() + L"PEST.INI"; }

int iniInt(const wchar_t* section, const wchar_t* key, int fallback) {  // signed, unlike GetPrivateProfileInt
    wchar_t text[32] = L"";
    GetPrivateProfileStringW(section, key, L"", text, 32, iniPath().c_str());
    return *text ? _wtoi(text) : fallback;
}

void readIniSettings() {  // App_ReadIniSettings 1000:28ce
    app.world.setAudioEnabled(iniInt(L"Options", L"Audio", 1) != 0);
    Cheats c;
    c.neuroticism = iniInt(L"Cheats", L"Neuroticism", -1);
    c.extroversion = iniInt(L"Cheats", L"Extroversion", -1);
    c.goodStomach = iniInt(L"Cheats", L"GoodStomach", 50);
    c.junkStomach = iniInt(L"Cheats", L"JunkStomach", 0);
    c.bowels = iniInt(L"Cheats", L"Bowels", 0);
    c.mentalState = iniInt(L"Cheats", L"MentalState", 0);
    c.gotFleas = iniInt(L"Cheats", L"GotFleas", 0);
    c.gotRabies = iniInt(L"Cheats", L"GotRabies", 0);
    c.onHeat = iniInt(L"Cheats", L"OnHeat", 0);
    app.world.setCheats(c);
}

// [Files] ADB, as written by the original installer, when that file exists.
std::wstring iniAdbPath() {
    wchar_t path[MAX_PATH] = L"";
    GetPrivateProfileStringW(L"Files", L"ADB", L"", path, MAX_PATH, iniPath().c_str());
    if (!*path || GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return {};
    return path;
}

void loadCursors() {
    const adm::Node* table = app.db.findModule(kCursorTable);
    if (!table) return;
    for (const adm::Node& c : table->children) {
        const adm::FileRef* file = c.tag == adm::Tag::List ? adm::fileValue(c) : nullptr;
        if (!file) continue;
        auto flc = std::make_unique<Flc>();
        if (!flc->open(app.db.bytes(*file)) || !flc->decodeAll() || flc->frames().empty()) continue;
        const std::vector<int32_t> hot = adm::intValues(c);
        app.cursors[c.id] = {flc.get(), hot.size() > 0 ? hot[0] : 0, hot.size() > 1 ? hot[1] : 0};
        app.cursorImages.push_back(std::move(flc));
    }
}

bool loadGame(const std::wstring& admPath) {
    std::string err;
    if (!app.db.load(admPath, &err)) {
        app.error = L"Can't load CyberPest data (" + admPath + L"): " + widen(err) +
                    L"\n\nPass the path to your original PEST.ADM with --adm.";
        return false;
    }
    const adm::FileRef* pal = app.db.findFile("PALETTE.BMP");
    if (!pal || !readBmpPalette(app.db.bytes(*pal), &app.palette)) {
        app.error = L"PALETTE.BMP missing from PEST.ADM.";
        return false;
    }
    loadCursors();
    app.world.setSoundPlayer([](const Sound* s) { app.mixer.play(0, s); });
    app.world.setAudioHandler([](bool on) { app.mixer.setMuted(!on); });  // audio button -94
    readIniSettings();
    app.world.setAttentionHandler([](int32_t reason) { attention::request(app.hwnd, app.db, reason); });
    app.world.setAppMessageHandler([](int32_t message) {  // 1000:4768
        if (!app.hwnd) return;
        if (message == kAppClose) PostMessageW(app.hwnd, WM_CLOSE, 0, 0);
        else if (message == kAppMinimize) ShowWindow(app.hwnd, SW_MINIMIZE);
    });
    if (!app.world.load(app.db, sym::RoomScreen, &err)) {
        app.error = L"Can't build the room: " + widen(err);
        return false;
    }
    return true;
}

void compose(DWORD now) {
    app.screen.fill(0);
    app.world.draw(app.screen, app.palette);
    const Point2 mouse = app.world.mouse();
    if (mouse.x < 0) return;
    auto it = app.cursors.find(app.world.cursor());
    if (it == app.cursors.end()) it = app.cursors.find(kDefaultCursor);
    if (it == app.cursors.end()) return;
    const Cursor& c = it->second;
    const int frame = int(now / DWORD(std::max(c.flc->frameDelayMs(), 20)) % DWORD(c.flc->frameCount()));
    app.screen.blit(c.flc->frames()[frame].data(), c.flc->width(), c.flc->height(), mouse.x - c.hotX,
                    mouse.y - c.hotY);
}

// Where the 640x480 picture goes in the client area: 4:3, centred, letterboxed with black
// bars; a whole-number scale (evenly sized pixels) when that still nearly fills the window.
RECT gameRect(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    double scale = std::min(double(rc.right) / kWidth, double(rc.bottom) / kHeight);
    if (scale >= 1 && std::floor(scale) / scale >= 0.85) scale = std::floor(scale);
    const int w = int(kWidth * scale), h = int(kHeight * scale);
    const int left = (rc.right - w) / 2, top = (rc.bottom - h) / 2;
    return {left, top, left + w, top + h};
}

void present(HWND hwnd, HDC dc) {
    struct {
        BITMAPINFOHEADER header;
        RGBQUAD colors[256];
    } bmi = {};
    bmi.header.biSize = sizeof bmi.header;
    bmi.header.biWidth = kWidth;
    bmi.header.biHeight = -kHeight;  // top-down
    bmi.header.biPlanes = 1;
    bmi.header.biBitCount = 8;
    bmi.header.biClrUsed = 256;
    for (int i = 0; i < 256; ++i) bmi.colors[i] = {app.palette[i].b, app.palette[i].g, app.palette[i].r, 0};
    const RECT to = gameRect(hwnd);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));  // letterbox bars
    const RECT bars[] = {{0, 0, rc.right, to.top}, {0, to.bottom, rc.right, rc.bottom},
                         {0, to.top, to.left, to.bottom}, {to.right, to.top, rc.right, to.bottom}};
    for (const RECT& bar : bars) {
        if (bar.right > bar.left && bar.bottom > bar.top) FillRect(dc, &bar, black);
    }
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, to.left, to.top, to.right - to.left, to.bottom - to.top, 0, 0, kWidth, kHeight,
                  app.screen.pixels(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS, SRCCOPY);
}

POINT toGame(HWND hwnd, LPARAM lp) {
    const RECT to = gameRect(hwnd);
    const int w = std::max<int>(1, to.right - to.left), h = std::max<int>(1, to.bottom - to.top);
    const int x = short(LOWORD(lp)) - to.left, y = short(HIWORD(lp)) - to.top;
    // The bars count as the nearest edge of the picture.
    return {std::clamp(x * kWidth / w, 0, kWidth - 1), std::clamp(y * kHeight / h, 0, kHeight - 1)};
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        present(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(nullptr);  // the game draws its own animated cursors
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE: {
        const POINT p = toGame(hwnd, lp);
        app.world.mouseMove(p.x, p.y);
        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (GetCapture() != hwnd) app.world.mouseLeave();
        return 0;
    case WM_LBUTTONDOWN: {
        const POINT p = toGame(hwnd, lp);
        SetCapture(hwnd);
        app.world.buttonDown(p.x, p.y);
        return 0;
    }
    case WM_LBUTTONUP: {
        const POINT p = toGame(hwnd, lp);
        ReleaseCapture();
        app.world.buttonUp(p.x, p.y);
        return 0;
    }
    case WM_CHAR:
        app.world.inputChar(wchar_t(wp));
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED: {  // moved to a monitor with another scale: take the suggested size
        const RECT* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        RECT min = {0, 0, kWidth / 2, kHeight / 2};
        AdjustWindowRectExForDpi(&min, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(hwnd) ? GetDpiForWindow(hwnd) : 96);
        auto* info = reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMinTrackSize = {min.right - min.left, min.bottom - min.top};
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int runWindow(HINSTANCE instance, int show) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CyberPest";
    RegisterClassW(&wc);

    // Largest whole-number scale of 640x480 that fits 90% of the work area, or the display's
    // own scale (e.g. 150%) when that is larger and fits.
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const double room = std::min((work.right - work.left) * 0.9 / kWidth, (work.bottom - work.top) * 0.9 / kHeight);
    const UINT dpi = GetDpiForSystem();
    double scale = std::max(1.0, std::floor(room));
    if (dpi / 96.0 > scale && dpi / 96.0 <= room) scale = dpi / 96.0;
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rc = {0, 0, LONG(kWidth * scale), LONG(kHeight * scale)};
    AdjustWindowRectExForDpi(&rc, style, FALSE, 0, dpi);
    HWND hwnd = app.hwnd = CreateWindowW(L"CyberPest", L"CyberPest", style, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                              rc.bottom - rc.top, nullptr, nullptr, instance, nullptr);
    ShowWindow(hwnd, show);  // honours a shortcut's Run: Minimised/Maximised
    app.mixer.start();

    MSG msg;
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                app.mixer.stop();
                return int(msg.wParam);
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        const DWORD now = GetTickCount();
        app.world.tick(now);
        compose(now);
        HDC dc = GetDC(hwnd);
        present(hwnd, dc);
        ReleaseDC(hwnd, dc);
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
}

struct Click {
    int x, y;
    DWORD at;
};

struct Drag {
    int fromX, fromY, toX, toY;
    DWORD at;
    bool started = false;
    bool finished = false;
};

struct TypedText {
    std::wstring value;
    DWORD at;
};

int runSnapshot(const std::wstring& path, DWORD duration, std::vector<Click> clicks,
                std::vector<Drag> drags, std::vector<TypedText> typed, bool dump) {
    std::sort(clicks.begin(), clicks.end(), [](const Click& a, const Click& b) { return a.at < b.at; });
    std::sort(drags.begin(), drags.end(), [](const Drag& a, const Drag& b) { return a.at < b.at; });
    std::sort(typed.begin(), typed.end(), [](const TypedText& a, const TypedText& b) { return a.at < b.at; });
    size_t next = 0;
    size_t nextText = 0;
    for (DWORD t = 0; t <= duration; t += 10) {
        app.world.tick(t);
        for (; next < clicks.size() && clicks[next].at <= t; ++next) {
            app.world.mouseMove(clicks[next].x, clicks[next].y);
            app.world.buttonDown(clicks[next].x, clicks[next].y);
            app.world.buttonUp(clicks[next].x, clicks[next].y);
        }
        for (Drag& drag : drags) {
            if (!drag.started && drag.at <= t) {
                app.world.mouseMove(drag.fromX, drag.fromY);
                app.world.buttonDown(drag.fromX, drag.fromY);
                drag.started = true;
            }
            if (drag.started && !drag.finished && drag.at + 400 <= t) {
                app.world.mouseMove(drag.toX, drag.toY);
                app.world.buttonUp(drag.toX, drag.toY);
                drag.finished = true;
            }
        }
        for (; nextText < typed.size() && typed[nextText].at <= t; ++nextText) {
            for (wchar_t character : typed[nextText].value) app.world.inputChar(character);
        }
    }
    compose(duration);
    if (dump) std::fputs(app.world.describe().c_str(), stdout);
    return writeBmp(path.c_str(), app.screen, app.palette) ? 0 : 2;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // Real pixels on scaled displays (otherwise Windows blurs the whole window).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetProcessDPIAware();
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring admPath, snapshot;
    DWORD duration = 0;
    std::vector<Click> clicks;
    std::vector<Drag> drags;
    std::vector<TypedText> typed;
    bool dump = false;
    int popupReason = 0;
    for (int i = 1; i < argc; ++i) {
        const std::wstring key = argv[i];
        if (key == L"--dump") {
            dump = true;
            continue;
        }
        if (i + 1 >= argc) break;
        const std::wstring value = argv[++i];
        if (key == L"--adm") admPath = value;
        else if (key == L"--snapshot") snapshot = value;
        else if (key == L"--time") duration = DWORD(_wtoi(value.c_str()));
        else if (key == L"--popup") popupReason = _wtoi(value.c_str());
        else if (key == L"--click") {
            Click c{};
            if (swscanf(value.c_str(), L"%d,%d,%lu", &c.x, &c.y, &c.at) == 3) clicks.push_back(c);
        } else if (key == L"--drag") {
            Drag drag{};
            if (swscanf(value.c_str(), L"%d,%d,%d,%d,%lu", &drag.fromX, &drag.fromY,
                        &drag.toX, &drag.toY, &drag.at) == 5) drags.push_back(drag);
        } else if (key == L"--type") {
            const size_t comma = value.rfind(L',');
            if (comma != std::wstring::npos) {
                const DWORD at = DWORD(_wtol(value.c_str() + comma + 1));
                typed.push_back({value.substr(0, comma), at});
            }
        }
    }
    LocalFree(argv);
    if (admPath.empty()) admPath = iniAdbPath();
    if (admPath.empty()) {
        admPath = exeDirectory() + L"PEST.ADM";
        if (GetFileAttributesW(admPath.c_str()) == INVALID_FILE_ATTRIBUTES) admPath = L"PEST.ADM";
    }
    if (!loadGame(admPath)) {
        if (snapshot.empty()) MessageBoxW(nullptr, app.error.c_str(), L"CyberPest", MB_ICONERROR);
        return 1;
    }
    if (!snapshot.empty() && popupReason)
        return attention::snapshot(app.db, popupReason, snapshot.c_str()) ? 0 : 1;
    if (!snapshot.empty())
        return runSnapshot(snapshot, duration, std::move(clicks), std::move(drags), std::move(typed), dump);
    return runWindow(instance, show);
}
