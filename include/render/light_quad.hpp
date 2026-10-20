#pragma once

#include <array>
#include <cstdint>

namespace tibia::render {

struct RgbColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

struct LightQuad {
    int x{};
    int y{};
    int width{};
    int height{};
    std::array<RgbColor, 4> corners{};
};

}
