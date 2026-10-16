// CyberPest remake. Loads the user's original PEST.ADM and runs the room defined in it.
// Options:
//   --adm <path>        PEST.ADM to load (default: next to the exe, then the current folder)
//   --snapshot <bmp>    render a frame to an 8-bit BMP and exit (no window, no sound)
//   --time <ms>         with --snapshot: simulated time before the frame is taken
//   --click <x,y,ms>    with --snapshot: simulated left click (640x480 coordinates); repeatable
//   --dump              with --snapshot: print every object's state to stdout
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "adm.h"
#include "audio.h"
#include "flc.h"
#include "surface.h"
#include "symbols.h"
#include "world.h"

namespace {

constexpr int kWidth = 640, kHeight = 480;
constexpr int32_t kCursorTable = -58, kDefaultCursor = -59, kFrameScaleConfig = -49, kConfig = -42;

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
    POINT mouse = {-1, -1};  // in 640x480 space; -1 = outside the window
    std::wstring error;
} app;

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::wstring exeDirectory() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path, n);
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
}

int configInt(int32_t listId, int fallback) {
    const adm::Node* config = app.db.findModule(kConfig);
    if (!config) return fallback;
    for (const adm::Node& c : config->children) {
        if (c.tag == adm::Tag::List && c.id == listId) {
            const std::vector<int32_t> v = adm::intValues(c);
            return v.empty() ? fallback : v[0];
        }
    }
    return fallback;
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
    if (!app.world.load(app.db, sym::RoomScreen, configInt(kFrameScaleConfig, 1), &err)) {
        app.error = L"Can't build the room: " + widen(err);
        return false;
    }
    return true;
}

void compose(DWORD now) {
    app.screen.fill(0);
    app.world.draw(app.screen);
    if (app.mouse.x < 0) return;
    const int32_t id = app.world.hover(app.mouse.x, app.mouse.y);
    auto it = app.cursors.find(id ? id : kDefaultCursor);
    if (it == app.cursors.end()) it = app.cursors.find(kDefaultCursor);
    if (it == app.cursors.end()) return;
    const Cursor& c = it->second;
    const int frame = int(now / DWORD(std::max(c.flc->frameDelayMs(), 20)) % DWORD(c.flc->frameCount()));
    app.screen.blit(c.flc->frames()[frame].data(), c.flc->width(), c.flc->height(), app.mouse.x - c.hotX,
                    app.mouse.y - c.hotY);
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
    RECT rc;
    GetClientRect(hwnd, &rc);
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, 0, rc.right, rc.bottom, 0, 0, kWidth, kHeight, app.screen.pixels(),
                  reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS, SRCCOPY);
}

POINT toGame(HWND hwnd, LPARAM lp) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int x = short(LOWORD(lp)), y = short(HIWORD(lp));
    return {rc.right ? x * kWidth / rc.right : 0, rc.bottom ? y * kHeight / rc.bottom : 0};
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
        app.mouse = toGame(hwnd, lp);
        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        app.mouse = {-1, -1};
        return 0;
    case WM_LBUTTONDOWN: {
        const POINT p = toGame(hwnd, lp);
        app.world.click(p.x, p.y, GetTickCount());
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int runWindow(HINSTANCE instance) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CyberPest";
    RegisterClassW(&wc);

    // Largest integer scale of 640x480 that fits the work area.
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int scale = std::max<int>(1, std::min<int>((work.right - work.left) * 9 / 10 / kWidth,
                                                     (work.bottom - work.top) * 9 / 10 / kHeight));
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rc = {0, 0, kWidth * scale, kHeight * scale};
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowW(L"CyberPest", L"CyberPest", style, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                              rc.bottom - rc.top, nullptr, nullptr, instance, nullptr);
    ShowWindow(hwnd, SW_SHOWNORMAL);
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

int runSnapshot(const std::wstring& path, DWORD duration, std::vector<Click> clicks, bool dump) {
    std::sort(clicks.begin(), clicks.end(), [](const Click& a, const Click& b) { return a.at < b.at; });
    size_t next = 0;
    for (DWORD t = 0; t <= duration; t += 10) {
        for (; next < clicks.size() && clicks[next].at <= t; ++next) app.world.click(clicks[next].x, clicks[next].y, t);
        app.world.tick(t);
    }
    compose(duration);
    if (dump) std::fputs(app.world.describe().c_str(), stdout);
    return writeBmp(path.c_str(), app.screen, app.palette) ? 0 : 2;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring admPath, snapshot;
    DWORD duration = 0;
    std::vector<Click> clicks;
    bool dump = false;
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
        else if (key == L"--click") {
            Click c{};
            if (swscanf(value.c_str(), L"%d,%d,%lu", &c.x, &c.y, &c.at) == 3) clicks.push_back(c);
        }
    }
    LocalFree(argv);
    if (admPath.empty()) {
        admPath = exeDirectory() + L"PEST.ADM";
        if (GetFileAttributesW(admPath.c_str()) == INVALID_FILE_ATTRIBUTES) admPath = L"PEST.ADM";
    }
    if (!loadGame(admPath)) {
        if (snapshot.empty()) MessageBoxW(nullptr, app.error.c_str(), L"CyberPest", MB_ICONERROR);
        return 1;
    }
    if (!snapshot.empty()) return runSnapshot(snapshot, duration, std::move(clicks), dump);
    return runWindow(instance);
}
