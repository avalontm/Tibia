#include "network/encrypted_frame.hpp"

#include <algorithm>
#include <stdexcept>

namespace tibia::network {
namespace {

std::uint16_t read_u16_le(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t read_u32_le(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void write_u32_le(std::uint8_t* bytes, std::uint32_t value) noexcept {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8U);
    bytes[2] = static_cast<std::uint8_t>(value >> 16U);
    bytes[3] = static_cast<std::uint8_t>(value >> 24U);
}

void write_u16_le(std::uint8_t* bytes, std::uint16_t value) noexcept {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8U);
}

} // namespace

DecodedEncryptedFrame decode_encrypted_frame(
    std::span<const std::uint8_t> frame,
    const crypto::XteaKey& key) {
    if (frame.size() < 2) {
        throw std::invalid_argument("encrypted frame is missing its outer length");
    }

    const auto encrypted_size = static_cast<std::size_t>(read_u16_le(frame.data()));
    if ((encrypted_size & 7U) != 0) {
        throw std::invalid_argument("encrypted body length is not a multiple of 8");
    }
    if (frame.size() != encrypted_size + 2U) {
        throw std::invalid_argument("input must contain exactly one complete frame");
    }

    DecodedEncryptedFrame decoded{
        std::vector<std::uint8_t>(frame.begin(), frame.end()),
        0,
    };

    for (std::size_t offset = 2; offset < decoded.bytes.size(); offset += 8) {
        crypto::XteaBlock block{
            read_u32_le(decoded.bytes.data() + offset),
            read_u32_le(decoded.bytes.data() + offset + 4),
        };
        crypto::decrypt_block(block, key);
        write_u32_le(decoded.bytes.data() + offset, block[0]);
        write_u32_le(decoded.bytes.data() + offset + 4, block[1]);
    }

    // The outer TCP length occupies bytes 0-1. After decrypting from offset 2,
    // the native reader consumes the inner length at its current offset 2 and
    // sets the complete logical size to inner length + both 16-bit headers.
    if (decoded.bytes.size() < 4) {
        throw std::invalid_argument("decrypted body is too short for its inner length");
    }
    decoded.logical_size = static_cast<std::size_t>(read_u16_le(decoded.bytes.data() + 2)) + 4U;
    if (decoded.logical_size > decoded.bytes.size()) {
        throw std::invalid_argument("inner payload length exceeds the decrypted frame");
    }
    return decoded;
}

std::vector<std::uint8_t> encode_encrypted_frame(
    std::span<const std::uint8_t> payload,
    std::span<const std::uint8_t> padding,
    const crypto::XteaKey& key) {
    if (payload.size() > 0xFFFFU) {
        throw std::invalid_argument("payload is too large for the observed 16-bit inner length");
    }

    const auto unpadded_body_size = payload.size() + 2U;
    const auto encrypted_size = (unpadded_body_size + 7U) & ~std::size_t{7U};
    if (encrypted_size > 0xFFF8U) {
        throw std::invalid_argument("encrypted body is too large for the observed 16-bit outer length");
    }
    const auto padding_size = encrypted_size - unpadded_body_size;
    if (padding.size() != padding_size) {
        throw std::invalid_argument("padding span must exactly fill the final 8-byte block");
    }

    std::vector<std::uint8_t> frame(encrypted_size + 2U);
    write_u16_le(frame.data(), static_cast<std::uint16_t>(encrypted_size));
    write_u16_le(frame.data() + 2, static_cast<std::uint16_t>(payload.size()));
    std::copy(payload.begin(), payload.end(), frame.begin() + 4);
    std::copy(padding.begin(), padding.end(), frame.begin() + 4 + payload.size());

    for (std::size_t offset = 2; offset < frame.size(); offset += 8) {
        crypto::XteaBlock block{
            read_u32_le(frame.data() + offset),
            read_u32_le(frame.data() + offset + 4),
        };
        crypto::encrypt_block(block, key);
        write_u32_le(frame.data() + offset, block[0]);
        write_u32_le(frame.data() + offset + 4, block[1]);
    }

    return frame;
}

} // namespace tibia::network
