#include "zip.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace zip {
namespace {

constexpr size_t kMaxUnpacked = 64u << 20;  // refuses bomb-sized or corrupt output

uint16_t u16(std::span<const uint8_t> d, size_t at) {
    return uint16_t(uint16_t(d[at]) | uint16_t(d[at + 1]) << 8);
}

uint32_t u32(std::span<const uint8_t> d, size_t at) {
    return uint32_t(d[at]) | uint32_t(d[at + 1]) << 8 | uint32_t(d[at + 2]) << 16 | uint32_t(d[at + 3]) << 24;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

bool fail(std::string* error, const std::string& text) {
    *error = text;
    return false;
}

// Deflate bit stream: bits come least significant first (RFC 1951 3.1.1).
class Bits {
public:
    explicit Bits(std::span<const uint8_t> data) : data_(data) {}

    bool get(int need, uint32_t* value) {
        while (count_ < need) {
            if (pos_ >= data_.size()) return false;
            buffer_ |= uint64_t(data_[pos_++]) << count_;
            count_ += 8;
        }
        *value = uint32_t(buffer_ & ((uint32_t(1) << need) - 1));
        buffer_ >>= need;
        count_ -= need;
        return true;
    }

    void align() { buffer_ = 0, count_ = 0; }  // skips the rest of the current byte

    bool aligned16(uint32_t* value) {
        if (count_ != 0 || pos_ + 2 > data_.size()) return false;
        *value = uint32_t(data_[pos_]) | uint32_t(data_[pos_ + 1]) << 8;
        pos_ += 2;
        return true;
    }

    bool raw(size_t count, std::vector<uint8_t>* out) {
        if (count_ != 0 || pos_ + count > data_.size()) return false;
        out->insert(out->end(), data_.begin() + pos_, data_.begin() + pos_ + count);
        pos_ += count;
        return true;
    }

private:
    std::span<const uint8_t> data_;
    size_t pos_ = 0;
    uint64_t buffer_ = 0;
    int count_ = 0;
};

struct Huffman {
    std::array<uint16_t, 16> count{};
    std::vector<uint16_t> symbol;
};

bool buildHuffman(const uint8_t* lengths, size_t n, Huffman* h) {
    *h = Huffman{};
    for (size_t i = 0; i < n; ++i) ++h->count[lengths[i]];
    if (h->count[0] == n) return false;  // no codes at all
    int left = 1;
    for (int len = 1; len < 16; ++len) {
        left = left * 2 - h->count[len];
        if (left < 0) return false;  // over-subscribed
    }
    std::array<uint16_t, 16> offset{};
    for (int len = 1; len < 15; ++len) offset[len + 1] = uint16_t(offset[len] + h->count[len]);
    h->symbol.resize(n);
    for (size_t i = 0; i < n; ++i) {
        if (lengths[i]) h->symbol[offset[lengths[i]]++] = uint16_t(i);
    }
    return true;
}

bool decodeSymbol(Bits& bits, const Huffman& h, int* symbol) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
        uint32_t bit = 0;
        if (!bits.get(1, &bit)) return false;
        code |= int(bit);
        const int count = h.count[len];
        if (code < first + count) {
            *symbol = h.symbol[size_t(index + code - first)];
            return true;
        }
        index += count;
        first = (first + count) * 2;
        code *= 2;
    }
    return false;
}

