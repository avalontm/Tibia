#include "client/map_window_light_sources.hpp"

#include "client/map_window_item_frame.hpp"
#include "client/game_world_snapshot.hpp"
#include "render/creature_projection.hpp"
#include "resources/tibia_dat.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace tibia::client {
namespace {

map::RgbLight palette_color(std::uint16_t index) noexcept {
    const auto red = std::min<std::uint32_t>((index / 36U) * 0x33U, 0xffU);
    const auto green = ((index / 6U) % 6U) * 0x33U;
    const auto blue = (index % 6U) * 0x33U;
    return {{{static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green),
        static_cast<std::uint8_t>(blue)}}};
}

std::optional<std::uint32_t> creature_native_u32(
    const network::State6CreatureRuntimeState& creature,
    std::uint16_t offset) noexcept {
    if (offset < network::State6CreatureRuntimeState::first_native_offset)
        return std::nullopt;
    const auto index = static_cast<std::size_t>(
        offset - network::State6CreatureRuntimeState::first_native_offset);
    if (index + 4 > creature.native_fields.size()) return std::nullopt;
    const auto& fields = creature.native_fields;
    return static_cast<std::uint32_t>(fields[index]) |
        (static_cast<std::uint32_t>(fields[index + 1]) << 8U) |
        (static_cast<std::uint32_t>(fields[index + 2]) << 16U) |
        (static_cast<std::uint32_t>(fields[index + 3]) << 24U);
}

struct CreatureLightAppearance {
    const resources::TibiaDatObject* object{};
    std::int32_t displacement_x{};
    std::int32_t displacement_y{};
};

CreatureLightAppearance resolve_creature_light_appearance(
    const GameWorldCreatureSnapshot& creature,
    const resources::TibiaDatFile& dat) noexcept {
    CreatureLightAppearance result;
    const auto outfit_type = creature_native_u32(creature.state, 0x60);
    if (!outfit_type) return result;

    if (*outfit_type != 0 &&
        *outfit_type <= std::numeric_limits<std::uint16_t>::max()) {
        result.object = dat.outfits.find(static_cast<std::uint16_t>(*outfit_type));
    } else if (*outfit_type == 0) {
        const auto outfit_item = creature_native_u32(creature.state, 0x64);
        if (!outfit_item) return result;
        if (*outfit_item != 0 &&
            *outfit_item <= std::numeric_limits<std::uint16_t>::max()) {
            result.object = dat.find_item(static_cast<std::uint16_t>(*outfit_item));
        } else if (creature.state.creature_id < 0x40000000U) {
            result.displacement_x = 8;
            result.displacement_y = 8;
        }
    }

    if (result.object == nullptr) return result;
    const auto* displacement = result.object->find_attribute(0x19);
    if (displacement != nullptr && displacement->payload_words.size() == 2) {
        result.displacement_x = displacement->payload_words[0];
        result.displacement_y = displacement->payload_words[1];
    }
    return result;
}

}

std::vector<map::LightSource> collect_map_window_item_light_sources(
    const MapWindowItemFrame& frame, const resources::TibiaDatFile& dat) {
    std::vector<map::LightSource> sources;

    for (const auto& entry : frame.visits) {
        const auto& visit = entry.visit;
        if (visit.neighboring_object_pass || visit.tile == nullptr) continue;
        std::int32_t height_adjustment{};

        for (const auto* object : visit.ordered_objects) {
            if (object->creature_id != 0 || object->type == 0 || object->type == 0x63)
                continue;
            const auto* item = dat.find_item(object->type);
            if (item == nullptr) continue;

            const auto* light = item->find_attribute(0x16);
            if (light != nullptr && light->payload_words.size() == 2 &&
                light->payload_words[0] != 0) {
                std::int32_t displacement_x{};
                std::int32_t displacement_y{};
                const auto* displacement = item->find_attribute(0x19);
                if (displacement != nullptr) {
                    if (displacement->payload_words.size() != 2) {
                        light = nullptr;
                    } else {
                        displacement_x = displacement->payload_words[0];
                        displacement_y = displacement->payload_words[1];
                    }
                }
                if (light != nullptr) {
                    sources.push_back({
                        visit.anchor_x + frame.camera_offset_x - height_adjustment -
                            displacement_x + 16,
                        visit.anchor_y + frame.camera_offset_y - height_adjustment -
                            displacement_y + 16,
                        light->payload_words[0],
                        visit.floor,
                        palette_color(light->payload_words[1])
                    });
                }
            }

            const auto* height = item->find_attribute(0x1a);
            if (height != nullptr && height->payload_words.size() == 1)
                height_adjustment = std::min<std::int32_t>(
                    height_adjustment + height->payload_words[0], 0x18);
        }
    }
    return sources;
}

std::vector<map::LightSource> collect_map_window_creature_light_sources(
    const GameWorldSnapshot& snapshot, const MapWindowItemFrame& frame,
    const resources::TibiaDatFile& dat) {
    std::vector<map::LightSource> sources;
    if (!snapshot.map) return sources;

    for (const auto& creature : snapshot.creatures) {
        if (!creature.viewport_map_position ||
            creature.viewport_map_position->floor > frame.highest_active_floor)
            continue;
        const auto movement = render::resolve_creature_movement_offset(
            creature.state, snapshot.map->local_player_id,
            snapshot.local_player_movement);
        if (!movement) continue;
        const auto position = render::project_creature_screen_position(
            creature.viewport_map_position->x, creature.viewport_map_position->y,
            0, 0, *movement, 0, 0);
        if (!position) continue;

        const auto& fields = creature.state.native_fields;
        const auto level_index = static_cast<std::size_t>(
            0x78 - network::State6CreatureRuntimeState::first_native_offset);
        const auto color_index = static_cast<std::size_t>(
            0x7c - network::State6CreatureRuntimeState::first_native_offset);
        const auto level = fields[level_index];
        const auto color = fields[color_index];
        const auto appearance = resolve_creature_light_appearance(creature, dat);
        const auto source_x = static_cast<std::int64_t>(position->x) + 16 -
            appearance.displacement_x;
        const auto source_y = static_cast<std::int64_t>(position->y) + 16 -
            appearance.displacement_y;
        if (source_x < std::numeric_limits<std::int32_t>::min() ||
            source_x > std::numeric_limits<std::int32_t>::max() ||
            source_y < std::numeric_limits<std::int32_t>::min() ||
            source_y > std::numeric_limits<std::int32_t>::max())
            continue;

        const auto floor = static_cast<std::uint8_t>(
            creature.viewport_map_position->floor);
        if (level != 0)
            sources.push_back({static_cast<std::int32_t>(source_x),
                static_cast<std::int32_t>(source_y), level, floor,
                palette_color(color)});
        if (appearance.object != nullptr) {
            const auto* light = appearance.object->find_attribute(0x16);
            if (light != nullptr && light->payload_words.size() == 2 &&
                light->payload_words[0] != 0)
                sources.push_back({static_cast<std::int32_t>(source_x),
                    static_cast<std::int32_t>(source_y), light->payload_words[0],
                    floor, palette_color(light->payload_words[1])});
        }
        if (creature.state.creature_id == snapshot.map->local_player_id && level < 2)
            sources.push_back({static_cast<std::int32_t>(source_x),
                static_cast<std::int32_t>(source_y), 2, floor,
                map::RgbLight{{0xff, 0xff, 0xff}}});
    }
    return sources;
}

}
