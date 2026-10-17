// Getting the player's PEST.ADM in place: from a PEST.ADM, the original installer's DATA.Z,
// the original CyberPest.zip (read natively) or a RAR/other archive Windows' tar.exe can
// unpack. Used by the first-run import and by the Internet Archive download.
#pragma once

#include <string>

namespace gamedata {

// %APPDATA%\CyberPest (created if needed), where PEST.ADM and the save live.
std::wstring dataDirectory();

// Copies the game's PEST.ADM out of `source` to `target`, whatever form the player has it
// in (PEST.ADM, DATA.Z, CyberPest.zip, cyberpest.rar, ...). Returns false with a message.
bool importFile(const std::wstring& source, const std::wstring& target, std::wstring* error);

// Recursive search for a file name (case-insensitive) under `directory`; "" when absent.
std::wstring findFile(const std::wstring& directory, const std::wstring& name);

}  // namespace gamedata
