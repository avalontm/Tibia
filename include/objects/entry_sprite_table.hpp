#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tibia::objects {

struct EntrySpriteDimensions {
    std::size_t width{};
    std::size_t height{};
    std::size_t layers{};
    std::size_t pattern_width{};
    std::size_t pattern_height{};
    std::size_t pattern_depth{};
    std::size_t phases{};
};

class EntrySpriteTable {
public:
    EntrySpriteTable(EntrySpriteDimensions dimensions, std::span<const std::uint16_t> sprites);

    [[nodiscard]] std::uint16_t at(
        std::size_t x,
        std::size_t y,
        std::size_t layer,
        std::size_t pattern_x,
        std::size_t pattern_y,
        std::size_t pattern_z,
        std::size_t phase) const;

private:
    EntrySpriteDimensions dimensions_;
    std::span<const std::uint16_t> sprites_;
};

}
