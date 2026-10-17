// Reads a member out of a PKZIP archive in memory, so the original CyberPest.zip (and other
// zips holding the game files) can be imported directly. Stored and deflate entries; the
// inflate is RFC 1951, after zlib's puff.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace zip {

// Reads the entry called `wanted` ("DATA.Z"/"PEST.ADM"; case-insensitive, in any folder)
// from `archive`. Returns false with a message on failure.
bool extractMember(std::span<const uint8_t> archive, const std::string& wanted,
                   std::vector<uint8_t>* out, std::string* error);

}  // namespace zip
