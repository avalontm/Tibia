#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace tibia::map {

struct CreatureAppearance {
    std::uint32_t replaced_creature_id{};
    std::uint8_t health_percent{};
    std::uint8_t direction{};
    std::uint16_t outfit_type{};
    std::uint16_t outfit_item{};
    std::array<std::uint8_t, 5> outfit_components{};
    std::uint8_t light_level{};
    std::uint8_t light_color{};
    std::uint16_t speed{};
    std::uint8_t skull{};
    std::uint8_t party_shield{};
    bool has_full_appearance{};
    bool has_direction{};
};

struct Object {
    std::uint16_t type{};
    std::uint8_t extra{};
    bool has_extra{};
    std::uint32_t creature_id{};
    std::uint32_t render_context{};
    std::string creature_name;
    CreatureAppearance creature_appearance;
};

struct Tile {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
    std::vector<Object> objects;
};

struct DecodedMap {
    std::uint16_t center_x{};
    std::uint16_t center_y{};
    std::uint8_t center_floor{};
    std::uint32_t local_player_id{};
    std::vector<Tile> tiles;
    std::vector<std::uint8_t> unparsed_tail;
};

struct ViewportTilePosition {
    std::uint8_t x{};
    std::uint8_t y{};
};

struct ViewportMapPosition {
    std::uint8_t x{};
    std::uint8_t y{};
    std::uint8_t floor{};
};

[[nodiscard]] std::optional<ViewportTilePosition> viewport_tile_position(
    const DecodedMap& map, const Tile& tile) noexcept;

[[nodiscard]] std::optional<ViewportMapPosition> viewport_map_position(
    const DecodedMap& map, const Tile& tile) noexcept;

}
