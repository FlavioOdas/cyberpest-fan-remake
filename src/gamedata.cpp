#include "gamedata.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "installer.h"
#include "zip.h"

namespace gamedata {
namespace {

bool fail(std::wstring* error, const std::wstring& text) {
    *error = text;
    return false;
}

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::wstring exeDirectory() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path, n);
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
}

std::wstring tempDirectory() {
    wchar_t temp[MAX_PATH] = L"";
    const DWORD n = GetTempPathW(MAX_PATH, temp);
    return std::wstring(temp, n) + L"CyberPest-import";
}

bool readWholeFile(const std::wstring& path, std::vector<uint8_t>* out) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out->resize(size > 0 ? size_t(size) : 0);
    const bool ok = size > 0 && std::fread(out->data(), 1, out->size(), f) == out->size();
    std::fclose(f);
    return ok;
}

// Writes next to the target and moves it over, so a failed import never leaves a
// half-written PEST.ADM behind.
bool writeFile(const std::wstring& path, const std::vector<uint8_t>& data, std::wstring* error) {
    const std::wstring tmp = path + L".tmp";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return fail(error, L"Can't write " + path + L".");
    const bool stored = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    const bool closed = std::fclose(f) == 0;
    if (!stored || !closed || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        return fail(error, L"Can't write " + path + L".");
    }
    return true;
}

// The original CyberPest game data is an RIFF container; DATA.Z is the
// InstallShield 3 archive (13 5D 65 8C); CyberPest.zip is PKZIP; the Internet Archive's
// cyberpest.rar is RAR 4 (Rar!\x1a\x07\x00).
bool isRiff(std::span<const uint8_t> d) { return d.size() >= 4 && std::memcmp(d.data(), "RIFF", 4) == 0; }

bool isZip(std::span<const uint8_t> d) {
    return d.size() >= 4 && d[0] == 'P' && d[1] == 'K' && d[2] == 3 && d[3] == 4;
}

bool isRar(std::span<const uint8_t> d) {
    return d.size() >= 7 && std::memcmp(d.data(), "Rar!\x1a\x07", 6) == 0 && (d[6] == 0x00 || d[6] == 0x01);
}

std::wstring quote(const std::wstring& s) { return L'"' + s + L'"'; }

void removeTree(const std::wstring& dir, int depth = 0) {
    if (depth > 8) return;
    WIN32_FIND_DATAW entry;
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &entry);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring name = entry.cFileName;
        if (name == L"." || name == L"..") continue;
        const std::wstring child = dir + L"\\" + name;
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) removeTree(child, depth + 1);
        else DeleteFileW(child.c_str());
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    RemoveDirectoryW(dir.c_str());
}

std::wstring findIn(const std::wstring& dir, const std::wstring& name, int depth) {
    if (depth > 8) return {};
    WIN32_FIND_DATAW entry;
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &entry);
    if (find == INVALID_HANDLE_VALUE) return {};
    std::wstring found;
    do {
        const std::wstring childName = entry.cFileName;
        if (childName == L"." || childName == L"..") continue;
        const std::wstring child = dir + L"\\" + childName;
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) found = findIn(child, name, depth + 1);
        else if (lstrcmpiW(childName.c_str(), name.c_str()) == 0) found = child;
    } while (found.empty() && FindNextFileW(find, &entry));
    FindClose(find);
    return found;
}

// Windows' built-in bsdtar (System32\tar.exe, Windows 10 1803+) unpacks RAR and other
// archives libarchive knows. Its exit status is ignored: it warns about one unrelated
// SETUP.BMP entry in the known cyberpest.rar but still writes DATA.Z correctly.
bool runTar(const std::wstring& archive, const std::wstring& dir) {
    wchar_t system[MAX_PATH] = L"";
    const UINT n = GetSystemDirectoryW(system, MAX_PATH);
    const std::wstring tar = std::wstring(system, n) + L"\\tar.exe";
    if (GetFileAttributesW(tar.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    std::wstring command = quote(tar) + L" -xf " + quote(archive) + L" -C " + quote(dir);
    STARTUPINFOW startup = {sizeof startup};
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process))
        return false;
    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

bool importDataZ(std::span<const uint8_t> dataz, const std::wstring& target, std::wstring* error) {
    std::vector<uint8_t> adm;
    std::string why;
    if (!installer::extract(dataz, "PEST.ADM", "SLOW", &adm, &why)) return fail(error, widen(why));
    return writeFile(target, adm, error);
}

bool import(std::span<const uint8_t> bytes, const std::wstring& source, const std::wstring& target,
            std::wstring* error);

bool importTar(const std::wstring& source, const std::wstring& target, std::wstring* error) {
    const std::wstring dir = tempDirectory();
    removeTree(dir);
    CreateDirectoryW(dir.c_str(), nullptr);
    bool ok = false;
    if (!runTar(source, dir)) {
        fail(error, L"Windows' tar.exe is needed to unpack " + source + L", but it is not available.");
    } else {
        std::wstring member = findIn(dir, L"PEST.ADM", 0);
        if (member.empty()) member = findIn(dir, L"DATA.Z", 0);
        if (member.empty()) fail(error, L"No PEST.ADM or DATA.Z inside " + source + L".");
        else ok = importFile(member, target, error);
    }
    removeTree(dir);
    return ok;
}

bool import(std::span<const uint8_t> bytes, const std::wstring& source, const std::wstring& target,
            std::wstring* error) {
    if (isRiff(bytes)) return writeFile(target, std::vector<uint8_t>(bytes.begin(), bytes.end()), error);

    if (isZip(bytes)) {
        std::vector<uint8_t> member;
        std::string why;
        if (zip::extractMember(bytes, "DATA.Z", &member, &why)) return importDataZ(member, target, error);
        if (zip::extractMember(bytes, "PEST.ADM", &member, &why) && isRiff(member))
            return writeFile(target, member, error);
        return fail(error, L"No DATA.Z or PEST.ADM inside " + source + L".");
    }

    if (isRar(bytes)) return importTar(source, target, error);

    return importDataZ(bytes, target, error);  // the installer's DATA.Z itself
}

}  // namespace

std::wstring dataDirectory() {
    const wchar_t* appData = _wgetenv(L"APPDATA");
    std::wstring dir = appData ? std::wstring(appData) + L"\\CyberPest" : exeDirectory();
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring findFile(const std::wstring& directory, const std::wstring& name) {
    return findIn(directory, name, 0);
}

bool importFile(const std::wstring& source, const std::wstring& target, std::wstring* error) {
    std::vector<uint8_t> bytes;
    if (!readWholeFile(source, &bytes)) return fail(error, L"Can't read " + source + L".");
    return import(bytes, source, target, error);
}

}  // namespace gamedata
