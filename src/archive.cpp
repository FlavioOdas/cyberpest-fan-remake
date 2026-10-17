#include "archive.h"

#include <windows.h>
#include <commctrl.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

#include "gamedata.h"

namespace archive {
namespace {

constexpr wchar_t kUserAgent[] = L"CyberPest-Remake/1.0 (+https://github.com/FlavioOdas/cyberpest)";
constexpr UINT kRefresh = WM_APP, kFinished = WM_APP + 1;
constexpr int kCancelId = 1;
constexpr int kBaseWidth = 400, kBaseHeight = 150;

enum Phase { kDownloading, kChecking, kCopying };

struct Fetch {
    HWND window = nullptr;
    std::wstring target;
    std::wstring download;
    std::atomic<bool> cancelled{false};
    std::atomic<int> phase{kDownloading};
    std::atomic<int> percent{0};
    std::atomic<bool> finished{false};
    bool ok = false;
    std::wstring error;
};

HWND g_phase = nullptr, g_source = nullptr, g_bar = nullptr, g_cancel = nullptr;

bool fail(std::wstring* error, const std::wstring& text) {
    *error = text;
    return false;
}

void post(Fetch* fetch) {
    if (fetch->window) PostMessageW(fetch->window, kRefresh, 0, 0);
}

bool crack(const std::wstring& url, std::wstring* host, INTERNET_PORT* port, std::wstring* path) {
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof parts;
    parts.dwSchemeLength = DWORD(-1);
    parts.dwHostNameLength = DWORD(-1);
    parts.dwUrlPathLength = DWORD(-1);
    parts.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), DWORD(url.size()), 0, &parts)) return false;
    *host = std::wstring(parts.lpszHostName, parts.dwHostNameLength);
    *path = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength) path->append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    *port = parts.nPort;
    return parts.nScheme == INTERNET_SCHEME_HTTPS;
}

bool download(const std::wstring& url, const std::wstring& dest, Fetch* fetch, std::wstring* error) {
    std::wstring host, path;
    INTERNET_PORT port = 0;
    if (!crack(url, &host, &port, &path)) return fail(error, L"Bad download address.");
    HINTERNET session = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return fail(error, L"Can't start an Internet connection.");
    WinHttpSetTimeouts(session, 30000, 30000, 30000, 60000);
    HINTERNET connection = WinHttpConnect(session, host.c_str(), port, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                        WINHTTP_FLAG_SECURE)
                                   : nullptr;

    bool ok = false;
    std::wstring why;
    if (!request) {
        why = L"Can't reach " + host + L".";
    } else if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        why = L"Can't send the download request.";
    } else if (!WinHttpReceiveResponse(request, nullptr)) {
        why = L"No answer from " + host + L".";
    } else {
        DWORD status = 0, size = sizeof status;
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
            why = L"The answer from " + host + L" was not understood.";
        } else if (status != 200) {
            why = L"The Internet Archive answered with error " + std::to_wstring(status) + L".";
        } else {
            uint64_t expected = 0;
            DWORD length = 0;
            size = sizeof length;
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &length, &size, WINHTTP_NO_HEADER_INDEX))
                expected = length;
            FILE* file = _wfopen(dest.c_str(), L"wb");
            if (!file) {
                why = L"Can't write to " + dest + L".";
            } else {
                std::vector<uint8_t> buffer(64 << 10);
                uint64_t total = 0;
                int lastPercent = -1;
                for (;;) {
                    if (fetch->cancelled) {
                        why = L"The download was cancelled.";
                        break;
                    }
                    DWORD available = 0;
                    if (!WinHttpQueryDataAvailable(request, &available)) {
                        why = L"The download stopped.";
                        break;
                    }
                    if (available == 0) {
                        ok = true;
                        break;
                    }
                    bool failed = false;
                    while (available > 0) {
                        const DWORD want = std::min<DWORD>(available, DWORD(buffer.size()));
                        DWORD got = 0;
                        if (!WinHttpReadData(request, buffer.data(), want, &got) || got == 0 ||
                            std::fwrite(buffer.data(), 1, got, file) != got) {
                            why = L"The download stopped.";
                            failed = true;
                            break;
                        }
                        available -= got;
                        total += got;
                    }
                    if (failed) break;
                    const int percent = expected ? int(total * 100 / expected) : 0;
                    if (percent != lastPercent) {
                        fetch->percent = percent;
                        lastPercent = percent;
                        post(fetch);
                    }
                }
                std::fclose(file);
                if (ok && expected && total != expected) {
                    why = L"The download is incomplete.";
                    ok = false;
                }
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!ok) {
        DeleteFileW(dest.c_str());
        *error = why.empty() ? L"The download failed." : why;
    }
    return ok;
}

