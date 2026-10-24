// Unit tests for host/psp_savedata_crypto.cpp (spec docs/superpowers/specs/2026-10-23-savedata-routes-design.md).
// Plain executable, run by CTest. Synthetic slot keys, game key and data; the expected values come from the reference
// in tools/savedata.py (checked there against a real save's PARAM.SFO hash).
#include "psp_savedata_crypto.hpp"

#include <openssl/evp.h>

#include <cstdio>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::string hex(const std::uint8_t *data, std::size_t size) {
    std::string out;
    char digits[3];
    for (std::size_t i = 0; i < size; ++i) {
        std::snprintf(digits, sizeof digits, "%02x", data[i]);
        out += digits;
    }
    return out;
}

std::string sha256(const std::vector<std::uint8_t> &data) {
    std::uint8_t digest[32];
    unsigned int size = 0;
    EVP_Digest(data.data(), data.size(), digest, &size, EVP_sha256(), nullptr);
    return hex(digest, size);
}

// A PARAM.SFO with SAVEDATA_PARAMS (first byte `mode`) and SAVEDATA_FILE_LIST naming GAME.DAT with `digest`.
std::vector<std::uint8_t> make_sfo(std::uint8_t mode, std::uint8_t digest_byte) {
    const std::string keys = std::string("SAVEDATA_FILE_LIST") + '\0' + "SAVEDATA_PARAMS" + '\0';
    std::vector<std::uint8_t> files(3168u, 0u), params(128u, 0u);
    const char name[] = "GAME.DAT";
    std::copy(name, name + 8, files.begin());
    for (std::size_t i = 0; i < 16u; ++i) files[13u + i] = digest_byte;
    params[0] = mode;
    std::vector<std::uint8_t> sfo;
    const auto put32 = [&sfo](std::uint32_t v) { for (int b = 0; b < 4; ++b) sfo.push_back(static_cast<std::uint8_t>(v >> (8 * b))); };
    const auto put16 = [&sfo](std::uint16_t v) { sfo.push_back(static_cast<std::uint8_t>(v)); sfo.push_back(static_cast<std::uint8_t>(v >> 8)); };
    const std::uint32_t key_table = 20u + 32u, data_table = key_table + static_cast<std::uint32_t>(keys.size());
    sfo = {0x00, 'P', 'S', 'F'};
    put32(0x101u); put32(key_table); put32(data_table); put32(2u);
    put16(0u); put16(4u); put32(3168u); put32(3168u); put32(0u);
    put16(19u); put16(4u); put32(128u); put32(128u); put32(3168u);
    sfo.insert(sfo.end(), keys.begin(), keys.end());
    sfo.insert(sfo.end(), files.begin(), files.end());
    sfo.insert(sfo.end(), params.begin(), params.end());
    return sfo;
}
} // namespace

int main() {
    gt::KirkSlotKeys keys{};
    for (std::uint8_t i = 0; i < 16u; ++i) {
        keys.slot16[i] = i;
        keys.slot18[i] = static_cast<std::uint8_t>(16u + i);
        keys.slot100[i] = static_cast<std::uint8_t>(100u + i);
    }
    const std::array<std::uint8_t, 16> game_key{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                                0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    std::vector<std::uint8_t> file;
    for (std::uint8_t i = 0x30; i < 0x40; ++i) file.push_back(i);
    for (unsigned i = 0; i < 2100u; ++i) file.push_back(static_cast<std::uint8_t>(i * 7u + 3u));  // crosses 2048

    const auto plain = gt::savedata_decrypt(file, game_key, keys);
    check(plain.has_value() && plain->size() == 2100u, "decrypt: the data after the 16-byte seed");
    if (plain) {
        check(hex(plain->data(), 16) == "b746364ea8cee5ef11851a7e70e75943", "decrypt: first block");
        check(hex(plain->data() + plain->size() - 16, 16) == "0e3cf06c71d2b1f023a3e09a02759711", "decrypt: last block");
        check(sha256(*plain) == "41a291e0f0c4442cb15e4762a35f9b594f29554f4c5cec5d880b0887160439ec", "decrypt: all of it");
    }
    const auto hash = gt::savedata_hash(file, game_key, keys);
    check(hex(hash.data(), 16) == "8fdb9790f77f52c6cac523b911c24b33", "hash");

    auto broken = file;
    broken[1000] ^= 1u;  // one flipped ciphertext bit: the hash no longer matches
    check(gt::savedata_hash(broken, game_key, keys) != hash, "a flipped byte changes the hash");
    check(!gt::savedata_decrypt(std::vector<std::uint8_t>(16u), game_key, keys).has_value(), "16 bytes: no data");

    {  // PARAM.SFO: the crypt flag and the file's hash
        const auto sfo = make_sfo(0x41u, 0xABu);
        const auto info = gt::savedata_sfo_crypto(sfo, "GAME.DAT");
        check(info && info->encrypted && info->hash && (*info->hash)[0] == 0xABu && (*info->hash)[15] == 0xABu,
              "sfo: encrypted, hash of GAME.DAT");
        const auto other = gt::savedata_sfo_crypto(sfo, "OTHER.DAT");
        check(other && other->encrypted && !other->hash, "sfo: a file it does not list has no hash");
        const auto plain = gt::savedata_sfo_crypto(make_sfo(0u, 0u), "GAME.DAT");
        check(plain && !plain->encrypted, "sfo: mode 0 is a plain save");
        check(!gt::savedata_sfo_crypto(std::vector<std::uint8_t>{1, 2, 3}, "GAME.DAT"), "sfo: not a PARAM.SFO");
    }

    if (failures == 0) std::puts("savedata_crypto: all tests passed");
    return failures == 0 ? 0 : 1;
}
