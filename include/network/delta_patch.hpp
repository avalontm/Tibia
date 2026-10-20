#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tibia::network {

// Applies the command stream observed in the client's state-4 patch finalizer
// to an already-loaded base file. The result is returned in memory.
[[nodiscard]] std::vector<std::uint8_t> apply_observed_delta_patch(
    std::span<const std::uint8_t> base,
    std::span<const std::uint8_t> patch_stream,
    std::optional<std::uint32_t> expected_base_prefix = std::nullopt);

} // namespace tibia::network
