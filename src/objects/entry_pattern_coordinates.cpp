#include "objects/entry_pattern_coordinates.hpp"

#include <stdexcept>

namespace tibia::objects {

std::pair<std::size_t, std::size_t> outfit_direction_coordinates(int direction) {
    switch (direction) {
    case 0:
        return {2, 1};
    case 1:
        return {3, 1};
    case 2:
        return {3, 2};
    case 3:
        return {3, 3};
    case 4:
        return {2, 3};
    case 5:
        return {1, 3};
    case 6:
        return {1, 2};
    case 7:
        return {1, 1};
    default:
        throw std::out_of_range("outfit direction must be between 0 and 7");
    }
}

std::pair<std::size_t, std::size_t> hanging_pattern_coordinates(
    std::uint32_t mode,
    std::size_t pattern_width) noexcept {
    if (mode == 0x13 && pattern_width > 1) {
        return {2, 1};
    }
    if (mode == 0x14 && pattern_width > 2) {
        return {3, 1};
    }
    return {1, 1};
}

std::pair<std::size_t, std::size_t> liquid_pattern_coordinates(
    std::uint32_t liquid_type,
    std::size_t pattern_width,
    std::size_t pattern_depth) {
    if (pattern_width == 0 || pattern_depth == 0) {
        throw std::invalid_argument("liquid pattern dimensions must be nonzero");
    }

    std::uint32_t mapped = 1;
    switch (liquid_type) {
    case 0:
        mapped = 0;
        break;
    case 2:
    case 10:
        mapped = 7;
        break;
    case 3:
    case 4:
    case 7:
    case 13:
        mapped = 3;
        break;
    case 5:
    case 11:
        mapped = 2;
        break;
    case 6:
        mapped = 4;
        break;
    case 8:
    case 12:
    case 14:
        mapped = 5;
        break;
    case 9:
    case 15:
        mapped = 6;
        break;
    default:
        break;
    }
    return {(mapped & 3U) % pattern_width + 1U,
            (mapped >> 2U) % pattern_depth + 1U};
}

CumulativePatternCoordinates cumulative_pattern_coordinates(
    int value,
    std::size_t pattern_width,
    std::size_t pattern_depth) {
    if (pattern_width == 0 || pattern_depth == 0) {
        throw std::invalid_argument("cumulative pattern dimensions must be nonzero");
    }

    int x = 0;
    int z = 0;
    if (value == 2) {
        x = 1;
    } else if (value == 3) {
        x = 2;
    } else if (value == 4) {
        x = 3;
    } else if (value >= 5 && value < 10) {
        z = 1;
    } else if (value >= 10 && value < 25) {
        x = 1;
        z = 1;
    } else if (value >= 25 && value < 50) {
        x = 2;
        z = 1;
    } else if (value >= 50) {
        x = 3;
        z = 1;
    }

    const auto pattern_x = static_cast<std::size_t>(x);
    const auto pattern_z = static_cast<std::size_t>(z);
    return {pattern_x % pattern_width + 1U,
            pattern_z % pattern_depth + 1U,
            pattern_z / pattern_depth};
}

}