constexpr uint16_t kLengthBase[29] = {3,  4,  5,  6,  7,  8,  9,  10,  11,  13,  15,  17,  19,  23, 27,
                                      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t kDistanceBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,    25,
                                        33,   49,   65,   97,   129,  193,   257,   385,   513,   769,
                                        1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
constexpr uint8_t kDistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4,  4,  5,  5,  6,
                                        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

Huffman makeFixedLiteral() {
    std::vector<uint8_t> lengths(288);
    for (int i = 0; i < 144; ++i) lengths[i] = 8;
    for (int i = 144; i < 256; ++i) lengths[i] = 9;
    for (int i = 256; i < 280; ++i) lengths[i] = 7;
    for (int i = 280; i < 288; ++i) lengths[i] = 8;
    Huffman h;
    buildHuffman(lengths.data(), lengths.size(), &h);
    return h;
}

const Huffman& fixedLiteral() {
    static const Huffman h = makeFixedLiteral();
    return h;
}

const Huffman& fixedDistance() {
    static const Huffman h = [] {
        std::vector<uint8_t> lengths(32, 5);
        Huffman built;
        buildHuffman(lengths.data(), lengths.size(), &built);
        return built;
    }();
    return h;
}

bool inflate(std::span<const uint8_t> raw, size_t expected, std::vector<uint8_t>* out, std::string* error) {
    Bits bits(raw);
    out->clear();
    out->reserve(std::min(expected, kMaxUnpacked));
    auto copy = [&](int length, int distance) {
        if (distance < 1 || size_t(distance) > out->size() || out->size() + size_t(length) > kMaxUnpacked)
            return false;
        const size_t from = out->size() - size_t(distance);
        for (int i = 0; i < length; ++i) out->push_back((*out)[from + size_t(i)]);
        return true;
    };

    bool last = false;
    while (!last) {
        uint32_t bit = 0, type = 0;
        if (!bits.get(1, &bit) || !bits.get(2, &type)) return fail(error, "The deflate data ends early.");
        last = bit != 0;
        if (type == 0) {  // stored block
            bits.align();
            uint32_t length = 0, complement = 0;
            if (!bits.aligned16(&length) || !bits.aligned16(&complement) || (length ^ 0xFFFF) != complement)
                return fail(error, "Damaged stored deflate block.");
            if (out->size() + length > kMaxUnpacked || !bits.raw(length, out))
                return fail(error, "The deflate data ended early.");
        } else if (type == 1 || type == 2) {
            Huffman literal, distance;
            bool distanceUsable = true;
            if (type == 1) {
                literal = fixedLiteral();
                distance = fixedDistance();
            } else {
                uint32_t hlit = 0, hdist = 0, hclen = 0;
                if (!bits.get(5, &hlit) || !bits.get(5, &hdist) || !bits.get(4, &hclen))
                    return fail(error, "The deflate data ends early.");
                hlit += 257, hdist += 1, hclen += 4;
                static const uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
                uint8_t lengths[19] = {};
                for (uint32_t i = 0; i < hclen; ++i) {
                    uint32_t value = 0;
                    if (!bits.get(3, &value)) return fail(error, "The deflate data ends early.");
                    lengths[kOrder[i]] = uint8_t(value);
                }
                Huffman codeLengths;
                if (!buildHuffman(lengths, 19, &codeLengths) || hlit + hdist > 320)
                    return fail(error, "Damaged deflate code-length codes.");
                std::vector<uint8_t> table(hlit + hdist);
                size_t at = 0;
                while (at < table.size()) {
                    int symbol = 0;
                    if (!decodeSymbol(bits, codeLengths, &symbol))
                        return fail(error, "Damaged deflate codes.");
                    if (symbol < 16) {
                        table[at++] = uint8_t(symbol);
                        continue;
                    }
                    uint32_t repeat = 0;
                    int extra = 0;
                    uint8_t value = 0;
                    if (symbol == 16) {
                        if (at == 0 || !bits.get(2, &repeat)) return fail(error, "Damaged deflate repeat.");
                        value = table[at - 1], extra = 3;
                    } else if (symbol == 17) {
                        if (!bits.get(3, &repeat)) return fail(error, "Damaged deflate repeat.");
                        extra = 3;
                    } else {
                        if (!bits.get(7, &repeat)) return fail(error, "Damaged deflate repeat.");
                        extra = 11;
                    }
                    const size_t count = size_t(repeat) + size_t(extra);
                    if (at + count > table.size()) return fail(error, "Damaged deflate repeat.");
                    for (size_t i = 0; i < count; ++i) table[at++] = value;
                }
                if (table[256] == 0) return fail(error, "Damaged deflate codes.");
                if (!buildHuffman(table.data(), hlit, &literal)) return fail(error, "Damaged deflate codes.");
                distanceUsable = buildHuffman(table.data() + hlit, hdist, &distance);
            }
            for (;;) {
                int symbol = 0;
                if (!decodeSymbol(bits, literal, &symbol)) return fail(error, "Damaged deflate data.");
                if (symbol < 256) {
                    if (out->size() >= kMaxUnpacked) return fail(error, "The unpacked data is too large.");
                    out->push_back(uint8_t(symbol));
                } else if (symbol == 256) {
                    break;
                } else {
                    symbol -= 257;
                    if (symbol >= 29) return fail(error, "Damaged deflate data.");
                    uint32_t extra = 0;
                    if (!bits.get(kLengthExtra[symbol], &extra)) return fail(error, "The deflate data ends early.");
                    const int length = kLengthBase[symbol] + int(extra);
                    int distanceSymbol = 0;
                    if (!distanceUsable || !decodeSymbol(bits, distance, &distanceSymbol) || distanceSymbol >= 30)
                        return fail(error, "Damaged deflate data.");
                    if (!bits.get(kDistanceExtra[distanceSymbol], &extra))
                        return fail(error, "The deflate data ends early.");
                    if (!copy(length, int(kDistanceBase[distanceSymbol] + extra)))
                        return fail(error, "Damaged deflate back-reference.");
                }
            }
        } else {
            return fail(error, "Unknown deflate block type.");
        }
    }
    if (out->size() != expected) return fail(error, "The unpacked size does not match the ZIP entry.");
    return true;
}

}  // namespace

