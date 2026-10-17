// CyberPest remake. Loads the user's original PEST.ADM and runs the room defined in it.
// Options:
//   --adm <path>        PEST.ADM to load (default: next to the exe, then the current folder)
//   --snapshot <bmp>    render a frame to an 8-bit BMP and exit (no window, no sound)
//   --time <ms>         with --snapshot: simulated time before the frame is taken; with a live
//                       run: exit after this many milliseconds (used with --fps-log)
//   --click <x,y,ms>    with --snapshot: simulated left click (640x480 coordinates); repeatable
//   --drag <x1,y1,x2,y2,ms>  with --snapshot: hold and move over 400 ms; repeatable
//   --type <text,ms>   with --snapshot: type into the active text field; repeatable
//   --dump              with --snapshot: print every object's state to stdout
//   --popup <reason>    with --snapshot: draw the attention pop-up (e.g. -601) instead (24-bit BMP)
//   --activate <ms>     with --snapshot: the window is re-activated from another application
//   --startup on|off    add or remove the "run when Windows starts" entry, then exit
//   --import <file>     copy PEST.ADM, or extract it from the original installer's DATA.Z,
//                       CyberPest.zip or cyberpest.rar, to --out (default
//                       %APPDATA%\CyberPest\PEST.ADM), then exit
//   --download-ia       download the original game from the Internet Archive into --out
//                       (default %APPDATA%\CyberPest\PEST.ADM), then exit
//   --hd / --classic / --mod    choose smoothing, original pixels, or the experimental art mod
//   --60fps            experimental: 60 presentations per second and render-only in-between
//                      positions for moving objects (original animation timing unchanged)
//   --fps-log <path>    write one CSV line per presented frame (live run, scripted --click
//                       events replay, mixer muted) and exit after --time
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

#include "adm.h"
#include "archive.h"
#include "attention.h"
#include "gamedata.h"
#include "pest.h"
#include "audio.h"
#include "flc.h"
#include "mod.h"
#include "smooth.h"
#include "surface.h"
#include "symbols.h"
#include "upscale.h"
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
    HdScaler hdScaler;
    bool smoothGraphics = true;
    bool experimentalMod = false;
    bool highFrameRate = false;
    bool highResolutionTimer = false;
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

bool fileExists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

