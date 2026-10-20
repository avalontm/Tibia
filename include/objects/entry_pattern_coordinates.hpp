#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace tibia::objects {

struct CumulativePatternCoordinates {
    std::size_t pattern_x{};
    std::size_t pattern_z{};
    std::size_t quotient{};
};

[[nodiscard]] std::pair<std::size_t, std::size_t> outfit_direction_coordinates(int direction);
[[nodiscard]] std::pair<std::size_t, std::size_t> hanging_pattern_coordinates(
    std::uint32_t mode,
    std::size_t pattern_width) noexcept;
[[nodiscard]] std::pair<std::size_t, std::size_t> liquid_pattern_coordinates(
    std::uint32_t liquid_type,
    std::size_t pattern_width,
    std::size_t pattern_depth);
[[nodiscard]] CumulativePatternCoordinates cumulative_pattern_coordinates(
    int value,
    std::size_t pattern_width,
    std::size_t pattern_depth);

}
