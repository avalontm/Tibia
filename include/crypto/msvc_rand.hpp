#pragma once

#include <cstdint>
#include <span>

namespace tibia::crypto {

// Reproduces the classic Microsoft C runtime rand() sequence for an explicit
// srand() seed. The original executable's seed initialization remains unknown.
class MsvcRand {
public:
    explicit MsvcRand(std::uint32_t seed) noexcept : state_(seed) {}

    [[nodiscard]] std::uint16_t next() noexcept;
    void fill_bytes(std::span<std::uint8_t> output) noexcept;

private:
    std::uint32_t state_;
};

} // namespace tibia::crypto