// First run without game data: offer the Internet Archive copy (the remake ships no game
// data) or a file the player already has.
std::wstring askForGameData() {
    for (;;) {
        const std::wstring message =
            L"CyberPest needs the original game data from 1996, which is not part of this remake.\n\n"
            L"Yes - download a preserved copy from the Internet Archive\n      " + std::wstring(archive::kItemPage) +
            L"  (about 11 MB)\n\n"
            L"No - choose CyberPest.zip, DATA.Z or PEST.ADM from your own copy\n\n"
            L"Cancel - quit";
        const int answer = MessageBoxW(nullptr, message.c_str(), L"CyberPest", MB_YESNOCANCEL | MB_ICONINFORMATION);
        if (answer == IDCANCEL || answer == 0) return {};
        const std::wstring target = gamedata::dataDirectory() + L"\\PEST.ADM";
        if (answer == IDYES) {
            std::wstring error;
            if (archive::fetchGameData(target, &error, /*showProgress=*/true)) return target;
            if (MessageBoxW(nullptr, (error + L"\n\nChoose a file from your own copy instead?").c_str(), L"CyberPest",
                            MB_YESNO | MB_ICONWARNING) != IDYES)
                return {};
        }
        for (;;) {
            wchar_t file[MAX_PATH] = L"";
            OPENFILENAMEW ofn = {sizeof ofn};
            ofn.lpstrFilter =
                L"CyberPest files (CyberPest.zip, cyberpest.rar, DATA.Z, PEST.ADM)\0*.zip;*.rar;*.Z;*.ADM\0All files\0*.*\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = L"Find your CyberPest files (CyberPest.zip, DATA.Z or PEST.ADM)";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetOpenFileNameW(&ofn)) return {};
            std::wstring error;
            if (gamedata::importFile(file, target, &error)) return target;
            if (MessageBoxW(nullptr, (error + L"\n\nTry another file?").c_str(), L"CyberPest",
                            MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY)
                return {};
        }
    }
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
    app.world.setAttentionHandler([](int32_t reason) {
        attention::request(app.hwnd, app.db, reason, app.smoothGraphics);
    });
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
    app.screen.setExperimentalMod(app.experimentalMod);
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
    if (!app.smoothGraphics && scale >= 1 && std::floor(scale) / scale >= 0.85)
        scale = std::floor(scale);
    const int w = int(kWidth * scale), h = int(kHeight * scale);
    const int left = (rc.right - w) / 2, top = (rc.bottom - h) / 2;
    return {left, top, left + w, top + h};
}

void present(HWND hwnd, HDC dc) {
    const RECT to = gameRect(hwnd);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));  // letterbox bars
    const RECT bars[] = {{0, 0, rc.right, to.top}, {0, to.bottom, rc.right, rc.bottom},
                         {0, to.top, to.left, to.bottom}, {to.right, to.top, rc.right, to.bottom}};
    for (const RECT& bar : bars) {
        if (bar.right > bar.left && bar.bottom > bar.top) FillRect(dc, &bar, black);
    }
    if (app.smoothGraphics) {
        app.hdScaler.scale(app.screen.pixels(), kWidth, kHeight, app.palette);
        if (app.experimentalMod)
            applyExperimentalMod(app.screen, app.palette, app.hdScaler.mutablePixels());
        BITMAPINFO bitmap = {};
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = app.hdScaler.width();
        bitmap.bmiHeader.biHeight = -app.hdScaler.height();
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, to.left, to.top, to.right - to.left, to.bottom - to.top,
                      0, 0, app.hdScaler.width(), app.hdScaler.height(),
                      app.hdScaler.pixels(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
    } else {
        struct {
            BITMAPINFOHEADER header;
            RGBQUAD colors[256];
        } bitmap = {};
        bitmap.header.biSize = sizeof bitmap.header;
        bitmap.header.biWidth = kWidth;
        bitmap.header.biHeight = -kHeight;
        bitmap.header.biPlanes = 1;
        bitmap.header.biBitCount = 8;
        bitmap.header.biClrUsed = 256;
        for (int index = 0; index < 256; ++index)
            bitmap.colors[index] = {app.palette[index].b, app.palette[index].g, app.palette[index].r, 0};
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, to.left, to.top, to.right - to.left, to.bottom - to.top,
                      0, 0, kWidth, kHeight, app.screen.pixels(),
                      reinterpret_cast<BITMAPINFO*>(&bitmap), DIB_RGB_COLORS, SRCCOPY);
    }
}

