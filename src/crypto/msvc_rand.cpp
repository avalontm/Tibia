#include "crypto/msvc_rand.hpp"

namespace tibia::crypto {

std::uint16_t MsvcRand::next() noexcept {
    state_ = state_ * 214013U + 2531011U;
    return static_cast<std::uint16_t>((state_ >> 16U) & 0x7FFFU);
}

void MsvcRand::fill_bytes(std::span<std::uint8_t> output) noexcept {
    for (auto& byte : output) {
        byte = static_cast<std::uint8_t>(next());
    }
}

} // namespace tibia::crypto