bool extractMember(std::span<const uint8_t> archive, const std::string& wanted, std::vector<uint8_t>* out,
                   std::string* error) {
    constexpr uint32_t kEocd = 0x06054b50, kCentral = 0x02014b50, kLocal = 0x04034b50;
    if (archive.size() < 22) return fail(error, "This is not a ZIP archive.");
    size_t eocd = SIZE_MAX;
    const size_t last = archive.size() - 22;
    const size_t first = last > 65535 ? last - 65535 : 0;
    for (size_t i = last + 1; i-- > first;) {
        if (u32(archive, i) == kEocd) {
            eocd = i;
            break;
        }
    }
    if (eocd == SIZE_MAX) return fail(error, "This is not a ZIP archive.");
    if (u16(archive, eocd + 10) == 0xFFFF)
        return fail(error, "ZIP64 archives are not supported; unpack the file with Explorer first.");
    const size_t count = u16(archive, eocd + 10);
    const size_t cdOffset = u32(archive, eocd + 16);
    if (cdOffset == 0xFFFFFFFFu) return fail(error, "ZIP64 archives are not supported; unpack the file with Explorer first.");

    const std::string wantedLower = lower(wanted);
    struct Entry {
        int rank = 0;
        uint16_t method = 0, flags = 0;
        uint32_t packedSize = 0, size = 0, localOffset = 0;
    } best;
    size_t p = cdOffset;
    for (size_t i = 0; i < count && p + 46 <= archive.size(); ++i) {
        if (u32(archive, p) != kCentral) break;
        const uint16_t flags = u16(archive, p + 8), method = u16(archive, p + 10);
        const uint32_t packedSize = u32(archive, p + 20), size = u32(archive, p + 24);
        const uint16_t nameLength = u16(archive, p + 28), extraLength = u16(archive, p + 30);
        const uint16_t commentLength = u16(archive, p + 32);
        const uint32_t localOffset = u32(archive, p + 42);
        if (p + 46 + nameLength + extraLength + commentLength > archive.size())
            return fail(error, "The ZIP archive is damaged.");
        const std::string name(reinterpret_cast<const char*>(archive.data() + p + 46), nameLength);
        p += 46 + nameLength + extraLength + commentLength;
        if (name.empty() || name.back() == '/') continue;  // folder
        const std::string lowerName = lower(name);
        int rank = 0;
        if (lowerName == wantedLower) rank = 2;
        else {
            const size_t slash = lowerName.find_last_of('/');
            if (lowerName.substr(slash == std::string::npos ? 0 : slash + 1) == wantedLower) rank = 1;
        }
        if (rank > best.rank) best = {rank, method, flags, packedSize, size, localOffset};
    }
    if (best.rank == 0) return fail(error, wanted + " is not in this ZIP archive.");
    if (best.flags & 1) return fail(error, "The ZIP entry is encrypted.");
    if (best.size == 0xFFFFFFFFu || best.packedSize == 0xFFFFFFFFu)
        return fail(error, "ZIP64 archives are not supported; unpack the file with Explorer first.");
    if (best.localOffset + 30 > archive.size() || u32(archive, best.localOffset) != kLocal)
        return fail(error, "The ZIP archive is damaged.");
    const size_t nameAt = best.localOffset + 30 + u16(archive, best.localOffset + 26) + u16(archive, best.localOffset + 28);
    if (nameAt + best.packedSize > archive.size()) return fail(error, "The ZIP archive is damaged.");
    const std::span<const uint8_t> packed = archive.subspan(nameAt, best.packedSize);
    if (best.method == 0) {
        out->assign(packed.begin(), packed.end());
        return true;
    }
    if (best.method == 8) return inflate(packed, best.size, out, error);
    return fail(error, "Unsupported ZIP compression method.");
}

}  // namespace zip