// Window icon drawn from the pest's standing sprite in the player's PEST.ADM (the original
// icon is an asset, so it is not shipped). Index 0 is the transparent green.
HICON makeIcon(int size) {
    const adm::FileRef* file = app.db.findFile("PSSTILL.FLC");
    Flc flc;
    if (!file || !flc.open(app.db.bytes(*file)) || !flc.decodeAll() || flc.frames().empty()) return nullptr;
    const std::vector<uint8_t>& px = flc.frames()[0];
    const int w = flc.width(), h = flc.height();
    int left = w, top = h, right = -1, bottom = -1;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (px[size_t(y * w + x)]) left = std::min(left, x), right = std::max(right, x), top = std::min(top, y),
                                       bottom = std::max(bottom, y);
    if (right < left) return nullptr;
    const int cw = right - left + 1, ch = bottom - top + 1, side = std::max(cw, ch);
    BITMAPV5HEADER info = {};
    info.bV5Size = sizeof info;
    info.bV5Width = size;
    info.bV5Height = -size;  // top-down
    info.bV5Planes = 1;
    info.bV5BitCount = 32;
    info.bV5Compression = BI_BITFIELDS;
    info.bV5RedMask = 0x00FF0000, info.bV5GreenMask = 0x0000FF00, info.bV5BlueMask = 0x000000FF;
    info.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color) return nullptr;
    auto* out = static_cast<uint32_t*>(bits);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {  // nearest neighbour, centred, aspect kept
            const int sx = left + (x * side / size) - (side - cw) / 2, sy = top + (y * side / size) - (side - ch) / 2;
            uint32_t argb = 0;
            if (sx >= left && sx <= right && sy >= top && sy <= bottom) {
                const uint8_t i = px[size_t(sy * w + sx)];
                if (i) argb = 0xFF000000u | uint32_t(app.palette[i].r) << 16 | uint32_t(app.palette[i].g) << 8 | app.palette[i].b;
            }
            out[y * size + x] = argb;
        }
    }
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO ii = {TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

// "Run CyberPest on startup?" (the original installer's StartUp group icon): a per-user
// Run entry, toggled from the window's system menu.
constexpr UINT kSysRunAtStartup = 0x0010;
constexpr UINT kSysSmoothGraphics = 0x0020;
constexpr UINT kSysExperimentalMod = 0x0030;
constexpr UINT kSysHighFrameRate = 0x0040;
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

void setHighFrameRate(bool enabled) {
    if (app.highFrameRate == enabled) return;
    if (enabled) {
        app.highResolutionTimer = timeBeginPeriod(1) == TIMERR_NOERROR;
    } else if (app.highResolutionTimer) {
        timeEndPeriod(1);
        app.highResolutionTimer = false;
    }
    app.highFrameRate = enabled;
    app.world.setSmoothAnimation(enabled);  // render-only; original animation timing is untouched
}

bool runsAtStartup() {
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, L"CyberPest", RRF_RT_REG_SZ, nullptr, nullptr, nullptr) ==
           ERROR_SUCCESS;
}

