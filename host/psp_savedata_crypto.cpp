#include "psp_savedata_crypto.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <cstring>
#include <memory>

namespace gt {

namespace {
using Block = std::array<std::uint8_t, 16>;

// Firmware constants of the mode-5 cipher and hash (tools/savedata.py).
constexpr Block kSeedMask{0xEC, 0x6D, 0x29, 0x59, 0x26, 0x35, 0xA5, 0x7F, 0x97, 0x2A, 0x0D, 0xBC, 0xA3, 0x26, 0x33, 0x00};
constexpr Block kSeedResultMask{0x70, 0x44, 0xA3, 0xAE, 0xEF, 0x5D, 0xA5, 0xF2,
                                0x85, 0x7F, 0xF2, 0xD6, 0x94, 0xF5, 0x36, 0x3B};
constexpr Block kHashMask{0xCB, 0x15, 0xF4, 0x07, 0xF9, 0x6A, 0x52, 0x3C, 0x04, 0xB9, 0xB2, 0xEE, 0x5C, 0x53, 0xFA, 0x86};

// AES-128 on whole 16-byte blocks, no padding: KIRK commands 4 (encrypt) and 7 (decrypt) with a slot key.
void aes_blocks(const Block &key, const std::uint8_t *in, std::uint8_t *out, std::size_t size, bool encrypt) {
    const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    EVP_CipherInit_ex(ctx.get(), EVP_aes_128_ecb(), nullptr, key.data(), nullptr, encrypt ? 1 : 0);
    EVP_CIPHER_CTX_set_padding(ctx.get(), 0);
    int written = 0;
    EVP_CipherUpdate(ctx.get(), out, &written, in, static_cast<int>(size));
}

Block aes_block(const Block &key, const Block &in, bool encrypt) {
    Block out{};
    aes_blocks(key, in.data(), out.data(), 16u, encrypt);
    return out;
}

Block xor_block(Block a, const std::uint8_t *b) {
    for (std::size_t i = 0; i < 16u; ++i) a[i] ^= b[i];
    return a;
}

std::vector<std::uint8_t> padded(std::span<const std::uint8_t> file) {
    std::vector<std::uint8_t> data(file.begin(), file.end());
    data.resize((data.size() + 15u) & ~std::size_t{15u}, 0u);
    return data;
}

// Multiplication by x in GF(2^128) (CMAC's subkey step).
Block double_block(const Block &in) {
    Block out{};
    for (std::size_t i = 0; i < 15u; ++i) out[i] = static_cast<std::uint8_t>((in[i] << 1u) | (in[i + 1u] >> 7u));
    out[15] = static_cast<std::uint8_t>((in[15] << 1u) ^ ((in[0] & 0x80u) != 0u ? 0x87u : 0u));
    return out;
}
} // namespace

std::optional<std::vector<std::uint8_t>> savedata_decrypt(std::span<const std::uint8_t> file,
                                                          std::span<const std::uint8_t, 16> game_key,
                                                          const KirkSlotKeys &keys) {
    if (file.size() <= 16u) return std::nullopt;
    std::vector<std::uint8_t> data = padded(file);
    // The seed: the file's first block with the game key, through slot 18.
    Block head{};
    std::copy_n(data.begin(), 16u, head.begin());
    const Block seed = xor_block(aes_block(keys.slot18, xor_block(xor_block(head, game_key.data()), kSeedMask.data()),
                                           false),
                                 kSeedResultMask.data());
    // The keystream: slot 100 decrypting the counter blocks seed[0..11] || le32(i + 1), each XORed with the previous
    // counter block (a zero block before the first) — AES-CBC decryption of the counter sequence.
    const std::size_t blocks = (data.size() - 16u) / 16u;
    std::vector<std::uint8_t> counters(blocks * 16u), stream(blocks * 16u);
    for (std::size_t i = 0; i < blocks; ++i) {
        std::memcpy(&counters[i * 16u], seed.data(), 12u);
        const std::uint32_t counter = static_cast<std::uint32_t>(i + 1u);
        for (std::size_t b = 0; b < 4u; ++b) counters[i * 16u + 12u + b] = static_cast<std::uint8_t>(counter >> (8u * b));
    }
    aes_blocks(keys.slot100, counters.data(), stream.data(), stream.size(), false);
    for (std::size_t i = 16u; i < stream.size(); ++i) stream[i] ^= counters[i - 16u];
    std::vector<std::uint8_t> plain(file.size() - 16u);
    for (std::size_t i = 0; i < plain.size(); ++i) plain[i] = static_cast<std::uint8_t>(data[16u + i] ^ stream[i]);
    return plain;
}

std::array<std::uint8_t, 16> savedata_hash(std::span<const std::uint8_t> file,
                                           std::span<const std::uint8_t, 16> game_key, const KirkSlotKeys &keys) {
    const std::vector<std::uint8_t> data = padded(file);
    // CBC-MAC with slot 16 over all but the last block, the last block masked with the CMAC subkey.
    Block state{};
    for (std::size_t offset = 0; offset + 16u < data.size(); offset += 16u)
        state = aes_block(keys.slot16, xor_block(state, &data[offset]), true);
    const Block subkey = double_block(aes_block(keys.slot16, Block{}, true));
    const std::uint8_t *last = data.empty() ? subkey.data() : &data[data.size() - 16u];
    Block tag = aes_block(keys.slot16, xor_block(xor_block(state, last), subkey.data()), true);
    // Then the firmware's mask and the game key, through slot 16 once more.
    tag = aes_block(keys.slot16, xor_block(xor_block(tag, kHashMask.data()), game_key.data()), true);
    return tag;
}

std::optional<SavedataSfoCrypto> savedata_sfo_crypto(std::span<const std::uint8_t> sfo, std::string_view file_name) {
    const auto u32 = [&sfo](std::size_t at) {
        return static_cast<std::uint32_t>(sfo[at] | sfo[at + 1u] << 8u | sfo[at + 2u] << 16u | sfo[at + 3u] << 24u);
    };
    if (sfo.size() < 20u || sfo[0] != 0u || sfo[1] != 'P' || sfo[2] != 'S' || sfo[3] != 'F') return std::nullopt;
    const std::uint32_t key_table = u32(8u), data_table = u32(12u), count = u32(16u);
    if (20u + std::size_t{count} * 16u > sfo.size()) return std::nullopt;
    SavedataSfoCrypto result;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t entry = 20u + std::size_t{i} * 16u;
        const std::size_t name_at = key_table + (sfo[entry] | sfo[entry + 1u] << 8u);
        const std::size_t length = u32(entry + 4u), data_at = data_table + std::size_t{u32(entry + 12u)};
        if (name_at >= sfo.size() || data_at + length > sfo.size()) return std::nullopt;
        std::string_view name(reinterpret_cast<const char *>(&sfo[name_at]), sfo.size() - name_at);
        name = name.substr(0u, name.find('\0'));
        const auto value = sfo.subspan(data_at, length);
        if (name == "SAVEDATA_PARAMS") {
            result.encrypted = !value.empty() && value[0] != 0u;
        } else if (name == "SAVEDATA_FILE_LIST") {  // entries of 0x20: name (13), hash (16), padding (3)
            for (std::size_t at = 0; at + 0x20u <= value.size(); at += 0x20u) {
                std::string_view listed(reinterpret_cast<const char *>(&value[at]), 13u);
                listed = listed.substr(0u, listed.find('\0'));
                if (listed != file_name || listed.empty()) continue;
                std::array<std::uint8_t, 16> hash{};
                std::copy_n(&value[at + 13u], 16u, hash.begin());
                result.hash = hash;
            }
        }
    }
    return result;
}

} // namespace gt
