#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace tibia::crypto {

using XteaKey = std::array<std::uint32_t, 4>;
using XteaKeyBytes = std::array<std::uint8_t, 16>;
using XteaBlock = std::array<std::uint32_t, 2>;

// Reconstructs the 16-byte key copy observed in Tibia.exe. Since the target is
// x86, each consecutive four key bytes become one little-endian word.
[[nodiscard]] XteaKey key_from_bytes(std::span<const std::uint8_t, 16> bytes) noexcept;

// Transcribed from the 32-round block routines observed in Tibia.exe.
void decrypt_block(XteaBlock& block, const XteaKey& key) noexcept;

// Inverse transform for the observed decryption routine.
void encrypt_block(XteaBlock& block, const XteaKey& key) noexcept;

} // namespace tibia::crypto
