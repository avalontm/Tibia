#pragma once

#include "crypto/xtea.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace tibia::network {

using RsaLoginBlock = std::array<std::uint8_t, 128>;

// Rebuilds the bytes placed in the 128-byte RSA input buffer by the login
// request path. The account number is serialized as a 32-bit little-endian
// value before the length-prefixed password. random_block supplies the initial block;
// bytes not overwritten by fields remain as the caller provided them.
[[nodiscard]] RsaLoginBlock make_login_rsa_block(
    const crypto::XteaKeyBytes& xtea_key,
    std::uint32_t account_number,
    std::string_view password,
    std::span<const std::uint8_t, 128> random_block);

// Rebuilds the corresponding game-login block. The account number is followed
// by the selected character name and password; flag semantics remain open.
[[nodiscard]] RsaLoginBlock make_game_login_rsa_block(
    const crypto::XteaKeyBytes& xtea_key,
    bool flag,
    std::uint32_t account_number,
    std::string_view character_name,
    std::string_view password,
    std::span<const std::uint8_t, 128> random_block);

} // namespace tibia::network
