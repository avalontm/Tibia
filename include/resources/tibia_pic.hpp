#pragma once

#include "resources/tibia_spr.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tibia::resources {

struct TibiaPicPicture {
    std::uint8_t tiles_x{};
    std::uint8_t tiles_y{};
    TibiaSprColor color_key{};
    std::vector<std::uint32_t> tile_offsets;
};

struct TibiaPicFile {
    std::uint32_t signature{};
    std::vector<TibiaPicPicture> pictures;
    std::size_t parsed_index_bytes{};
};

// Parses the observed Tibia 7.92 picture table. Picture/tile IDs are one-based.
[[nodiscard]] TibiaPicFile parse_tibia_pic(std::span<const std::uint8_t> bytes);
[[nodiscard]] TibiaSprImage decode_tibia_pic_tile(
    std::span<const std::uint8_t> bytes, const TibiaPicFile& file,
    std::uint16_t picture_id, std::uint16_t tile_x, std::uint16_t tile_y);

} // namespace tibia::resources
