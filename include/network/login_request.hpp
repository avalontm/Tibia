#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tibia::network {

using RsaCiphertext = std::array<std::uint8_t, 128>;

struct LoginServerRequestFields {
    std::uint32_t tibia_dat_signature{};
    std::uint32_t tibia_spr_signature{};
    std::uint32_t tibia_pic_signature{};
};

// Produces the unencrypted initial application payload (before stream framing).
[[nodiscard]] std::vector<std::uint8_t> make_login_server_request(
    const LoginServerRequestFields& fields,
    const RsaCiphertext& encrypted_rsa_block);

[[nodiscard]] std::vector<std::uint8_t> make_game_server_request(
    const RsaCiphertext& encrypted_rsa_block);

// Applies the initial send-wrapper behavior observed at 0x004E2680: remove
// the two reserved length words, prefix the remaining payload with its 16-bit
// length, and leave the initial request unencrypted.
[[nodiscard]] std::vector<std::uint8_t> frame_initial_login_request(
    std::span<const std::uint8_t> buffered_request);

} // namespace tibia::network