bool sha1OfFile(const std::wstring& path, std::wstring* hex, std::wstring* error) {
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return fail(error, L"Can't check the downloaded file (no crypto provider).");
    HCRYPTHASH hash = 0;
    bool ok = CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash) != 0;
    FILE* file = ok ? _wfopen(path.c_str(), L"rb") : nullptr;
    if (!file) ok = false;
    std::vector<uint8_t> buffer(64 << 10);
    while (ok) {
        const size_t got = std::fread(buffer.data(), 1, buffer.size(), file);
        if (got == 0) break;
        ok = CryptHashData(hash, buffer.data(), DWORD(got), 0) != 0;
    }
    if (file) {
        ok = ok && std::ferror(file) == 0;
        std::fclose(file);
    }
    BYTE digest[20] = {};
    DWORD length = sizeof digest;
    ok = ok && CryptGetHashParam(hash, HP_HASHVAL, digest, &length, 0) != 0;
    if (hash) CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    if (!ok) return fail(error, L"Can't check the downloaded file.");
    static const wchar_t kDigits[] = L"0123456789abcdef";
    hex->clear();
    for (BYTE byte : digest) {
        *hex += kDigits[byte >> 4];
        *hex += kDigits[byte & 15];
    }
    return true;
}

// Download, fingerprint check, unpack and import: the worker's whole job.
bool runFetch(Fetch* fetch) {
    fetch->phase = kDownloading, fetch->percent = 0;
    post(fetch);
    if (!download(kFileUrl, fetch->download, fetch, &fetch->error)) return false;
    if (fetch->cancelled) return fail(&fetch->error, L"The download was cancelled.");
    fetch->phase = kChecking, fetch->percent = 0;
    post(fetch);
    std::wstring sha;
    if (!sha1OfFile(fetch->download, &sha, &fetch->error)) return false;
    if (lstrcmpiW(sha.c_str(), kFileSha1) != 0) {
        DeleteFileW(fetch->download.c_str());
        return fail(&fetch->error,
                    L"The file at the Internet Archive does not match the copy this version was checked "
                    L"with; it may have been changed at the source. Download it in a browser from " +
                        std::wstring(kItemPage) + L", or use a CyberPest file you already have.");
    }
    fetch->phase = kCopying;
    post(fetch);
    const bool ok = gamedata::importFile(fetch->download, fetch->target, &fetch->error);
    DeleteFileW(fetch->download.c_str());
    return ok;
}

DWORD WINAPI fetchThread(LPVOID param) {
    auto* fetch = static_cast<Fetch*>(param);
    fetch->ok = runFetch(fetch);
    fetch->finished = true;
    if (fetch->window) PostMessageW(fetch->window, kFinished, 0, 0);
    return 0;
}

