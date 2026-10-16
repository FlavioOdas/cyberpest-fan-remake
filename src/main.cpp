// CyberPest remake. Loads the user's original PEST.ADM and runs the room defined in it.
// Options:
//   --adm <path>        PEST.ADM to load (default: next to the exe, then the current folder)
//   --snapshot <bmp>    render the first frame to an 8-bit BMP and exit (no window, no sound)
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <memory>
#include <string>

#include "adm.h"
#include "audio.h"
#include "flc.h"
#include "scene.h"
#include "surface.h"
#include "symbols.h"
#include "wav.h"

namespace {

constexpr int kWidth = 640, kHeight = 480;

struct App {
    adm::Database db;
    Palette palette{};
    Surface screen{kWidth, kHeight};
    Scene scene;
    Mixer mixer;
    std::wstring error;
} app;

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::wstring exeDirectory() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path, n);
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
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
    if (!app.scene.loadRoom(app.db, sym::RoomScreen, &err)) {
        app.error = L"Can't build the room: " + widen(err);
        return false;
    }
    return true;
}

void compose() {
    app.screen.fill(0);
    app.scene.draw(app.screen);
}

void tick(DWORD) {}

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
        tick(GetTickCount());
        compose();
        HDC dc = GetDC(hwnd);
        present(hwnd, dc);
        ReleaseDC(hwnd, dc);
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring admPath, snapshot;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::wstring key = argv[i], value = argv[i + 1];
        if (key == L"--adm") admPath = value;
        else if (key == L"--snapshot") snapshot = value;
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
    if (!snapshot.empty()) {
        compose();
        return writeBmp(snapshot.c_str(), app.screen, app.palette) ? 0 : 2;
    }
    return runWindow(instance);
}
