#include "installer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace installer {
namespace {

// PKWARE DCL explode, after zlib's contrib/blast (canonical Huffman codes, bits inverted).
struct Huffman {
    std::array<int16_t, 16> count{};
    std::vector<int16_t> symbol;
};

Huffman construct(std::initializer_list<uint8_t> rep) {
    std::vector<int> lengths;
    for (uint8_t b : rep) lengths.insert(lengths.end(), (b >> 4) + 1, b & 15);
    Huffman h;
    h.symbol.resize(lengths.size());
    for (int len : lengths) ++h.count[size_t(len)];
    std::array<int, 16> offs{};
    for (size_t i = 1; i < 15; ++i) offs[i + 1] = offs[i] + h.count[i];
    for (size_t s = 0; s < lengths.size(); ++s) {
        if (lengths[s]) h.symbol[size_t(offs[size_t(lengths[s])]++)] = int16_t(s);
    }
    return h;
}

const Huffman& lengthCode() {
    static const Huffman h = construct({2, 35, 36, 53, 38, 23});
    return h;
}

const Huffman& distanceCode() {
    static const Huffman h = construct({2, 20, 53, 230, 247, 151, 248});
    return h;
}

constexpr int16_t kBase[16] = {3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264};
constexpr int8_t kExtra[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};

class Bits {
public:
    explicit Bits(std::span<const uint8_t> data) : data_(data) {}
    bool get(int need, int* value) {
        while (count_ < need) {
            if (pos_ >= data_.size()) return false;
            buffer_ |= uint32_t(data_[pos_++]) << count_;
            count_ += 8;
        }
        *value = int(buffer_ & ((1u << need) - 1));
        buffer_ >>= need;
        count_ -= need;
        return true;
    }
    bool decode(const Huffman& h, int* symbol) {
        int code = 0, first = 0, index = 0;
        for (size_t len = 1; len < 16; ++len) {
            int bit = 0;
            if (!get(1, &bit)) return false;
            code |= bit ^ 1;
            const int count = h.count[len];
            if (code < first + count) {
                *symbol = h.symbol[size_t(index + code - first)];
                return true;
            }
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return false;
    }

private:
    std::span<const uint8_t> data_;
    size_t pos_ = 0;
    uint32_t buffer_ = 0;
    int count_ = 0;
};

bool explode(std::span<const uint8_t> in, size_t expected, std::vector<uint8_t>* out, std::string* error) {
    Bits bits(in);
    int literalMode = 0, dictBits = 0;
    if (!bits.get(8, &literalMode) || !bits.get(8, &dictBits)) return *error = "truncated data", false;
    if (literalMode != 0) return *error = "coded literals are not supported", false;
    if (dictBits < 4 || dictBits > 6) return *error = "bad dictionary size", false;
    out->clear();
    out->reserve(expected);
    for (;;) {
        int flag = 0;
        if (!bits.get(1, &flag)) return *error = "compressed data ended early", false;
        if (!flag) {
            int literal = 0;
            if (!bits.get(8, &literal)) return *error = "compressed data ended early", false;
            out->push_back(uint8_t(literal));
            continue;
        }
        int symbol = 0, extra = 0;
        if (!bits.decode(lengthCode(), &symbol) || !bits.get(kExtra[symbol], &extra))
            return *error = "bad length code", false;
        const int length = kBase[symbol] + extra;
        if (length == 519) break;  // end of stream
        const int shift = length == 2 ? 2 : dictBits;
        int high = 0, low = 0;
        if (!bits.decode(distanceCode(), &high) || !bits.get(shift, &low)) return *error = "bad distance code", false;
        const size_t distance = size_t((high << shift) + low + 1);
        if (distance > out->size()) return *error = "distance too far back", false;
        const size_t from = out->size() - distance;
        for (int i = 0; i < length; ++i) out->push_back((*out)[from + size_t(i)]);
    }
    return true;
}

template <typename T>
bool read(std::span<const uint8_t> d, size_t at, T* v) {
    if (at + sizeof(T) > d.size()) return false;
    std::memcpy(v, d.data() + at, sizeof(T));
    return true;
}

bool equalNoCase(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::toupper(uint8_t(x)) == std::toupper(uint8_t(y));
           });
}

bool endsWithNoCase(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && equalNoCase(s.substr(s.size() - tail.size()), tail);
}

}  // namespace

bool extract(std::span<const uint8_t> raw, const std::string& name, const std::string& preferredFolder,
             std::vector<uint8_t>* out, std::string* error) {
    // Header (observed): magic 13 5D 65 8C, file count at 0x0C, table offset at 0x29,
    // folder count at 0x31; folder entries, then one entry per file in folder order.
    static const uint8_t kMagic[4] = {0x13, 0x5d, 0x65, 0x8c};
    if (raw.size() < 0x33 || std::memcmp(raw.data(), kMagic, 4) != 0)
        return *error = "This is not an InstallShield 3 DATA.Z archive.", false;
    uint16_t fileCount = 0, folderCount = 0;
    uint32_t table = 0;
    read(raw, 0x0C, &fileCount), read(raw, 0x29, &table), read(raw, 0x31, &folderCount);
    size_t p = table;
    std::vector<std::string> folderOfFile;
    for (uint16_t i = 0; i < folderCount; ++i) {
        uint16_t count = 0, chunk = 0, length = 0;
        if (!read(raw, p, &count) || !read(raw, p + 2, &chunk) || !read(raw, p + 4, &length) ||
            p + 6 + length > raw.size() || chunk == 0)
            return *error = "Damaged folder table.", false;
        folderOfFile.insert(folderOfFile.end(), count, std::string(reinterpret_cast<const char*>(raw.data() + p + 6), length));
        p += chunk;
    }
    if (folderOfFile.size() != fileCount) return *error = "Damaged file table.", false;

    struct Entry {
        uint32_t size, packedSize, offset;
    };
    Entry best{}, any{};
    bool haveBest = false, haveAny = false;
    for (const std::string& folder : folderOfFile) {
        Entry e{};
        uint16_t chunk = 0;
        if (!read(raw, p + 3, &e.size) || !read(raw, p + 7, &e.packedSize) || !read(raw, p + 11, &e.offset) ||
            !read(raw, p + 0x17, &chunk) || p + 0x1E > raw.size() || chunk == 0)
            return *error = "Damaged file table.", false;
        const size_t length = raw[p + 0x1D];
        if (p + 0x1E + length > raw.size()) return *error = "Damaged file table.", false;
        const std::string file(reinterpret_cast<const char*>(raw.data() + p + 0x1E), length);
        if (equalNoCase(file, name)) {
            if (!haveAny) any = e, haveAny = true;
            if (!haveBest && endsWithNoCase(folder, preferredFolder)) best = e, haveBest = true;
        }
        p += chunk;
    }
    if (!haveAny) return *error = name + " is not in this archive.", false;
    const Entry& e = haveBest ? best : any;
    if (size_t(e.offset) + e.packedSize > raw.size()) return *error = "The archive is incomplete.", false;
    if (!explode(raw.subspan(e.offset, e.packedSize), e.size, out, error)) return false;
    if (out->size() != e.size) return *error = "Decompressed size does not match.", false;
    return true;
}

}  // namespace installer