LRESULT CALLBACK progressProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* fetch = reinterpret_cast<Fetch*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case kRefresh: {
        if (!fetch) return 0;
        wchar_t text[256] = L"";
        if (fetch->phase == kDownloading) {
            if (fetch->percent > 0)
                swprintf(text, 256, L"Downloading the original CyberPest from the Internet Archive... %d%%",
                         int(fetch->percent));
            else wcscpy(text, L"Downloading the original CyberPest from the Internet Archive...");
        } else if (fetch->phase == kChecking) {
            wcscpy(text, L"Checking the downloaded file...");
        } else {
            wcscpy(text, L"Copying the game data into place...");
        }
        SetWindowTextW(g_phase, text);
        if (fetch->phase == kDownloading) SendMessageW(g_bar, PBM_SETPOS, fetch->percent, 0);
        else EnableWindow(g_cancel, FALSE);
        return 0;
    }
    case kFinished:
        return 0;
    case WM_CLOSE:
        if (fetch && !fetch->finished) fetch->cancelled = true;
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == kCancelId && fetch && !fetch->finished) fetch->cancelled = true;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

HWND createProgressWindow(Fetch* fetch) {
    static bool registered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = progressProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CyberPestArchive";
        registered = RegisterClassW(&wc) != 0;
    }
    const int dpi = int(GetDpiForSystem());
    const int width = MulDiv(kBaseWidth, dpi, 96), height = MulDiv(kBaseHeight, dpi, 96);
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND hwnd = CreateWindowExW(0, L"CyberPestArchive", L"CyberPest", WS_CAPTION | WS_SYSMENU, x, y, width,
                                height, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return nullptr;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(fetch));
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);
    g_phase = CreateWindowExW(0, L"STATIC", L"Downloading the original CyberPest from the Internet Archive...",
                              WS_CHILD | WS_VISIBLE, MulDiv(16, dpi, 96), MulDiv(14, dpi, 96),
                              width - MulDiv(32, dpi, 96), MulDiv(20, dpi, 96), hwnd, nullptr, instance, nullptr);
    g_source = CreateWindowExW(0, L"STATIC", kItemPage, WS_CHILD | WS_VISIBLE, MulDiv(16, dpi, 96),
                               MulDiv(38, dpi, 96), width - MulDiv(32, dpi, 96), MulDiv(20, dpi, 96), hwnd,
                               nullptr, instance, nullptr);
    g_bar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE, MulDiv(16, dpi, 96),
                            MulDiv(70, dpi, 96), width - MulDiv(32, dpi, 96), MulDiv(20, dpi, 96), hwnd, nullptr,
                            instance, nullptr);
    g_cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                               width - MulDiv(96, dpi, 96), MulDiv(102, dpi, 96), MulDiv(80, dpi, 96),
                               MulDiv(26, dpi, 96), hwnd,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCancelId)), instance, nullptr);
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    for (HWND control : {g_phase, g_source, g_bar, g_cancel})
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(g_bar, PBM_SETRANGE32, 0, 100);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    return hwnd;
}

}  // namespace

bool fetchGameData(const std::wstring& target, std::wstring* error, bool showProgress) {
    Fetch fetch;
    fetch.target = target;
    const std::wstring dir = gamedata::dataDirectory() + L"\\download";
    CreateDirectoryW(dir.c_str(), nullptr);
    fetch.download = dir + L"\\cyberpest.rar";

    if (!showProgress) {
        if (runFetch(&fetch)) return true;
        *error = fetch.error;
        return false;
    }
    HWND window = createProgressWindow(&fetch);
    if (!window) {  // no window: still fetch, just without progress
        if (runFetch(&fetch)) return true;
        *error = fetch.error;
        return false;
    }
    fetch.window = window;
    DWORD threadId = 0;
    HANDLE thread = CreateThread(nullptr, 0, fetchThread, &fetch, 0, &threadId);
    if (!thread) {
        DestroyWindow(window);
        if (runFetch(&fetch)) return true;
        *error = fetch.error;
        return false;
    }
    MSG msg;
    while (!fetch.finished) {
        const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    if (IsWindow(window)) DestroyWindow(window);
    if (!fetch.ok) *error = fetch.error;
    return fetch.ok;
}

}  // namespace archive
