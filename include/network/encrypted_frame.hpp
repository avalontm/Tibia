#pragma once

#include "crypto/xtea.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tibia::network {

struct DecodedEncryptedFrame {
    // Contains the two-byte outer length followed by the decrypted body.
    std::vector<std::uint8_t> bytes;

    // Ghidra observed the client reading the inner 16-bit length at offset 2
    // and setting its logical buffer size to that value plus four bytes.
    std::size_t logical_size{};
};

// Decodes one complete frame as observed in Tibia.exe around 0x004e2a60.
// Throws std::invalid_argument for malformed or incomplete frames.
[[nodiscard]] DecodedEncryptedFrame decode_encrypted_frame(
    std::span<const std::uint8_t> frame,
    const crypto::XteaKey& key);

// Builds the encrypted form observed in the outgoing network path. Padding
// bytes are supplied by the caller; the original client filled them with rand().
[[nodiscard]] std::vector<std::uint8_t> encode_encrypted_frame(
    std::span<const std::uint8_t> payload,
    std::span<const std::uint8_t> padding,
    const crypto::XteaKey& key);

} // namespace tibia::network
