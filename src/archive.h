// The first-run option to fetch the original game from the Internet Archive. The remake
// itself never contains or ships the game: this does the same download a player would do
// in a browser, only after they pick it, and copies the game data into %APPDATA%\CyberPest.
// The item, size and SHA-1 were identified on 2026-10-17.
#pragma once

#include <string>

namespace archive {

inline constexpr wchar_t kItemPage[] = L"https://archive.org/details/cyberpest";
inline constexpr wchar_t kFileUrl[] = L"https://archive.org/download/cyberpest/cyberpest.rar";
inline constexpr wchar_t kFileSha1[] = L"03f02d0655362a25baf1b9d047aa356a3b380b9b";

// Downloads the archive, checks it against kFileSha1, unpacks its DATA.Z with Windows'
// tar.exe and copies PEST.ADM to `target`. `showProgress` opens a small window with a
// progress bar and Cancel; false is silent (for --download-ia and tests).
bool fetchGameData(const std::wstring& target, std::wstring* error, bool showProgress);

}  // namespace archive
