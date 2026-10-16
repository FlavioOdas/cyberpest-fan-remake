#include "adm.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace adm {
namespace {

uint32_t u32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
int32_t i32(const uint8_t* p) { return static_cast<int32_t>(u32(p)); }

bool is(const uint8_t* p, const char (&tag)[5]) { return std::memcmp(p, tag, 4) == 0; }

std::string cString(const uint8_t* p, size_t max) {
    size_t n = 0;
    while (n < max && p[n]) ++n;
    return std::string(reinterpret_cast<const char*>(p), n);
}

}  // namespace

bool Database::load(const std::wstring& path, std::string* error) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) {
        *error = "cannot open file";
        return false;
    }
    data_.assign(std::istreambuf_iterator<char>(in), {});
    if (data_.size() < 12 || !is(data_.data(), "RIFF") || !is(data_.data() + 8, "ADBM")) {
        *error = "not a CyberPest ADM file";
        return false;
    }
    // The tree ends where the trailing 'RIFF'/'ADBB' index form starts.
    uint32_t end = static_cast<uint32_t>(data_.size());
    for (uint32_t o = end - 4; o > 12; --o) {
        if (is(&data_[o], "RIFF")) {
            end = o;
            break;
        }
    }
    roots_.clear();
    if (!parse(12, end, roots_, 0)) {
        *error = "malformed ADM tree";
        return false;
    }
    files_.clear();
    indexFiles(roots_);
    modules_.clear();
    for (const Node& n : roots_) {
        if (n.tag == Tag::Map) modules_.try_emplace(n.id, &n);
    }
    return true;
}

const Node* Database::findModule(int32_t id) const {
    auto it = modules_.find(id);
    return it == modules_.end() ? nullptr : it->second;
}

const Node* childModule(const Node& module, int32_t id) {
    for (const Node& c : module.children) {
        if (c.tag == Tag::Map && c.id == id) return &c;
    }
    return nullptr;
}

std::vector<int32_t> intValues(const Node& list) {
    std::vector<int32_t> out;
    for (const Node& c : list.children) {
        if (c.tag == Tag::Int) out.push_back(c.intValue);
    }
    return out;
}

const FileRef* fileValue(const Node& list, size_t index) {
    for (const Node& c : list.children) {
        if (c.tag == Tag::FileEntry && index-- == 0) return &c.file;
    }
    return nullptr;
}

bool Database::parse(uint32_t o, uint32_t end, std::vector<Node>& out, int depth) {
    if (depth > 64) return false;
    while (o < end) {
        if (end - o < 8) return false;
        const uint8_t* p = &data_[o];
        const uint32_t size = u32(p + 4);
        const uint32_t payload = o + 8;
        if (size > end - payload) return false;
        Node node;
        node.offset = payload;
        node.size = size;
        const uint8_t* d = p + 8;
        if (is(p, "ADDM") || is(p, "ADDL")) {
            if (size < 12) return false;
            node.tag = is(p, "ADDM") ? Tag::Map : Tag::List;
            node.id = i32(d);
            node.ref = i32(d + 4);
            if (!parse(payload + 12, payload + size, node.children, depth + 1)) return false;
        } else if (is(p, "ADVI") && size >= 4) {
            node.tag = Tag::Int;
            node.intValue = i32(d);
        } else if (is(p, "ADVS") && size >= 4) {
            node.tag = Tag::String;
            node.text = cString(d + 4, std::min<uint32_t>(u32(d), size - 4));
        } else if (is(p, "NDVE") && size >= 12) {
            node.tag = Tag::FileEntry;
            node.file = {cString(d + 12, std::min<uint32_t>(u32(d + 8), size - 12)), u32(d), u32(d + 4)};
            if (node.file.offset > data_.size() || node.file.size > data_.size() - node.file.offset) return false;
        } else if (is(p, "ADFI")) {
            node.tag = Tag::FileBlob;
        }
        out.push_back(std::move(node));
        o = payload + size;
    }
    return true;
}

void Database::indexFiles(const std::vector<Node>& nodes) {
    for (const Node& n : nodes) {
        if (n.tag == Tag::FileEntry) files_.try_emplace(n.file.name, n.file);
        indexFiles(n.children);
    }
}

const FileRef* Database::findFile(std::string_view name) const {
    auto it = files_.find(std::string(name));
    return it == files_.end() ? nullptr : &it->second;
}

std::span<const uint8_t> Database::bytes(const FileRef& file) const {
    return {data_.data() + file.offset, file.size};
}

}  // namespace adm
