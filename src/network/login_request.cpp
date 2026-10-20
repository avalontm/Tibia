#include "network/login_request.hpp"

#include <stdexcept>

namespace tibia::network {
namespace {

void write_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void write_reserved_lengths(std::vector<std::uint8_t>& out) {
    write_u16_le(out, 0);
    write_u16_le(out, 0);
}

void write_protocol_header(std::vector<std::uint8_t>& out, std::uint8_t command) {
    out.push_back(command);
    write_u16_le(out, 2);
    write_u16_le(out, 792);
}

void write_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

} // namespace

std::vector<std::uint8_t> make_login_server_request(
    const LoginServerRequestFields& fields,
    const RsaCiphertext& encrypted_rsa_block) {
    std::vector<std::uint8_t> out;
    out.reserve(4 + 5 + 12 + encrypted_rsa_block.size());
    write_reserved_lengths(out);
    write_protocol_header(out, 1);
    write_u32_le(out, fields.tibia_dat_signature);
    write_u32_le(out, fields.tibia_spr_signature);
    write_u32_le(out, fields.tibia_pic_signature);
    out.insert(out.end(), encrypted_rsa_block.begin(), encrypted_rsa_block.end());
    return out;
}

std::vector<std::uint8_t> make_game_server_request(
    const RsaCiphertext& encrypted_rsa_block) {
    std::vector<std::uint8_t> out;
    out.reserve(4 + 5 + encrypted_rsa_block.size());
    write_reserved_lengths(out);
    write_protocol_header(out, 10);
    out.insert(out.end(), encrypted_rsa_block.begin(), encrypted_rsa_block.end());
    return out;
}

std::vector<std::uint8_t> frame_initial_login_request(
    std::span<const std::uint8_t> buffered_request) {
    if (buffered_request.size() < 4)
        throw std::invalid_argument("initial login buffer is shorter than its reserved header");
    if (buffered_request[0] != 0 || buffered_request[1] != 0 ||
        buffered_request[2] != 0 || buffered_request[3] != 0)
        throw std::invalid_argument("initial login buffer must contain two zeroed length slots");
    const auto payload_size = buffered_request.size() - 4U;
    if (payload_size > 0xFFFFU)
        throw std::invalid_argument("initial login payload exceeds its 16-bit length field");
    std::vector<std::uint8_t> wire;
    wire.reserve(payload_size + 2U);
    write_u16_le(wire, static_cast<std::uint16_t>(payload_size));
    wire.insert(wire.end(), buffered_request.begin() + 4, buffered_request.end());
    return wire;
}

} // namespace tibia::network
