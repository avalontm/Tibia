#include "crypto/xtea.hpp"

namespace tibia::crypto {
namespace {

constexpr std::uint32_t delta = 0x9E3779B9U;
constexpr unsigned rounds = 32;

constexpr std::uint32_t mix(std::uint32_t value) noexcept {
    return ((value << 4U) ^ (value >> 5U)) + value;
}

} // namespace

XteaKey key_from_bytes(std::span<const std::uint8_t, 16> bytes) noexcept {
    XteaKey key{};
    for (std::size_t word = 0; word < key.size(); ++word) {
        const auto offset = word * 4;
        key[word] = static_cast<std::uint32_t>(bytes[offset]) |
                    (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
                    (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
                    (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    }
    return key;
}

void decrypt_block(XteaBlock& block, const XteaKey& key) noexcept {
    auto& left = block[0];
    auto& right = block[1];
    std::uint32_t sum = delta * rounds;

    for (unsigned i = 0; i < rounds; ++i) {
        right -= mix(left) ^ (sum + key[(sum >> 11U) & 3U]);
        sum -= delta;
        left -= mix(right) ^ (sum + key[sum & 3U]);
    }
}

void encrypt_block(XteaBlock& block, const XteaKey& key) noexcept {
    auto& left = block[0];
    auto& right = block[1];
    std::uint32_t sum = 0;

    for (unsigned i = 0; i < rounds; ++i) {
        left += mix(right) ^ (sum + key[sum & 3U]);
        sum += delta;
        right += mix(left) ^ (sum + key[(sum >> 11U) & 3U]);
    }
}

} // namespace tibia::crypto
