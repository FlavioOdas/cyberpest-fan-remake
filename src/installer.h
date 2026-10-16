// Reads files out of the original CyberPest installer's DATA.Z (InstallShield 3 archive,
// PKWARE DCL "implode" compression), so players need nothing but their original disc or
// download. docs/formats/ has the format notes; tools/is3x.py is the reference version.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace installer {

// Finds `name` in the archive, preferring a copy whose folder ends with `preferredFolder`
// (case-insensitive), and decompresses it. Returns false with a message on failure.
bool extract(std::span<const uint8_t> archive, const std::string& name, const std::string& preferredFolder,
             std::vector<uint8_t>* out, std::string* error);

}  // namespace installer
