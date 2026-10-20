#pragma once

#include "crypto/xtea.hpp"
#include "network/login_request.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace tibia::network {

struct PreparedLogin {
    crypto::XteaKeyBytes xtea_key_bytes{};
    crypto::XteaKey xtea_key{};
    // Initial application payload, before TCP framing or XTEA stream encryption.
    std::vector<std::uint8_t> request;
};

// Random inputs are caller-supplied so platform RNG behavior stays explicit.
[[nodiscard]] PreparedLogin prepare_login_server_session(
    const crypto::XteaKeyBytes& xtea_key,
    std::span<const std::uint8_t, 128> rsa_random_block,
    const LoginServerRequestFields& fields,
    std::uint32_t account_number,
    std::string_view password);

[[nodiscard]] PreparedLogin prepare_game_server_session(
    const crypto::XteaKeyBytes& xtea_key,
    std::span<const std::uint8_t, 128> rsa_random_block,
    bool rsa_flag,
    std::uint32_t account_number,
    std::string_view character_name,
    std::string_view password);

} // namespace tibia::network