bool setRunAtStartup(bool on) {
    if (!on) {
        const LSTATUS r = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, L"CyberPest");
        return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
    }
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    const std::wstring command = L"\"" + std::wstring(path, n) + L"\"";
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, L"CyberPest", REG_SZ, command.c_str(),
                           DWORD((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

POINT toGame(HWND hwnd, LPARAM lp) {
    const RECT to = gameRect(hwnd);
    const int w = std::max<int>(1, to.right - to.left), h = std::max<int>(1, to.bottom - to.top);
    const int x = short(LOWORD(lp)) - to.left, y = short(HIWORD(lp)) - to.top;
    // The bars count as the nearest edge of the picture.
    return {std::clamp(x * kWidth / w, 0, kWidth - 1), std::clamp(y * kHeight / h, 0, kHeight - 1)};
}

// The original saves when the pest object is destroyed at exit (destructor 1038:4a16 → 1008:8f32).
void savePest() {
    if (auto* pest = dynamic_cast<Pest*>(app.world.find(-105))) pest->save();
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
    case WM_ACTIVATE: {  // OnActivate 1000:2192
        const bool active = LOWORD(wp) != WA_INACTIVE && !HIWORD(wp);
        // Win16 passed no other window when it belonged to another task.
        DWORD otherProcess = 0;
        if (lp) GetWindowThreadProcessId(reinterpret_cast<HWND>(lp), &otherProcess);
        const bool fromOtherApp = !lp || otherProcess != GetCurrentProcessId();
        app.world.setActive(active, active && fromOtherApp, long(std::time(nullptr)));
        break;
    }
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == kSysSmoothGraphics) {
            app.smoothGraphics = !app.smoothGraphics;
            app.experimentalMod = false;
            CheckMenuItem(GetSystemMenu(hwnd, FALSE), kSysSmoothGraphics,
                          MF_BYCOMMAND | (app.smoothGraphics ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(GetSystemMenu(hwnd, FALSE), kSysExperimentalMod, MF_BYCOMMAND | MF_UNCHECKED);
            return 0;
        }
        if ((wp & 0xFFF0) == kSysExperimentalMod) {
            app.experimentalMod = !app.experimentalMod;
            if (app.experimentalMod) app.smoothGraphics = true;
            CheckMenuItem(GetSystemMenu(hwnd, FALSE), kSysExperimentalMod,
                          MF_BYCOMMAND | (app.experimentalMod ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(GetSystemMenu(hwnd, FALSE), kSysSmoothGraphics,
                          MF_BYCOMMAND | (app.smoothGraphics ? MF_CHECKED : MF_UNCHECKED));
            return 0;
        }
        if ((wp & 0xFFF0) == kSysHighFrameRate) {
            setHighFrameRate(!app.highFrameRate);
            CheckMenuItem(GetSystemMenu(hwnd, FALSE), kSysHighFrameRate,
                          MF_BYCOMMAND | (app.highFrameRate ? MF_CHECKED : MF_UNCHECKED));
            return 0;
        }
        if ((wp & 0xFFF0) == kSysRunAtStartup) {
            setRunAtStartup(!runsAtStartup());
            CheckMenuItem(GetSystemMenu(hwnd, FALSE), kSysRunAtStartup,
                          MF_BYCOMMAND | (runsAtStartup() ? MF_CHECKED : MF_UNCHECKED));
            return 0;
        }
        break;
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
    case WM_ENDSESSION:  // Windows is shutting down or logging off
        if (wp) savePest();
        return 0;
    case WM_DESTROY:
        savePest();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

struct Click {
    int x, y;
    DWORD at;
};

// Millisecond clock for the presentation loop. GetTickCount only advances with the ~15.6 ms
// system tick, so a 16/17 ms presentation target can only ever fire every other tick (the
// measured result was ~36 presents/s); QueryPerformanceCounter gives the fine cadence the
// option promises. Logic keeps receiving plain milliseconds.
DWORD loopNowMs() {
    static const double ticksPerMs = [] {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        return double(frequency.QuadPart) / 1000.0;
    }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return DWORD(double(counter.QuadPart) / ticksPerMs);
}

int runWindow(HINSTANCE instance, int show, DWORD runForMs, const std::wstring& fpsLog,
              std::vector<Click> clicks) {
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
    double scale = app.smoothGraphics ? std::max(1.0, room) : std::max(1.0, std::floor(room));
    if (dpi / 96.0 > scale && dpi / 96.0 <= room) scale = dpi / 96.0;
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rc = {0, 0, LONG(kWidth * scale), LONG(kHeight * scale)};
    AdjustWindowRectExForDpi(&rc, style, FALSE, 0, dpi);
    HWND hwnd = app.hwnd = CreateWindowW(L"CyberPest", L"CyberPest", style, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                              rc.bottom - rc.top, nullptr, nullptr, instance, nullptr);
    if (HICON big = makeIcon(GetSystemMetricsForDpi(SM_CXICON, GetDpiForWindow(hwnd))))
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, LPARAM(big));
    if (HICON small = makeIcon(GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForWindow(hwnd))))
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, LPARAM(small));
    HMENU system = GetSystemMenu(hwnd, FALSE);
    AppendMenuW(system, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(system, MF_STRING | (runsAtStartup() ? MF_CHECKED : MF_UNCHECKED), kSysRunAtStartup,
                L"Run CyberPest when Windows starts");
    AppendMenuW(system, MF_STRING | (app.smoothGraphics ? MF_CHECKED : MF_UNCHECKED), kSysSmoothGraphics,
                L"Smooth graphics (HD)");
    AppendMenuW(system, MF_STRING | (app.experimentalMod ? MF_CHECKED : MF_UNCHECKED), kSysExperimentalMod,
                L"Experimental hand-fitted art mod (WIP)");
    AppendMenuW(system, MF_STRING | (app.highFrameRate ? MF_CHECKED : MF_UNCHECKED), kSysHighFrameRate,
                L"60 FPS animation (experimental, WIP)");
    ShowWindow(hwnd, show);  // honours a shortcut's Run: Minimised/Maximised
    app.mixer.start();
    if (!fpsLog.empty()) app.mixer.setMuted(true);  // a timing run stays quiet

    // --fps-log: one CSV line per presented frame, so live timing can be checked (missed
    // deadlines vs. repeated original frames) without guessing from snapshots. Scripted
    // --click events replay so the run can reach a walking pest; the mixer stays muted.
    std::FILE* log = nullptr;
    DWORD logStart = 0;
    if (!fpsLog.empty()) {
        log = _wfopen(fpsLog.c_str(), L"w");
        if (log) {
            logStart = loopNowMs();
            std::fputs("t_ms,dt_ms,sprite,frame,alpha,x,y\n", log);
        }
    }
    auto closeLog = [&] {
        if (log) {
            std::fclose(log);
            log = nullptr;
        }
    };
    size_t nextClick = 0;

    MSG msg;
    DWORD lastPresented = 0;
    int framePhase = 0;
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                closeLog();
                app.mixer.stop();
                setHighFrameRate(false);
                return int(msg.wParam);
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        const DWORD now = loopNowMs();
        for (; nextClick < clicks.size() && clicks[nextClick].at <= now - logStart; ++nextClick) {
            app.world.mouseMove(clicks[nextClick].x, clicks[nextClick].y);
            app.world.buttonDown(clicks[nextClick].x, clicks[nextClick].y);
            app.world.buttonUp(clicks[nextClick].x, clicks[nextClick].y);
        }
        app.world.tick(now);
        const DWORD frameInterval = app.highFrameRate ? (framePhase == 2 ? 16 : 17) : 33;
        if ((!app.smoothGraphics && !app.highFrameRate) || now - lastPresented >= frameInterval) {
            compose(now);
            HDC dc = GetDC(hwnd);
            present(hwnd, dc);
            ReleaseDC(hwnd, dc);
            if (log) {
                const GameObject* pest = app.world.find(-105);
                const Sprite* sprite = pest ? pest->shownSprite() : nullptr;
                const FramePose& pose = pest ? pest->framePose() : FramePose{};
                std::fprintf(log, "%u,%u,%s,%d,%.3f,%d,%d\n", unsigned(now - logStart),
                             unsigned(now - lastPresented), sprite ? sprite->name.c_str() : "-",
                             sprite ? sprite->frame : -1, pose.valid ? pose.alpha(now) : -1.0f,
                             pest ? pest->position().x : 0, pest ? pest->position().y : 0);
                std::fflush(log);
            }
            lastPresented = now;
            framePhase = (framePhase + 1) % 3;
        }
        if (runForMs && now - logStart >= runForMs) {
            closeLog();
            app.mixer.stop();
            setHighFrameRate(false);
            return 0;
        }
        const DWORD nextInterval = app.highFrameRate ? (framePhase == 2 ? 16 : 17) : 33;
        const DWORD elapsed = loopNowMs() - lastPresented;
        const DWORD waitMs = app.smoothGraphics || app.highFrameRate
                                 ? std::min<DWORD>(10, elapsed < nextInterval ? nextInterval - elapsed : 0)
                                 : 10;
        MsgWaitForMultipleObjects(0, nullptr, FALSE, waitMs, QS_ALLINPUT);
    }
}

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
                std::vector<Drag> drags, std::vector<TypedText> typed, bool dump, DWORD activateAt) {
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
        if (activateAt && t == activateAt / 10 * 10) {  // back from another application
            app.world.setActive(false, false, long(std::time(nullptr)));
            app.world.setActive(true, true, long(std::time(nullptr)));
        }
    }
    compose(duration);
    if (dump) std::fputs(app.world.describe().c_str(), stdout);
    if (app.smoothGraphics) {
        app.hdScaler.scale(app.screen.pixels(), kWidth, kHeight, app.palette);
        if (app.experimentalMod)
            applyExperimentalMod(app.screen, app.palette, app.hdScaler.mutablePixels());
        return writeBmp32(path.c_str(), app.hdScaler.pixels(), app.hdScaler.width(), app.hdScaler.height()) ? 0 : 2;
    }
    return writeBmp(path.c_str(), app.screen, app.palette) ? 0 : 2;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // Real pixels on scaled displays (otherwise Windows blurs the whole window).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring admPath, snapshot, importSource, importTarget, fpsLogPath;
    DWORD duration = 0;
    std::vector<Click> clicks;
    std::vector<Drag> drags;
    std::vector<TypedText> typed;
    bool dump = false;
    bool downloadIa = false;
    bool highFrameRateOption = false;
    int smoothOption = -1;
    int popupReason = 0;
    DWORD activateAt = 0;
    for (int i = 1; i < argc; ++i) {
        const std::wstring key = argv[i];
        if (key == L"--dump") {
            dump = true;
            continue;
        }
        if (key == L"--hd" || key == L"--classic" || key == L"--mod") {
            smoothOption = key == L"--classic" ? 0 : key == L"--hd" ? 1 : 2;
            continue;
        }
        if (key == L"--60fps") {
            highFrameRateOption = true;
            continue;
        }
        if (key == L"--download-ia") {
            downloadIa = true;
            continue;
        }
        if (i + 1 >= argc) break;
        const std::wstring value = argv[++i];
        if (key == L"--adm") admPath = value;
        else if (key == L"--snapshot") snapshot = value;
        else if (key == L"--time") duration = DWORD(_wtoi(value.c_str()));
        else if (key == L"--fps-log") fpsLogPath = value;
        else if (key == L"--popup") popupReason = _wtoi(value.c_str());
        else if (key == L"--activate") activateAt = DWORD(_wtoi(value.c_str()));
        else if (key == L"--startup") return setRunAtStartup(value == L"on") ? 0 : 1;
        else if (key == L"--import") importSource = value;
        else if (key == L"--out") importTarget = value;
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
    app.smoothGraphics = smoothOption < 0 ? snapshot.empty() : smoothOption != 0;
    app.experimentalMod = smoothOption == 2;
    app.world.setSmoothAnimation(highFrameRateOption);  // render-only in-between frames
    if (!importSource.empty()) {
        std::wstring error;
        if (importTarget.empty()) importTarget = gamedata::dataDirectory() + L"\\PEST.ADM";
        if (gamedata::importFile(importSource, importTarget, &error)) return 0;
        std::fwprintf(stderr, L"%ls\n", error.c_str());
        return 1;
    }
    if (downloadIa) {
        std::wstring error;
        if (importTarget.empty()) importTarget = gamedata::dataDirectory() + L"\\PEST.ADM";
        if (archive::fetchGameData(importTarget, &error, /*showProgress=*/false)) return 0;
        std::fwprintf(stderr, L"%ls\n", error.c_str());
        return 1;
    }
    // Game data: --adm, [Files] ADB, next to the exe, imported copy, current folder; else ask.
    if (admPath.empty()) admPath = iniAdbPath();
    for (const std::wstring& candidate : {exeDirectory() + L"PEST.ADM", gamedata::dataDirectory() + L"\\PEST.ADM",
                                          std::wstring(L"PEST.ADM")}) {
        if (admPath.empty() && fileExists(candidate)) admPath = candidate;
    }
    if (admPath.empty() && snapshot.empty()) admPath = askForGameData();
    if (admPath.empty()) {
        if (snapshot.empty()) return 0;  // the player cancelled
        admPath = L"PEST.ADM";
    }
    if (!snapshot.empty() || !fpsLogPath.empty()) {  // test runs: a separate save, never the player's pest
        wchar_t temp[MAX_PATH];
        const DWORD n = GetTempPathW(MAX_PATH, temp);
        app.world.setSaveDirectory(std::wstring(temp, n) + L"CyberPest-test");
    }
    if (!loadGame(admPath)) {
        if (snapshot.empty()) MessageBoxW(nullptr, app.error.c_str(), L"CyberPest", MB_ICONERROR);
        return 1;
    }
    if (!snapshot.empty() && popupReason)
        return attention::snapshot(app.db, popupReason, snapshot.c_str(), app.smoothGraphics) ? 0 : 1;
    if (!snapshot.empty())  // unseeded rand: snapshots are reproducible
        return runSnapshot(snapshot, duration, std::move(clicks), std::move(drags), std::move(typed), dump,
                           activateAt);
    std::srand(unsigned(std::time(nullptr)));  // as InitInstance does (1000:25dc)
    setHighFrameRate(highFrameRateOption);
    // A live run with --fps-log ends after --time (ms) so timing can be measured unattended,
    // replaying any --click script (the hatch click, then the pest walks around).
    return runWindow(instance, show, fpsLogPath.empty() ? 0 : duration, fpsLogPath,
                     fpsLogPath.empty() ? std::vector<Click>{} : std::move(clicks));
}
