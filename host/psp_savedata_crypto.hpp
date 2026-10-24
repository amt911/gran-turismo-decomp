// The PSP firmware's savedata cipher and hash, mode 5 (GT: SDK 5.x with a game key), on OpenSSL's AES
// (host/psp_savedata_crypto.cpp). Spec: docs/superpowers/specs/2026-10-23-savedata-routes-design.md; the algorithm
// and its reference implementation are described in tools/savedata.py.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace gt {

// KIRK commands 4/7's AES keys for the slots savedata mode 5 uses. Never in the repo: the build generates them from
// the pinned pspdecrypt checkout (tools/savedata.py keys), and tests pass synthetic ones.
struct KirkSlotKeys {
    std::array<std::uint8_t, 16> slot16{};
    std::array<std::uint8_t, 16> slot18{};
    std::array<std::uint8_t, 16> slot100{};
};

// The plaintext of an encrypted savedata file: its first 16 bytes seed the cipher, the rest is data. Empty when the
// file has no data after the seed.
std::optional<std::vector<std::uint8_t>> savedata_decrypt(std::span<const std::uint8_t> file,
                                                          std::span<const std::uint8_t, 16> game_key,
                                                          const KirkSlotKeys &keys);

// The hash PARAM.SFO's SAVEDATA_FILE_LIST keeps for the file (of its encrypted bytes, padded with zeroes to 16).
std::array<std::uint8_t, 16> savedata_hash(std::span<const std::uint8_t> file,
                                           std::span<const std::uint8_t, 16> game_key, const KirkSlotKeys &keys);

// What PARAM.SFO says about a file: whether the save is encrypted (SAVEDATA_PARAMS' first byte is not 0) and the
// file's hash in SAVEDATA_FILE_LIST (none when the file is not listed). Empty for bytes that are not a PARAM.SFO.
struct SavedataSfoCrypto {
    bool encrypted{};
    std::optional<std::array<std::uint8_t, 16>> hash;
};
std::optional<SavedataSfoCrypto> savedata_sfo_crypto(std::span<const std::uint8_t> sfo, std::string_view file_name);

} // namespace gt
