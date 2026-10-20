#include "objects/entry_sprite_table.hpp"

#include <array>
#include <limits>
#include <stdexcept>

namespace tibia::objects {

EntrySpriteTable::EntrySpriteTable(
    EntrySpriteDimensions dimensions,
    std::span<const std::uint16_t> sprites)
    : dimensions_(dimensions), sprites_(sprites) {
    const std::array<std::size_t, 7> extents{
        dimensions_.width,
        dimensions_.height,
        dimensions_.layers,
        dimensions_.pattern_width,
        dimensions_.pattern_height,
        dimensions_.pattern_depth,
        dimensions_.phases};
    std::size_t count = 1;
    for (const auto extent : extents) {
        if (extent != 0 && count > std::numeric_limits<std::size_t>::max() / extent) {
            throw std::length_error("entry sprite table dimensions overflow");
        }
        count *= extent;
    }
    if (count > sprites_.size()) {
        throw std::length_error("entry sprite table is shorter than its dimensions");
    }
}

std::uint16_t EntrySpriteTable::at(
    std::size_t x,
    std::size_t y,
    std::size_t layer,
    std::size_t pattern_x,
    std::size_t pattern_y,
    std::size_t pattern_z,
    std::size_t phase) const {
    if (x == 0 || x > dimensions_.width || y == 0 || y > dimensions_.height ||
        layer == 0 || layer > dimensions_.layers || pattern_x == 0 ||
        pattern_x > dimensions_.pattern_width || pattern_y == 0 ||
        pattern_y > dimensions_.pattern_height || pattern_z == 0 ||
        pattern_z > dimensions_.pattern_depth || phase == 0 || phase > dimensions_.phases) {
        throw std::out_of_range("entry sprite coordinate is outside its dimensions");
    }

    auto index = phase - 1;
    index = index * dimensions_.pattern_depth + pattern_z - 1;
    index = index * dimensions_.pattern_height + pattern_y - 1;
    index = index * dimensions_.pattern_width + pattern_x - 1;
    index = index * dimensions_.layers + layer - 1;
    index = index * dimensions_.height + y - 1;
    index = index * dimensions_.width + x - 1;
    if (index >= sprites_.size()) {
        throw std::out_of_range("entry sprite table is shorter than its dimensions");
    }
    return sprites_[index];
}

}
