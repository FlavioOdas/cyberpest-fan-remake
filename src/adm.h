// PEST.ADM "Application Database" reader. Format: docs/formats/adm.md.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace adm {

enum class Tag { Map, List, Int, String, FileEntry, FileBlob, Other };  // ADDM ADDL ADVI ADVS NDVE ADFI

struct FileRef {
    std::string name;
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct Node {
    Tag tag = Tag::Other;
    uint32_t offset = 0;  // payload offset within the file
    uint32_t size = 0;
    int32_t id = 0;       // Map/List
    int32_t ref = 0;      // Map/List
    std::vector<Node> children;
    int32_t intValue = 0;  // Int
    std::string text;      // String
    FileRef file;          // FileEntry
};

class Database {
public:
    // Loads and parses the whole file. Returns false and sets *error on failure.
    bool load(const std::wstring& path, std::string* error);

    const std::vector<Node>& roots() const { return roots_; }
    // Top-level module with this id (modules are the ADDM chunks at the root of the file).
    const Node* findModule(int32_t id) const;
    // First virtual file with this name (names are case-sensitive, upper case in the data).
    const FileRef* findFile(std::string_view name) const;
    std::span<const uint8_t> bytes(const FileRef& file) const;

private:
    bool parse(uint32_t begin, uint32_t end, std::vector<Node>& out, int depth);
    void indexFiles(const std::vector<Node>& nodes);

    std::vector<uint8_t> data_;
    std::vector<Node> roots_;
    std::unordered_map<std::string, FileRef> files_;
    std::unordered_map<int32_t, const Node*> modules_;
};

// Helpers for walking modules. A module's children are lists (`Tag::List`, whose `ref` is the
// command symbol inside states) and sub-modules (`Tag::Map`, e.g. states with ref -109).
const Node* childModule(const Node& module, int32_t id);
std::vector<int32_t> intValues(const Node& list);
const FileRef* fileValue(const Node& list, size_t index = 0);

}  // namespace adm
