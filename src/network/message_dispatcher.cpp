#include "network/message_dispatcher.hpp"
#include "map/tile_object_order.hpp"
#include "network/status_line_state.hpp"
#include "resources/tibia_dat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace tibia::network {

namespace {

std::uint16_t handler_key(DispatchPhase phase, std::uint8_t opcode) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(phase) << 8U) | opcode);
}

std::uint8_t projectile_direction(const State6ProjectileEffect& effect) noexcept {
    const auto from_x = effect.local_from.x;
    const auto from_y = effect.local_from.y;
    const auto to_x = effect.local_to.x;
    const auto to_y = effect.local_to.y;
    if (to_x == from_x) return to_y <= from_y ? 4 : 0;
    const auto dy = static_cast<std::int64_t>(to_y - from_y) * 0x100;
    if (from_x < to_x) {
        const auto dx = static_cast<std::int64_t>(to_x - from_x);
        if (dx * 0x26a < dy) return 4;
        if (dx * 0x6a < dy) return 3;
        if (static_cast<std::int64_t>(from_x - to_x) * 0x6a < dy) return 2;
        return static_cast<std::int64_t>(from_x - to_x) * 0x26a < dy ? 1 : 0;
    }
    const auto dx = static_cast<std::int64_t>(from_x - to_x);
    if (dx * 0x26a < dy) return 4;
    if (dx * 0x6a < dy) return 5;
    if (static_cast<std::int64_t>(to_x - from_x) * 0x6a < dy) return 6;
    return dy <= static_cast<std::int64_t>(to_x - from_x) * 0x26a ? 7 : 0;
}

void skip_unknown_creature_update(PacketReader& reader) {
    (void)reader.read_u8();
    (void)reader.read_u8();
    if (reader.read_u16() == 0) {
        (void)reader.read_u16();
        (void)reader.read_u8();
        (void)reader.read_u8();
        (void)reader.read_u16();
        return;
    }
    for (int i = 0; i < 5; ++i) (void)reader.read_u8();
    (void)reader.read_u16();
}

std::string read_state6_creature_name(PacketReader& reader, std::uint32_t creature_id) {
    auto name = reader.read_string();
    if (name.size() > 29) name.resize(29);
    if (creature_id < 0x40000000U) return name;
    bool capitalize_next = true;
    for (auto& character : name) {
        const auto value = static_cast<std::uint8_t>(character);
        const bool ascii_letter = (value >= 'a' && value <= 'z') ||
            (value >= 'A' && value <= 'Z');
        const bool extended_letter = value == 0xc4 || value == 0xd6 ||
            value == 0xdc || value == 0xdf || value == 0xe4 ||
            value == 0xf6 || value == 0xfc;
        if (!ascii_letter && !extended_letter) {
            capitalize_next = true;
            continue;
        }
        if (capitalize_next && ((value > 0x60 && value < 0x7b) ||
            (value > 0xdf && value != 0xff)))
            character = static_cast<char>(value - 0x20);
        capitalize_next = false;
    }
    return name;
}

State6MapTile read_map_tile(PacketReader& reader, std::uint16_t x, std::uint16_t y,
    std::uint8_t floor, const std::vector<std::uint16_t>& extra_ids,
    std::unordered_map<std::uint32_t, std::string>& known_creatures,
    std::uint16_t& skipped_tiles,
    bool stop_after_first = false) {
    State6MapTile tile{x, y, floor, {}};
    if (skipped_tiles != 0) {
        --skipped_tiles;
        return tile;
    }
    const auto consume_appearance = [&reader](bool with_name,
        std::string* creature_name, tibia::map::CreatureAppearance& appearance) {
        std::uint32_t creature_id = 0;
        if (with_name) {
            appearance.replaced_creature_id = reader.read_u32();
            creature_id = reader.read_u32();
            auto name = read_state6_creature_name(reader, creature_id);
            if (creature_name) *creature_name = std::move(name);
        }
        appearance.health_percent = reader.read_u8();
        appearance.direction = reader.read_u8();
        appearance.has_direction = true;
        appearance.outfit_type = reader.read_u16();
        if (appearance.outfit_type == 0) {
            appearance.outfit_item = reader.read_u16();
        } else {
            for (auto& component : appearance.outfit_components)
                component = reader.read_u8();
        }
        appearance.light_level = reader.read_u8();
        appearance.light_color = reader.read_u8();
        appearance.speed = reader.read_u16();
        appearance.skull = reader.read_u8();
        appearance.party_shield = reader.read_u8();
        appearance.has_full_appearance = true;
        return creature_id;
    };
    while (true) {
        const auto type = reader.read_u16();
        if (type > 0xFEFF) {
            skipped_tiles = static_cast<std::uint16_t>(type - 0xFF00);
            break;
        }
        if (type == 0) throw std::runtime_error("state-6 map contains invalid field type 0");
        State6MapObject object;
        object.type = type;
        object.render_context = 0;
        if (type == 0x61) {
            object.creature_id = consume_appearance(true, &object.creature_name,
                object.creature_appearance);
            if (object.creature_appearance.replaced_creature_id != 0)
                known_creatures.erase(object.creature_appearance.replaced_creature_id);
            known_creatures.insert_or_assign(object.creature_id,
                object.creature_name);
        } else if (type == 0x62) {
            object.creature_id = reader.read_u32();
            if (const auto found = known_creatures.find(object.creature_id);
                found != known_creatures.end()) {
                object.creature_name = found->second;
                (void)consume_appearance(false, nullptr,
                    object.creature_appearance);
            }
            else {
                skip_unknown_creature_update(reader);
            }
        } else if (type == 0x63) {
            object.creature_id = reader.read_u32();
            object.creature_appearance.direction = reader.read_u8();
            object.creature_appearance.has_direction = true;
            if (const auto found = known_creatures.find(object.creature_id);
                found != known_creatures.end()) {
                object.creature_name = found->second;
            }
        } else if (std::binary_search(extra_ids.begin(), extra_ids.end(), type)) {
            object.extra = reader.read_u8();
            object.has_extra = true;
        }
        if (tile.objects.size() >= 10)
            throw std::runtime_error("state-6 map tile exceeds the observed ten-object limit");
        tile.objects.push_back(object);
        if (stop_after_first) break;
    }
    return tile;
}

void seed_known_creatures(
    std::unordered_map<std::uint32_t, std::string>& known_creatures,
    const State6DecodedMap* map) {
    if (map == nullptr) return;
    for (const auto& tile : map->tiles) {
        for (const auto& object : tile.objects) {
            if (object.creature_id != 0)
                known_creatures.insert_or_assign(object.creature_id,
                    object.creature_name);
        }
    }
}

State6ContainerItem read_container_item(PacketReader& reader,
    const std::vector<std::uint16_t>& extra_ids) {
    State6ContainerItem item;
    item.type = reader.read_u16();
    if (std::binary_search(extra_ids.begin(), extra_ids.end(), item.type)) {
        item.extra = reader.read_u8();
        item.has_extra = true;
    }
    return item;
}

State6WirePosition read_state6_wire_position(PacketReader& reader) {
    return {reader.read_u16(), reader.read_u16(), reader.read_u8()};
}

std::pair<int, int> map_floor_range(std::uint8_t center_floor) {
    if (center_floor > 15) throw std::runtime_error("state-6 map floor is outside 0..15");
    return center_floor < 8 ? std::pair{0, 8} :
        std::pair{4, std::max(-1, static_cast<int>(center_floor) - 14)};
}

State6MapTile project_map_viewport_tile(std::uint16_t center_x,
    std::uint16_t center_y, std::uint8_t center_floor, std::int32_t local_x,
    std::int32_t local_y, std::int32_t viewport_floor) {
    const auto center_viewport_floor = center_floor < 8
        ? 7 - static_cast<std::int32_t>(center_floor) : 2;
    const auto floor_offset = viewport_floor - center_viewport_floor;
    const auto world_x = static_cast<std::uint16_t>(std::clamp(
        static_cast<std::int32_t>(center_x) + local_x - 8 + floor_offset, 0, 65535));
    const auto world_y = static_cast<std::uint16_t>(std::clamp(
        static_cast<std::int32_t>(center_y) + local_y - 6 + floor_offset, 0, 65535));
    const auto world_floor = static_cast<std::uint8_t>(
        static_cast<std::int32_t>(center_floor) - floor_offset);
    return {world_x, world_y, world_floor, {}};
}

std::optional<bool> state6_object_has_native_property(
    const State6MapObject& object, const resources::TibiaDatFile& dat,
    std::uint8_t property_id) {
    if (object.creature_id != 0) return property_id == 13;
    if (object.type == 0) return false;
    const auto* item = dat.find_item(object.type);
    if (item == nullptr) return std::nullopt;
    return item->has_native_property(property_id);
}

std::optional<std::size_t> state6_move_insertion_index(
    const std::vector<State6MapObject>& objects, const State6MapObject& moved,
    const resources::TibiaDatFile& dat) {
    for (std::size_t index = 0; index < objects.size(); ++index) {
        bool same_ordering_properties = true;
        for (std::uint8_t property = 0; property < 4; ++property) {
            const auto moved_has_property = state6_object_has_native_property(
                moved, dat, property);
            const auto current_has_property = state6_object_has_native_property(
                objects[index], dat, property);
            if (!moved_has_property || !current_has_property) return std::nullopt;
            if (*moved_has_property != *current_has_property) {
                if (*moved_has_property) return index;
                same_ordering_properties = false;
                break;
            }
        }
        if (same_ordering_properties &&
            (moved.creature_id != 0 || objects[index].creature_id != 0))
            return index;
    }
    return objects.size();
}

void insert_state6_map_object(std::vector<State6MapObject>& objects,
    const State6MapObject& object, const resources::TibiaDatFile& dat) {
    const auto observed_index = state6_move_insertion_index(objects, object, dat);
    const auto index = observed_index.value_or(objects.size());
    if (index >= 10) return;
    objects.insert(objects.begin() + static_cast<std::ptrdiff_t>(index), object);
    if (objects.size() > 10) objects.pop_back();
}

}

void MessageDispatcher::register_handler(
    DispatchPhase phase,
    std::uint8_t opcode,
    Handler handler) {
    if (!handler) {
        throw std::invalid_argument("message handler must be callable");
    }
    const auto [it, inserted] = handlers_.emplace(handler_key(phase, opcode), std::move(handler));
    if (!inserted) {
        throw std::invalid_argument("message opcode already has a handler");
    }
}

void register_observed_ping_reply_handler(
    MessageDispatcher& dispatcher,
    EncryptedPayloadSender send_encrypted_payload) {
    if (!send_encrypted_payload) {
        throw std::invalid_argument("encrypted payload sender must be callable");
    }
    dispatcher.register_handler(DispatchPhase::state6, 0x1E,
        [sender = std::move(send_encrypted_payload)](PacketReader&) {
        constexpr std::uint8_t reply_opcode = 0x1E;
        sender(std::span<const std::uint8_t>(&reply_opcode, 1));
    });
}

void register_observed_state6_byte_block_handler(
    MessageDispatcher& dispatcher,
    State6ByteBlockHandler consume_byte_block) {
    if (!consume_byte_block) {
        throw std::invalid_argument("byte block consumer must be callable");
    }
    dispatcher.register_handler(DispatchPhase::state6, 0x0B,
        [consumer = std::move(consume_byte_block)](PacketReader& reader) {
        std::array<std::uint8_t, 32> block{};
        for (auto& byte : block) {
            byte = reader.read_u8();
        }
        consumer(block);
    });
}

void register_observed_state6_game_start_handler(
    MessageDispatcher& dispatcher,
    State6GameStartHandler consume_game_start) {
    if (!consume_game_start) {
        throw std::invalid_argument("game-start consumer must be callable");
    }
    dispatcher.register_handler(DispatchPhase::state6, 0x0A,
        [consumer = std::move(consume_game_start)](PacketReader& reader) {
            const State6GameStart start{
                reader.read_u32(), reader.read_u16(), reader.read_u8()};
            consumer(start);
        });
}

void register_observed_state6_notice_handlers(
    MessageDispatcher& dispatcher, State6NoticeHandler present_notice) {
    if (!present_notice) throw std::invalid_argument("notice consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x14,
        [consumer = present_notice](PacketReader& reader) {
            consumer(State6Notice{0x14, "Sorry", reader.read_string(), true});
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x15,
        [consumer = std::move(present_notice)](PacketReader& reader) {
            consumer(State6Notice{0x15, "For Your Information", reader.read_string(), false});
        });
}

void register_observed_state6_opcode_16_handler(
    MessageDispatcher& dispatcher, State6Opcode16Handler consume_message) {
    if (!consume_message) throw std::invalid_argument("opcode 0x16 consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x16,
        [consumer = std::move(consume_message)](PacketReader& reader) {
            State6Opcode16Message message;
            message.text = reader.read_string();
            message.value = reader.read_u8();
            consumer(message);
        });
}

void register_observed_state6_remaining_record_handlers(
    MessageDispatcher& dispatcher, State6RecoveredRecordHandler consume_record) {
    if (!consume_record) throw std::invalid_argument("recovered-record consumer must be callable");
    const auto register_empty = [&dispatcher, &consume_record](std::uint8_t opcode) {
        dispatcher.register_handler(DispatchPhase::state6, opcode,
            [consumer = consume_record, opcode](PacketReader&) {
                consumer(State6RecoveredRecord{opcode, {}, {}});
            });
    };
    register_empty(0x28);
    dispatcher.register_handler(DispatchPhase::state6, 0xC8,
        [consumer = consume_record](PacketReader& reader) {
            State6RecoveredRecord record{0xC8, {}, {}};
            record.values.reserve(6);
            record.values.push_back(reader.read_u16());
            for (int i = 0; i < 5; ++i) record.values.push_back(reader.read_u8());
            const auto count = reader.read_u8();
            if (count == 0 || count > 15)
                throw std::runtime_error("state-6 opcode 0xC8 outfit count is outside observed range 1..15");
            record.values.push_back(count);
            record.strings.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                record.values.push_back(reader.read_u16());
                auto name = reader.read_string();
                if (name.size() > 29) name.resize(29);
                record.strings.push_back(std::move(name));
                record.values.push_back(reader.read_u8());
            }
            consumer(record);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xD2,
        [consumer = consume_record](PacketReader& reader) {
            State6RecoveredRecord record{0xD2, {}, {}};
            record.values.push_back(reader.read_u32());
            auto name = reader.read_string();
            if (name.size() > 29) name.resize(29);
            record.strings.push_back(std::move(name));
            record.values.push_back(reader.read_u8());
            consumer(record);
        });
    for (const auto opcode : {std::uint8_t{0xD3}, std::uint8_t{0xD4}}) {
        dispatcher.register_handler(DispatchPhase::state6, opcode,
            [consumer = consume_record, opcode](PacketReader& reader) {
                State6RecoveredRecord record{opcode, {reader.read_u32()}, {}};
                consumer(record);
            });
    }
    dispatcher.register_handler(DispatchPhase::state6, 0xF0,
        [consumer = consume_record](PacketReader& reader) {
            State6RecoveredRecord record{0xF0, {}, {}};
            const auto count = reader.read_u16();
            record.values.push_back(count);
            record.values.reserve(static_cast<std::size_t>(count) * 2U);
            record.strings.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                record.values.push_back(reader.read_u16());
                auto name = reader.read_string();
                if (name.size() > 49) name.resize(49);
                record.strings.push_back(std::move(name));
                record.values.push_back(reader.read_u8());
            }
            consumer(record);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xF1,
        [consumer = std::move(consume_record)](PacketReader& reader) {
            State6RecoveredRecord record{0xF1, {}, {}};
            record.values.push_back(reader.read_u16());
            const auto count = reader.read_u8();
            record.values.push_back(count);
            record.strings.reserve(static_cast<std::size_t>(count) * 2U);
            for (std::uint32_t i = 0; i < count; ++i) {
                auto name = reader.read_string();
                if (name.size() > 49) name.resize(49);
                record.strings.push_back(std::move(name));
                auto text = reader.read_string();
                if (text.size() > 199) text.resize(199);
                record.strings.push_back(std::move(text));
            }
            consumer(record);
        });
}

void register_observed_state6_floor_transition_handlers(
    MessageDispatcher& dispatcher, State6FloorTransitionHandler consume_transition) {
    if (!consume_transition)
        throw std::invalid_argument("floor-transition consumer must be callable");
    for (const auto [opcode, direction] : {
            std::pair{std::uint8_t{0xBE}, State6FloorDirection::up},
            std::pair{std::uint8_t{0xBF}, State6FloorDirection::down}}) {
        dispatcher.register_handler(DispatchPhase::state6, opcode,
            [consumer = consume_transition, direction](PacketReader& reader) {
                const auto bytes = reader.remaining_bytes();
                const auto consumed = consumer(State6FloorTransitionPacket{direction,
                    std::vector<std::uint8_t>(bytes.begin(), bytes.end())});
                if (consumed > reader.remaining())
                    throw std::runtime_error("floor-transition handler consumed beyond its message payload");
                (void)reader.read_bytes(consumed);
            });
    }
}

void register_observed_state6_map_description_handler(
    MessageDispatcher& dispatcher,
    State6MapDescriptionHandler consume_map_description) {
    if (!consume_map_description) {
        throw std::invalid_argument("map-description consumer must be callable");
    }
    dispatcher.register_handler(DispatchPhase::state6, 0x64,
        [consumer = std::move(consume_map_description)](PacketReader& reader) {
            State6MapDescription description;
            description.center_x = reader.read_u16();
            description.center_y = reader.read_u16();
            description.floor = reader.read_u8();
            const auto remaining = reader.remaining_bytes();
            description.tile_stream.assign(remaining.begin(), remaining.end());
            const auto consumed = consumer(description);
            if (consumed > reader.remaining())
                throw std::runtime_error("state-6 map handler consumed beyond its message payload");
            (void)reader.read_bytes(consumed);
        });
}

State6MapStrip decode_state6_map_strip(State6MapDirection direction,
    std::uint8_t center_floor, std::span<const std::uint8_t> stream,
    const std::vector<std::uint16_t>& extra_ids,
    const State6DecodedMap* current_map) {
    const auto [first_floor, end_floor] = map_floor_range(center_floor);
    const auto step = center_floor < 8 ? 1 : -1;
    const std::size_t cells_per_floor = (direction == State6MapDirection::north ||
        direction == State6MapDirection::south) ? 18U : 14U;
    PacketReader reader(stream);
    State6MapStrip strip{direction, center_floor, {},
        std::vector<std::uint8_t>(stream.begin(), stream.end())};
    std::unordered_map<std::uint32_t, std::string> creatures;
    seed_known_creatures(creatures, current_map);
    std::uint16_t skipped = 0;
    for (int floor = first_floor; floor != end_floor; floor += step) {
        for (std::size_t i = 0; i < cells_per_floor; ++i) {
            std::uint8_t x{}, y{};
            switch (direction) {
            case State6MapDirection::north: x = static_cast<std::uint8_t>(i); y = 0; break;
            case State6MapDirection::east: x = 17; y = static_cast<std::uint8_t>(i); break;
            case State6MapDirection::south: x = static_cast<std::uint8_t>(i); y = 13; break;
            case State6MapDirection::west: x = 0; y = static_cast<std::uint8_t>(i); break;
            }
            auto tile = read_map_tile(reader, x, y,
                static_cast<std::uint8_t>(floor), extra_ids, creatures, skipped);
            strip.tiles.push_back(std::move(tile));
        }
    }
    strip.wire_bytes_consumed = reader.position();
    return strip;
}

State6FloorTransitionResult apply_state6_floor_transition(State6DecodedMap& map,
    const State6FloorTransitionPacket& transition,
    const std::vector<std::uint16_t>& extra_ids,
    const resources::TibiaDatFile& dat) {
    if (map.center_floor > 15)
        throw std::runtime_error("state-6 floor transition starts outside the observed 0..15 range");

    const auto old_floor = static_cast<int>(map.center_floor);
    const bool moving_up = transition.direction == State6FloorDirection::up;
    const auto new_floor = old_floor + (moving_up ? -1 : 1);
    if (new_floor < 0 || new_floor > 15)
        throw std::runtime_error("state-6 floor transition exceeds the observed 0..15 range");

    State6FloorTransitionResult result;
    const auto shift_position = [moving_up](std::uint16_t value) {
        const auto shifted = static_cast<int>(value) + (moving_up ? 1 : -1);
        return static_cast<std::uint16_t>(std::clamp(shifted, 0, 65535));
    };
    map.center_x = shift_position(map.center_x);
    map.center_y = shift_position(map.center_y);
    map.center_floor = static_cast<std::uint8_t>(new_floor);

    if (moving_up && new_floor == 7) {
        result.floor_cache_shift = -3;
        for (std::uint8_t floor = 2; floor < 8; ++floor)
            result.refreshed_floors.push_back(floor);
    } else if (moving_up && new_floor >= 8) {
        result.floor_cache_shift = -1;
        result.refreshed_floors.push_back(4);
    } else if (!moving_up && new_floor == 8) {
        result.floor_cache_shift = 3;
        result.refreshed_floors = {2, 1, 0};
    } else if (!moving_up && new_floor >= 9) {
        result.floor_cache_shift = 1;
        if (new_floor < 14) result.refreshed_floors.push_back(0);
    }

    PacketReader reader(transition.raw_stream);
    std::unordered_map<std::uint32_t, std::string> known_creatures;
    seed_known_creatures(known_creatures, &map);
    std::uint16_t skipped_tiles = 0;
    for (const auto floor : result.refreshed_floors) {
        for (std::uint16_t x = 0; x < 18; ++x) {
            for (std::uint16_t y = 0; y < 14; ++y) {
                auto tile = read_map_tile(reader, x, y, floor, extra_ids,
                    known_creatures, skipped_tiles);
                auto projected = project_map_viewport_tile(map.center_x, map.center_y,
                    map.center_floor, x, y, floor);
                projected.objects = std::move(tile.objects);
                apply_state6_map_tile_update(map,
                    State6MapTileUpdate{projected.x, projected.y, projected.floor,
                        std::move(projected)}, dat);
                ++result.decoded_tiles;
            }
        }
    }
    result.wire_bytes_consumed = reader.position();

    result.center_x = map.center_x;
    result.center_y = map.center_y;
    result.center_floor = map.center_floor;
    return result;
}

void apply_state6_map_strip(State6DecodedMap& map, const State6MapStrip& strip,
    const resources::TibiaDatFile& dat) {
    switch (strip.direction) {
    case State6MapDirection::north: if (map.center_y != 0) --map.center_y; break;
    case State6MapDirection::east: if (map.center_x != 0xffff) ++map.center_x; break;
    case State6MapDirection::south: if (map.center_y != 0xffff) ++map.center_y; break;
    case State6MapDirection::west: if (map.center_x != 0) --map.center_x; break;
    }
    std::erase_if(map.tiles, [&map](const State6MapTile& tile) {
        return !tibia::map::viewport_map_position(map, tile).has_value();
    });
    for (auto tile : strip.tiles) {
        auto projected = project_map_viewport_tile(map.center_x, map.center_y,
            map.center_floor, tile.x, tile.y, tile.floor);
        projected.objects = std::move(tile.objects);
        apply_state6_map_tile_update(map,
            State6MapTileUpdate{projected.x, projected.y, projected.floor,
                std::move(projected)}, dat);
    }
}

void apply_state6_map_tile_update(State6DecodedMap& map,
    const State6MapTileUpdate& update, const resources::TibiaDatFile& dat) {
    auto replacement = update.tile;
    replacement.x = update.x;
    replacement.y = update.y;
    replacement.floor = update.floor;
    (void)tibia::map::refresh_native_tile_object_context(replacement, dat);
    const auto it = std::find_if(map.tiles.begin(), map.tiles.end(), [&](const State6MapTile& tile) {
        return tile.x == update.x && tile.y == update.y && tile.floor == update.floor;
    });
    if (it == map.tiles.end()) map.tiles.push_back(std::move(replacement));
    else *it = std::move(replacement);
}

void apply_state6_map_thing_add(State6DecodedMap& map,
    const resources::TibiaDatFile& dat, const State6MapThingAdd& add) {
    const auto it = std::find_if(map.tiles.begin(), map.tiles.end(), [&](const State6MapTile& tile) {
        return tile.x == add.x && tile.y == add.y && tile.floor == add.floor;
    });
    if (it == map.tiles.end()) {
        map.tiles.push_back(State6MapTile{add.x, add.y, add.floor, {add.object}});
    } else insert_state6_map_object(it->objects, add.object, dat);
    const auto tile = std::find_if(map.tiles.begin(), map.tiles.end(), [&add](const auto& value) {
        return value.x == add.x && value.y == add.y && value.floor == add.floor;
    });
    if (tile != map.tiles.end())
        (void)tibia::map::refresh_native_tile_object_context(*tile, dat);
}

void apply_state6_map_thing_transform(State6DecodedMap& map,
    const State6MapThingTransform& transform, const resources::TibiaDatFile& dat) {
    const auto tile = std::find_if(map.tiles.begin(), map.tiles.end(), [&](const State6MapTile& value) {
        return value.x == transform.x && value.y == transform.y && value.floor == transform.floor;
    });
    if (tile != map.tiles.end() && transform.stack_index < tile->objects.size()) {
        tile->objects[transform.stack_index] = transform.object;
        (void)tibia::map::refresh_native_tile_object_context(*tile, dat);
    }
}

void apply_state6_map_thing_remove(State6DecodedMap& map,
    const State6MapThingRemove& remove, const resources::TibiaDatFile& dat) {
    const auto tile = std::find_if(map.tiles.begin(), map.tiles.end(), [&](const State6MapTile& value) {
        return value.x == remove.x && value.y == remove.y && value.floor == remove.floor;
    });
    if (tile != map.tiles.end() && remove.stack_index < tile->objects.size()) {
        tile->objects.erase(tile->objects.begin() + remove.stack_index);
        (void)tibia::map::refresh_native_tile_object_context(*tile, dat);
    }
}

void register_observed_state6_map_strip_handlers(
    MessageDispatcher& dispatcher, State6MapStripHandler consume_strip) {
    if (!consume_strip) throw std::invalid_argument("map-strip consumer must be callable");
    constexpr std::array<std::pair<std::uint8_t, State6MapDirection>, 4> routes{{
        {0x65, State6MapDirection::north}, {0x66, State6MapDirection::east},
        {0x67, State6MapDirection::south}, {0x68, State6MapDirection::west}}};
    for (const auto [opcode, direction] : routes) {
        dispatcher.register_handler(DispatchPhase::state6, opcode,
            [consumer = consume_strip, direction](PacketReader& reader) {
                const auto remaining = reader.remaining_bytes();
                std::vector<std::uint8_t> bytes;
                bytes.assign(remaining.begin(), remaining.end());
                const auto consumed = consumer(State6MapStrip{direction, 0, {},
                    std::move(bytes), 0});
                if (consumed > reader.remaining())
                    throw std::runtime_error("map-strip handler consumed beyond its message payload");
                (void)reader.read_bytes(consumed);
            });
    }
}

void register_observed_state6_map_tile_update_handler(
    MessageDispatcher& dispatcher, State6MapTileUpdateHandler consume_update,
    const std::vector<std::uint16_t>& extra_ids, State6MapContextProvider current_map) {
    if (!consume_update) throw std::invalid_argument("map tile-update consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x69,
        [consumer = std::move(consume_update), extra_ids,
            current_map = std::move(current_map)](PacketReader& reader) {
            State6MapTileUpdate update;
            update.x = reader.read_u16();
            update.y = reader.read_u16();
            update.floor = reader.read_u8();
            std::unordered_map<std::uint32_t, std::string> creatures;
            seed_known_creatures(creatures, current_map ? current_map() : nullptr);
            std::uint16_t skipped = 0;
            update.tile = read_map_tile(reader, 0, 0, update.floor, extra_ids, creatures, skipped);
            update.tile.x = update.x;
            update.tile.y = update.y;
            consumer(update);
        });
}

void register_observed_state6_map_thing_add_handler(
    MessageDispatcher& dispatcher, State6MapThingAddHandler consume_add,
    const std::vector<std::uint16_t>& extra_ids, State6MapContextProvider current_map) {
    if (!consume_add) throw std::invalid_argument("map thing-add consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x6A,
        [consumer = std::move(consume_add), extra_ids,
            current_map = std::move(current_map)](PacketReader& reader) {
            State6MapThingAdd add;
            add.x = reader.read_u16();
            add.y = reader.read_u16();
            add.floor = reader.read_u8();
            std::unordered_map<std::uint32_t, std::string> creatures;
            seed_known_creatures(creatures, current_map ? current_map() : nullptr);
            std::uint16_t skipped = 0;
            const auto tile = read_map_tile(reader, add.x, add.y, add.floor,
                extra_ids, creatures, skipped, true);
            if (tile.objects.size() != 1)
                throw std::runtime_error("state-6 thing-add did not contain exactly one object");
            add.object = tile.objects.front();
            consumer(add);
        });
}

void register_observed_state6_map_thing_transform_handler(
    MessageDispatcher& dispatcher, State6MapThingTransformHandler consume_transform,
    const std::vector<std::uint16_t>& extra_ids, State6MapContextProvider current_map) {
    if (!consume_transform) throw std::invalid_argument("map thing-transform consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x6B,
        [consumer = std::move(consume_transform), extra_ids,
            current_map = std::move(current_map)](PacketReader& reader) {
            State6MapThingTransform transform;
            transform.x = reader.read_u16();
            transform.y = reader.read_u16();
            transform.floor = reader.read_u8();
            transform.stack_index = reader.read_u8();
            std::unordered_map<std::uint32_t, std::string> creatures;
            seed_known_creatures(creatures, current_map ? current_map() : nullptr);
            std::uint16_t skipped = 0;
            const auto tile = read_map_tile(reader, transform.x, transform.y, transform.floor,
                extra_ids, creatures, skipped, true);
            if (tile.objects.size() != 1)
                throw std::runtime_error("state-6 thing-transform did not contain exactly one replacement object");
            transform.object = tile.objects.front();
            consumer(transform);
        });
}

void register_observed_state6_map_thing_remove_handler(
    MessageDispatcher& dispatcher, State6MapThingRemoveHandler consume_remove) {
    if (!consume_remove) throw std::invalid_argument("map thing-remove consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x6C,
        [consumer = std::move(consume_remove)](PacketReader& reader) {
            const State6MapThingRemove remove{
                reader.read_u16(), reader.read_u16(), reader.read_u8(), reader.read_u8()};
            consumer(remove);
        });
}

void apply_state6_container_open(std::vector<State6Container>& containers,
    const State6Container& container) {
    if (container.id >= 16)
        throw std::runtime_error("state-6 container ID is outside the observed 0..15 range");
    auto observed = container;
    if (observed.items.size() > 36) observed.items.resize(36);
    const auto it = std::find_if(containers.begin(), containers.end(),
        [&](const State6Container& current) { return current.id == container.id; });
    if (it != containers.end())
        throw std::runtime_error("state-6 container-open received an already-open container ID");
    containers.push_back(std::move(observed));
}

void apply_state6_container_close(std::vector<State6Container>& containers, std::uint8_t id) {
    if (id >= 16)
        throw std::runtime_error("state-6 container ID is outside the observed 0..15 range");
    const auto it = std::find_if(containers.begin(), containers.end(),
        [=](const State6Container& container) { return container.id == id; });
    if (it == containers.end())
        throw std::runtime_error("state-6 container-close references a container that is not open");
    containers.erase(it);
}

void apply_state6_container_change(std::vector<State6Container>& containers,
    const State6ContainerChange& change) {
    const auto it = std::find_if(containers.begin(), containers.end(),
        [&](const State6Container& current) { return current.id == change.container_id; });
    if (change.container_id >= 16)
        throw std::runtime_error("state-6 container ID is outside the observed 0..15 range");
    if (it == containers.end())
        throw std::runtime_error("state-6 container change references a container that is not open");
    switch (change.kind) {
    case State6ContainerChangeKind::add:
        if (it->items.size() > 35) it->items.pop_back();
        it->items.insert(it->items.begin(), change.item);
        break;
    case State6ContainerChangeKind::update:
        if (change.slot < it->items.size()) it->items[change.slot] = change.item;
        break;
    case State6ContainerChangeKind::remove:
        if (change.slot < it->items.size())
            it->items.erase(it->items.begin() + change.slot);
        break;
    }
}

void apply_state6_thing_move(State6DecodedMap& map,
    const resources::TibiaDatFile& dat, const State6ThingMove& move) {
    const auto source = std::find_if(map.tiles.begin(), map.tiles.end(), [&](const State6MapTile& tile) {
        return tile.x == move.old_x && tile.y == move.old_y && tile.floor == move.old_floor;
    });
    if (source == map.tiles.end() || move.stack_index >= source->objects.size()) return;
    const auto object = source->objects[move.stack_index];
    source->objects.erase(source->objects.begin() + move.stack_index);
    (void)tibia::map::refresh_native_tile_object_context(*source, dat);
    const auto destination = std::find_if(map.tiles.begin(), map.tiles.end(), [&](const State6MapTile& tile) {
        return tile.x == move.new_x && tile.y == move.new_y && tile.floor == move.new_floor;
    });
    if (destination == map.tiles.end()) {
        map.tiles.push_back(State6MapTile{move.new_x, move.new_y, move.new_floor, {object}});
    } else insert_state6_map_object(destination->objects, object, dat);
    const auto updated_destination = std::find_if(map.tiles.begin(), map.tiles.end(), [&move](const auto& tile) {
        return tile.x == move.new_x && tile.y == move.new_y && tile.floor == move.new_floor;
    });
    if (updated_destination != map.tiles.end())
        (void)tibia::map::refresh_native_tile_object_context(*updated_destination, dat);
}

bool apply_observed_state6_thing_move(State6DecodedMap& map,
    State6CreatureStateStore& creatures, State6LocalPlayerMovementGlobals& movement,
    const resources::TibiaDatFile& dat, std::uint32_t first_timer_sample,
    std::uint32_t second_timer_sample, const State6ThingMove& move) {
    const auto source = std::find_if(map.tiles.begin(), map.tiles.end(), [&move](const auto& tile) {
        return tile.x == move.old_x && tile.y == move.old_y && tile.floor == move.old_floor;
    });
    const bool has_moved_object = source != map.tiles.end() &&
        move.stack_index < source->objects.size();
    const auto moved_creature_id = has_moved_object
        ? source->objects[move.stack_index].creature_id : 0;
    const bool moves_local_player = moved_creature_id != 0 &&
        moved_creature_id == map.local_player_id;
    const auto delta_x = static_cast<std::int32_t>(move.new_x) - move.old_x;
    const auto delta_y = static_cast<std::int32_t>(move.new_y) - move.old_y;
    const auto delta_floor = static_cast<std::int32_t>(move.new_floor) - move.old_floor;
    const bool short_same_floor_move = delta_floor == 0 &&
        std::abs(delta_x) < 2 && std::abs(delta_y) < 2;
    if (moves_local_player && short_same_floor_move) {
        (void)apply_observed_state6_local_player_reset(movement, creatures,
            map.local_player_id);
    } else if (moved_creature_id != 0 && movement.at_0x00605A24 != 0) {
        const auto* state = creatures.find(moved_creature_id);
        if (state == nullptr) {
            apply_state6_thing_move(map, dat, move);
            return moves_local_player;
        }
        auto duration_parameter = 0;
        const auto destination = std::find_if(map.tiles.begin(), map.tiles.end(),
            [&move](const auto& tile) {
                return tile.x == move.new_x && tile.y == move.new_y &&
                    tile.floor == move.new_floor;
            });
        if (!short_same_floor_move) {
            if (destination == map.tiles.end() || destination->objects.empty()) {
                apply_state6_thing_move(map, dat, move);
                return moves_local_player;
            }
            const auto* ground = dat.find_item(destination->objects.front().type);
            const auto* speed = ground == nullptr ? nullptr : ground->find_attribute(0x00);
            if (speed == nullptr || speed->payload_words.empty()) {
                apply_state6_thing_move(map, dat, move);
                return moves_local_player;
            }
            duration_parameter = speed->payload_words.front();
        }
        const auto speed_offset = static_cast<std::size_t>(0x8c -
            State6CreatureRuntimeState::first_native_offset);
        const auto creature_speed = static_cast<std::uint32_t>(
            state->native_fields[speed_offset]) |
            (static_cast<std::uint32_t>(state->native_fields[speed_offset + 1]) << 8U) |
            (static_cast<std::uint32_t>(state->native_fields[speed_offset + 2]) << 16U) |
            (static_cast<std::uint32_t>(state->native_fields[speed_offset + 3]) << 24U);
        const auto direction_offset = static_cast<std::size_t>(0x50 -
            State6CreatureRuntimeState::first_native_offset);
        const auto effect = make_observed_state6_creature_move_state(movement,
            State6CreatureMoveStateInput{
                short_same_floor_move ? 0 : delta_x,
                short_same_floor_move ? 0 : delta_y,
                duration_parameter, static_cast<std::int32_t>(creature_speed),
                static_cast<std::int32_t>(movement.at_0x00605A24), moved_creature_id,
                map.local_player_id, first_timer_sample, second_timer_sample,
                state->native_fields[direction_offset]});
        if (effect) (void)creatures.apply_move_state(moved_creature_id, *effect);
    }
    apply_state6_thing_move(map, dat, move);
    return moves_local_player;
}

void register_observed_state6_thing_move_handler(
    MessageDispatcher& dispatcher, State6ThingMoveHandler consume_move) {
    if (!consume_move) throw std::invalid_argument("thing-move consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x6D,
        [consumer = std::move(consume_move)](PacketReader& reader) {
            const State6ThingMove move{
                reader.read_u16(), reader.read_u16(), reader.read_u8(), reader.read_u8(),
                reader.read_u16(), reader.read_u16(), reader.read_u8()};
            consumer(move);
        });
}

void register_observed_state6_container_handlers(
    MessageDispatcher& dispatcher, State6ContainerOpenHandler consume_open,
    State6ContainerCloseHandler consume_close, State6ContainerChangeHandler consume_change,
    const std::vector<std::uint16_t>& extra_ids) {
    if (!consume_open || !consume_close || !consume_change)
        throw std::invalid_argument("container handlers must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x6E,
        [consumer = std::move(consume_open), extra_ids](PacketReader& reader) {
            State6Container container;
            container.id = reader.read_u8();
            container.container_item = read_container_item(reader, extra_ids);
            container.name = reader.read_string();
            if (container.name.size() > 29) container.name.resize(29);
            container.capacity = reader.read_u8();
            container.has_parent = reader.read_u8() != 0;
            const auto count = reader.read_u8();
            container.items.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i)
                container.items.push_back(read_container_item(reader, extra_ids));
            consumer(container);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x6F,
        [consumer = std::move(consume_close)](PacketReader& reader) {
            consumer(reader.read_u8());
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x70,
        [consumer = consume_change, extra_ids](PacketReader& reader) {
            State6ContainerChange change;
            change.kind = State6ContainerChangeKind::add;
            change.container_id = reader.read_u8();
            change.item = read_container_item(reader, extra_ids);
            consumer(change);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x71,
        [consumer = consume_change, extra_ids](PacketReader& reader) {
            State6ContainerChange change;
            change.kind = State6ContainerChangeKind::update;
            change.container_id = reader.read_u8();
            change.slot = reader.read_u8();
            change.item = read_container_item(reader, extra_ids);
            consumer(change);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x72,
        [consumer = std::move(consume_change)](PacketReader& reader) {
            State6ContainerChange change;
            change.kind = State6ContainerChangeKind::remove;
            change.container_id = reader.read_u8();
            change.slot = reader.read_u8();
            consumer(change);
        });
}

void register_observed_state6_shop_handlers(
    MessageDispatcher& dispatcher, State6ShopUpdateHandler consume_update,
    const std::vector<std::uint16_t>& extra_ids) {
    if (!consume_update) throw std::invalid_argument("shop-update consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x7D,
        [consumer = consume_update, extra_ids](PacketReader& reader) {
            State6ShopUpdate update;
            update.kind = State6ShopUpdateKind::open;
            update.title = reader.read_string();
            if (update.title.size() > 29) update.title.resize(29);
            const auto count = reader.read_u8();
            if (count > 100)
                throw std::runtime_error("state-6 own-offer count exceeds the observed 100-entry limit");
            update.items.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i)
                update.items.push_back(read_container_item(reader, extra_ids));
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x7E,
        [consumer = consume_update, extra_ids](PacketReader& reader) {
            State6ShopUpdate update;
            update.kind = State6ShopUpdateKind::goods;
            update.title = reader.read_string();
            if (update.title.size() > 29) update.title.resize(29);
            const auto count = reader.read_u8();
            if (count > 100)
                throw std::runtime_error("state-6 counter-offer count exceeds the observed 100-entry limit");
            update.items.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i)
                update.items.push_back(read_container_item(reader, extra_ids));
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x7F,
        [consumer = std::move(consume_update)](PacketReader&) {
            consumer(State6ShopUpdate{State6ShopUpdateKind::close, {}, {}});
        });
}

bool State6ShopState::apply(const State6ShopUpdate& update) {
    switch (update.kind) {
    case State6ShopUpdateKind::open:
        if (!own_offers_.empty())
            throw std::runtime_error("state-6 shop-open arrived while native OwnOffer is nonempty");
        own_offer_name_ = update.title;
        own_offers_ = update.items;
        return true;
    case State6ShopUpdateKind::goods:
        if (!counter_offers_.empty()) return false;
        counter_offer_name_ = update.title;
        counter_offers_ = update.items;
        return true;
    case State6ShopUpdateKind::close:
        own_offers_.clear();
        counter_offers_.clear();
        return true;
    }
    return false;
}

const std::string& State6ShopState::own_offer_name() const noexcept { return own_offer_name_; }
const std::vector<State6ContainerItem>& State6ShopState::own_offers() const noexcept {
    return own_offers_;
}
const std::string& State6ShopState::counter_offer_name() const noexcept {
    return counter_offer_name_;
}
const std::vector<State6ContainerItem>& State6ShopState::counter_offers() const noexcept {
    return counter_offers_;
}

void apply_state6_inventory_update(std::vector<State6InventorySlot>& inventory,
    const State6InventoryUpdate& update) {
    if (update.slot == 0 || update.slot > 10)
        throw std::runtime_error("state-6 inventory slot is outside the observed 1..10 range");
    const auto it = std::find_if(inventory.begin(), inventory.end(),
        [&](const State6InventorySlot& current) { return current.slot == update.slot; });
    if (update.remove) {
        if (it != inventory.end()) inventory.erase(it);
    } else if (it == inventory.end()) {
        inventory.push_back(State6InventorySlot{update.slot, update.item});
    } else {
        it->item = update.item;
    }
}

void register_observed_state6_inventory_handlers(
    MessageDispatcher& dispatcher, State6InventoryUpdateHandler consume_update,
    const std::vector<std::uint16_t>& extra_ids) {
    if (!consume_update) throw std::invalid_argument("inventory-update consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x78,
        [consumer = consume_update, extra_ids](PacketReader& reader) {
            State6InventoryUpdate update;
            update.slot = reader.read_u8();
            update.item = read_container_item(reader, extra_ids);
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x79,
        [consumer = std::move(consume_update)](PacketReader& reader) {
            State6InventoryUpdate update;
            update.slot = reader.read_u8();
            update.remove = true;
            consumer(update);
        });
}

void register_observed_state6_opcode_82_handler(
    MessageDispatcher& dispatcher, State6Opcode82Handler consume_values) {
    if (!consume_values) throw std::invalid_argument("opcode-0x82 consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x82,
        [consumer = std::move(consume_values)](PacketReader& reader) {
            const State6Opcode82Values values{reader.read_u8(), reader.read_u8()};
            consumer(values);
        });
}

std::int32_t apply_observed_state6_opcode_82(
    State6Opcode82State& state, const State6Opcode82Values& values) {
    const bool was_uninitialized = state.global_610c00 == -1;
    const auto palette_index = static_cast<std::int32_t>(values.value_1);
    state.global_755de8 = (palette_index / 36) * 0x33;
    state.global_610c00 = values.value_0;
    state.global_755df0 = (palette_index % 6) * 0x33;
    state.global_755dec = ((palette_index / 6) % 6) * 0x33;
    if (was_uninitialized) {
        state.global_610bfc = values.value_0;
        state.global_755df4 = state.global_755de8;
        state.global_755df8 = state.global_755dec;
        state.global_755dfc = state.global_755df0;
    }
    return palette_index / 36;
}

void advance_observed_state6_opcode_82(State6Opcode82State& state) noexcept {
    const auto advance = [](std::int32_t& current, std::int32_t target) {
        if (current < target) ++current;
        else if (current > target) --current;
    };
    advance(state.global_610bfc, state.global_610c00);
    advance(state.global_755df4, state.global_755de8);
    advance(state.global_755df8, state.global_755dec);
    advance(state.global_755dfc, state.global_755df0);
}

State6MapLocalPosition transform_state6_wire_position(
    const State6DecodedMap& map, const State6WirePosition& position) {
    const auto floor_delta = static_cast<std::int32_t>(map.center_floor) -
        static_cast<std::int32_t>(position.floor);
    const auto x = static_cast<std::int32_t>(position.x) + 8 -
        static_cast<std::int32_t>(map.center_x) - floor_delta;
    const auto y = static_cast<std::int32_t>(position.y) + 6 -
        static_cast<std::int32_t>(map.center_y) - floor_delta;
    const auto local_floor = map.center_floor < 8
        ? 7 - static_cast<std::int32_t>(map.center_floor) + floor_delta
        : floor_delta + 2;
    return {x, y, local_floor};
}

bool State6BuddyStore::apply(const State6RecoveredRecord& record) {
    if (record.opcode == 0xD2) {
        if (record.values.size() != 2 || record.strings.size() != 1)
            throw std::invalid_argument("state-6 opcode 0xD2 record has an invalid shape");
        const auto id = record.values[0];
        auto it = std::find_if(entries_.begin(), entries_.end(),
            [id](const State6BuddyEntry& entry) { return entry.id == id; });
        if (it == entries_.end()) {
            if (entries_.size() >= capacity)
                throw std::runtime_error("state-6 buddy count exceeds the observed native limit of 200");
            entries_.push_back(State6BuddyEntry{id, record.strings.front(), false});
            it = std::prev(entries_.end());
        } else {
            it->name = record.strings.front();
        }
        const bool online = record.values[1] == 1;
        if (it->online != online) {
            if (online) ++online_count_;
            else --online_count_;
            it->online = online;
        }
        return true;
    }
    if (record.opcode == 0xD3 || record.opcode == 0xD4) {
        if (record.values.size() != 1 || !record.strings.empty())
            throw std::invalid_argument("state-6 buddy status record has an invalid shape");
        const auto* current = find(record.values.front());
        if (current == nullptr) return false;
        const bool online = record.opcode == 0xD3;
        const auto index = static_cast<std::size_t>(current - entries_.data());
        if (entries_[index].online != online) {
            if (online) ++online_count_;
            else --online_count_;
            entries_[index].online = online;
        }
        return true;
    }
    return false;
}

const State6BuddyEntry* State6BuddyStore::find(std::uint32_t id) const noexcept {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
        [id](const State6BuddyEntry& entry) { return entry.id == id; });
    return it == entries_.end() ? nullptr : &*it;
}

const std::vector<State6BuddyEntry>& State6BuddyStore::entries() const noexcept {
    return entries_;
}

std::size_t State6BuddyStore::online_count() const noexcept { return online_count_; }

bool State6QuestState::apply(const State6RecoveredRecord& record) {
    if (record.opcode == 0xF0) {
        if (record.values.empty() || record.values.size() !=
                1U + static_cast<std::size_t>(record.values.front()) * 2U ||
            record.strings.size() != record.values.front())
            throw std::invalid_argument("state-6 opcode 0xF0 record has an invalid shape");
        std::size_t value_index = 1;
        for (std::size_t i = 0; i < record.strings.size(); ++i) {
            const auto id = static_cast<std::uint16_t>(record.values[value_index++]);
            const bool completed = record.values[value_index++] != 0;
            auto line_it = std::find_if(lines_.begin(), lines_.end(),
                [id](const State6QuestLine& line) { return line.id == id; });
            if (line_it == lines_.end()) {
                lines_.push_back(State6QuestLine{.id = id});
                line_it = std::prev(lines_.end());
            }
            auto& line = *line_it;
            line.id = id;
            line.name = record.strings[i];
            line.completed = completed;
        }
        quest_lines_changed_ = true;
        return true;
    }
    if (record.opcode == 0xF1) {
        if (record.values.size() != 2 || record.strings.size() !=
                static_cast<std::size_t>(record.values[1]) * 2U)
            throw std::invalid_argument("state-6 opcode 0xF1 record has an invalid shape");
        const auto id = static_cast<std::uint16_t>(record.values[0]);
        auto line_it = std::find_if(lines_.begin(), lines_.end(),
            [id](const State6QuestLine& line) { return line.id == id; });
        if (line_it == lines_.end()) {
            lines_.push_back(State6QuestLine{.id = id});
            line_it = std::prev(lines_.end());
        }
        auto& line = *line_it;
        line.id = id;
        line.flags.clear();
        line.flags.reserve(record.values[1]);
        for (std::size_t i = 0; i < record.strings.size(); i += 2)
            line.flags.push_back(State6QuestFlag{record.strings[i], record.strings[i + 1]});
        quest_flags_changed_ = true;
        ++quest_flags_revision_;
        last_quest_flags_id_ = id;
        return true;
    }
    return false;
}

const State6QuestLine* State6QuestState::find(std::uint16_t id) const noexcept {
    const auto it = std::find_if(lines_.begin(), lines_.end(),
        [id](const State6QuestLine& line) { return line.id == id; });
    return it == lines_.end() ? nullptr : &*it;
}

const std::vector<State6QuestLine>& State6QuestState::lines() const noexcept {
    return lines_;
}

bool State6QuestState::quest_lines_changed() const noexcept { return quest_lines_changed_; }
bool State6QuestState::quest_flags_changed() const noexcept { return quest_flags_changed_; }
std::uint64_t State6QuestState::quest_flags_revision() const noexcept {
    return quest_flags_revision_;
}
std::optional<std::uint16_t> State6QuestState::last_quest_flags_id() const noexcept {
    return last_quest_flags_id_;
}

std::optional<std::size_t> State6VisualEffectPool::reserve(std::uint8_t opcode) {
    const auto slot = std::find_if(slots_.begin(), slots_.end(),
        [](const State6VisualEffectSlot& entry) { return !entry.active; });
    if (slot == slots_.end()) return std::nullopt;
    slot->active = true;
    slot->opcode = opcode;
    slot->insertion_order = next_insertion_order_++;
    return static_cast<std::size_t>(slot - slots_.begin());
}

std::optional<std::size_t> State6VisualEffectPool::add(const State6PositionEffect& effect) {
    const auto index = reserve(0x83);
    if (!index) return std::nullopt;
    auto& slot = slots_[*index];
    slot.effect = effect.effect;
    slot.from = effect.position;
    slot.local_from = effect.local_position;
    return index;
}

std::optional<std::size_t> State6VisualEffectPool::add(const State6TextEffect& effect) {
    const auto index = reserve(0x84);
    if (!index) return std::nullopt;
    auto& slot = slots_[*index];
    slot.effect = effect.effect;
    slot.from = effect.position;
    slot.local_from = effect.local_position;
    slot.text = effect.text;
    return index;
}

std::optional<std::size_t> State6VisualEffectPool::add(
    const State6ProjectileEffect& effect, std::uint32_t now_ms) {
    const auto index = reserve(0x85);
    if (!index) return std::nullopt;
    auto& slot = slots_[*index];
    slot.effect = effect.effect;
    slot.projectile_direction = projectile_direction(effect);
    slot.from = effect.from;
    slot.local_from = effect.local_from;
    slot.to = effect.to;
    slot.local_to = effect.local_to;
    const auto dx = effect.local_from.x - effect.local_to.x;
    const auto dy = effect.local_from.y - effect.local_to.y;
    const auto dx_subtiles = dx * 32;
    const auto dy_subtiles = dy * 32;
    const auto distance = std::sqrt(static_cast<float>(dx) * static_cast<float>(dx) +
                                    static_cast<float>(dy) * static_cast<float>(dy));
    const auto rounded_distance = std::nearbyint(distance);
    slot.projectile_duration_ms = static_cast<std::uint32_t>(rounded_distance * 150.0F);
    slot.projectile_deadline_ms = now_ms + slot.projectile_duration_ms;
    slot.projectile_initial_x_subtiles = dx_subtiles;
    slot.projectile_initial_y_subtiles = dy_subtiles;
    slot.projectile_remaining_x_subtiles = dx_subtiles;
    slot.projectile_remaining_y_subtiles = dy_subtiles;
    return index;
}

bool State6VisualEffectPool::release(std::size_t slot) noexcept {
    if (slot >= slots_.size() || !slots_[slot].active) return false;
    slots_[slot] = {};
    return true;
}

bool State6VisualEffectPool::advance_tick(const EffectDurationLookup& duration_for_effect,
                                         std::uint32_t now_ms) {
    if (!duration_for_effect)
        throw std::invalid_argument("effect duration lookup must be callable");

    constexpr std::uint32_t effect_cycle_interval_ms = 100;
    std::uint32_t effect_cycle_steps{};
    if (!effect_cycle_tick_initialized_) {
        last_effect_cycle_tick_ms_ = now_ms;
        effect_cycle_tick_initialized_ = true;
    } else {
        effect_cycle_steps = (now_ms - last_effect_cycle_tick_ms_) /
            effect_cycle_interval_ms;
        last_effect_cycle_tick_ms_ += effect_cycle_steps * effect_cycle_interval_ms;
    }

    bool changed = false;
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        auto& slot = slots_[index];
        if (!slot.active) continue;

        if (slot.opcode == 0x85) {
            const auto previous_x = slot.projectile_remaining_x_subtiles;
            const auto previous_y = slot.projectile_remaining_y_subtiles;
            const auto elapsed = now_ms - (slot.projectile_deadline_ms - slot.projectile_duration_ms);
            const auto advance_axis = [elapsed, &slot](std::int32_t initial) {
                const auto magnitude = static_cast<std::uint64_t>(
                    initial < 0 ? -static_cast<std::int64_t>(initial) : initial);
                if (magnitude == 0 || slot.projectile_duration_ms == 0) return std::int32_t{0};
                const auto speed = magnitude;
                const auto numerator = static_cast<std::uint64_t>(elapsed) * speed;
                const auto travelled = std::min<std::uint64_t>(magnitude,
                    (numerator + slot.projectile_duration_ms - 1U) /
                        slot.projectile_duration_ms);
                const auto remaining = static_cast<std::int32_t>(magnitude - travelled);
                return initial < 0 ? -remaining : remaining;
            };
            slot.projectile_remaining_x_subtiles = advance_axis(slot.projectile_initial_x_subtiles);
            slot.projectile_remaining_y_subtiles = advance_axis(slot.projectile_initial_y_subtiles);
            changed = changed || previous_x != slot.projectile_remaining_x_subtiles ||
                previous_y != slot.projectile_remaining_y_subtiles;
            if (now_ms > slot.projectile_deadline_ms &&
                slot.projectile_remaining_x_subtiles == 0 &&
                slot.projectile_remaining_y_subtiles == 0) {
                changed = release(index) || changed;
            }
            continue;
        }

        if (effect_cycle_steps == 0) continue;
        slot.update_ticks = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            static_cast<std::uint64_t>(slot.update_ticks) + effect_cycle_steps,
            std::numeric_limits<std::uint32_t>::max()));
        changed = true;
        if (slot.opcode == 0x83) {
            if (slot.effect == 0)
                throw std::runtime_error("position effect has invalid type zero");
            if (slot.update_ticks >= duration_for_effect(slot.effect))
                (void)release(index);
        } else if (slot.opcode == 0x84 && slot.update_ticks > 10) {
            (void)release(index);
        }
    }
    return changed;
}

const std::array<State6VisualEffectSlot, State6VisualEffectPool::capacity>&
State6VisualEffectPool::slots() const noexcept {
    return slots_;
}

void register_observed_state6_visual_update_handlers(
    MessageDispatcher& dispatcher, State6VisualUpdates consumers) {
    if (!consumers.transform_position || !consumers.position_effect || !consumers.text_effect ||
        !consumers.projectile_effect || !consumers.creature_value)
        throw std::invalid_argument("visual-update consumers must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x83,
        [consumer = consumers.position_effect, transform = consumers.transform_position](PacketReader& reader) {
            State6PositionEffect update;
            update.position = read_state6_wire_position(reader);
            update.local_position = transform(update.position);
            update.effect = reader.read_u8();
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x84,
        [consumer = consumers.text_effect, transform = consumers.transform_position](PacketReader& reader) {
            State6TextEffect update;
            update.position = read_state6_wire_position(reader);
            update.local_position = transform(update.position);
            update.effect = reader.read_u8();
            update.text = reader.read_string();
            if (update.text.size() > 9) update.text.resize(9);
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x85,
        [consumer = consumers.projectile_effect, transform = consumers.transform_position](PacketReader& reader) {
            State6ProjectileEffect update;
            update.from = read_state6_wire_position(reader);
            update.local_from = transform(update.from);
            update.to = read_state6_wire_position(reader);
            update.local_to = transform(update.to);
            update.effect = reader.read_u8();
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x86,
        [consumer = std::move(consumers.creature_value)](PacketReader& reader) {
            const State6CreatureValue update{reader.read_u32(), reader.read_u8()};
            consumer(update);
        });
}

void register_observed_state6_creature_update_handlers(
    MessageDispatcher& dispatcher, State6CreatureUpdateHandler consume_update) {
    if (!consume_update) throw std::invalid_argument("creature-update consumer must be callable");
    const auto register_fixed = [&dispatcher, &consume_update](std::uint8_t opcode,
        std::uint8_t field_count) {
        dispatcher.register_handler(DispatchPhase::state6, opcode,
            [consumer = consume_update, opcode, field_count](PacketReader& reader) {
                State6CreatureUpdate update;
                update.creature_id = reader.read_u32();
                update.opcode = opcode;
                update.field_count = field_count;
                const auto fields = reader.read_bytes(field_count);
                std::copy(fields.begin(), fields.end(), update.fields.begin());
                consumer(update);
            });
    };
    register_fixed(0x8C, 1);
    register_fixed(0x8D, 2);
    dispatcher.register_handler(DispatchPhase::state6, 0x8E,
        [consumer = consume_update](PacketReader& reader) {
            State6CreatureUpdate update;
            update.creature_id = reader.read_u32();
            update.opcode = 0x8E;
            const auto first = reader.read_u16();
            update.fields[0] = static_cast<std::uint8_t>(first & 0xffU);
            update.fields[1] = static_cast<std::uint8_t>(first >> 8U);
            update.field_count = 2;
            const std::uint8_t remaining_count = first == 0 ? 2 : 5;
            const auto remaining = reader.read_bytes(remaining_count);
            std::copy(remaining.begin(), remaining.end(), update.fields.begin() + 2);
            update.field_count = static_cast<std::uint8_t>(update.field_count + remaining_count);
            consumer(update);
        });
    register_fixed(0x8F, 2);
    register_fixed(0x90, 1);
    register_fixed(0x91, 1);
}

void State6CreatureStateStore::observe_map(const State6DecodedMap& map, bool reset) {
    if (reset) {
        creatures_.clear();
        battle_slots_.fill(0);
    }
    const auto slot_for = [this](std::uint32_t id) {
        return std::find(battle_slots_.begin(), battle_slots_.end(), id);
    };
    const auto write_native_u32 = [](State6CreatureRuntimeState& state,
        std::uint16_t native_offset, std::uint32_t value) {
        const auto index = static_cast<std::size_t>(native_offset -
            State6CreatureRuntimeState::first_native_offset);
        for (unsigned shift = 0; shift < 32; shift += 8)
            state.native_fields[index + shift / 8] =
                static_cast<std::uint8_t>(value >> shift);
    };
    const auto write_native_u8 = [](State6CreatureRuntimeState& state,
        std::uint16_t native_offset, std::uint8_t value) {
        const auto index = static_cast<std::size_t>(native_offset -
            State6CreatureRuntimeState::first_native_offset);
        state.native_fields[index] = value;
    };
    const auto apply_appearance = [&write_native_u32](State6CreatureRuntimeState& state,
        const tibia::map::CreatureAppearance& appearance) {
        if (appearance.has_full_appearance) {
            write_native_u32(state, 0x88, appearance.health_percent);
            write_native_u32(state, 0x60, appearance.outfit_type);
            if (appearance.outfit_type == 0) {
                write_native_u32(state, 0x64, appearance.outfit_item);
            } else {
                const std::array<std::uint16_t, 5> offsets{
                    0x64, 0x68, 0x6c, 0x70, 0x74};
                for (std::size_t index = 0; index < offsets.size(); ++index)
                    write_native_u32(state, offsets[index],
                        appearance.outfit_components[index]);
            }
            write_native_u32(state, 0x78, appearance.light_level);
            write_native_u32(state, 0x7c, appearance.light_color);
            write_native_u32(state, 0x8c, appearance.speed);
            write_native_u32(state, 0x94, appearance.skull);
            write_native_u32(state, 0x98, appearance.party_shield);
        }
        if (appearance.has_direction)
            write_native_u32(state, 0x50, appearance.direction);
    };

    for (const auto& tile : map.tiles) {
        for (const auto& object : tile.objects) {
            if (object.creature_id == 0) continue;
            if (object.type == 0x61) {
                auto slot = battle_slots_.end();
                const auto replaced_id = object.creature_appearance.replaced_creature_id;
                if (replaced_id != 0) {
                    slot = slot_for(replaced_id);
                    if (slot == battle_slots_.end())
                        slot = std::prev(battle_slots_.end());
                    const auto previous = creatures_.find(replaced_id);
                    if (previous != creatures_.end() && replaced_id != object.creature_id) {
                        auto state = std::move(previous->second);
                        creatures_.erase(previous);
                        state.creature_id = object.creature_id;
                        creatures_.insert_or_assign(object.creature_id, std::move(state));
                    }
                    *slot = object.creature_id;
                } else {
                    slot = slot_for(object.creature_id);
                    if (slot == battle_slots_.end()) {
                        slot = std::find(battle_slots_.begin(), battle_slots_.end(), 0);
                        if (slot == battle_slots_.end())
                            slot = std::prev(battle_slots_.end());
                        *slot = object.creature_id;
                    }
                }
                auto found = creatures_.find(object.creature_id);
                if (found == creatures_.end() && creatures_.size() < capacity) {
                    State6CreatureRuntimeState state;
                    state.creature_id = object.creature_id;
                    found = creatures_.emplace(object.creature_id, state).first;
                }
                if (found != creatures_.end()) {
                    apply_appearance(found->second, object.creature_appearance);
                    write_native_u8(found->second, 0x90, 1);
                }
                continue;
            }

            if (object.type != 0x62 && object.type != 0x63) continue;
            const auto found = creatures_.find(object.creature_id);
            if (found != creatures_.end()) {
                apply_appearance(found->second, object.creature_appearance);
                write_native_u8(found->second, 0x90, 1);
            }
        }
    }
}

void State6CreatureStateStore::forget(std::uint32_t creature_id) noexcept {
    creatures_.erase(creature_id);
    std::replace(battle_slots_.begin(), battle_slots_.end(), creature_id, 0U);
}

void State6CreatureStateStore::set_map_presence(
    std::uint32_t creature_id, bool present) noexcept {
    const auto found = creatures_.find(creature_id);
    if (found == creatures_.end()) return;
    constexpr std::size_t visibility_offset = 0x90 -
        State6CreatureRuntimeState::first_native_offset;
    found->second.native_fields[visibility_offset] = present ? 1 : 0;
}

bool State6CreatureStateStore::apply(const State6CreatureUpdate& update,
                                     std::int32_t unix_time_seconds) {
    const auto found = creatures_.find(update.creature_id);
    if (found == creatures_.end()) return false;
    auto& fields = found->second.native_fields;
    const auto read_u32 = [&fields](std::uint16_t offset) {
        const auto index = static_cast<std::size_t>(offset -
            State6CreatureRuntimeState::first_native_offset);
        return static_cast<std::uint32_t>(fields[index]) |
            (static_cast<std::uint32_t>(fields[index + 1]) << 8U) |
            (static_cast<std::uint32_t>(fields[index + 2]) << 16U) |
            (static_cast<std::uint32_t>(fields[index + 3]) << 24U);
    };
    const auto write_u32 = [&fields](std::uint16_t offset, std::uint32_t value) {
        const auto index = static_cast<std::size_t>(offset -
            State6CreatureRuntimeState::first_native_offset);
        for (unsigned shift = 0; shift < 32; shift += 8)
            fields[index + shift / 8] = static_cast<std::uint8_t>(value >> shift);
    };
    const auto byte = [&update](std::size_t index) {
        return static_cast<std::uint32_t>(update.fields[index]);
    };
    const auto word = [&update](std::size_t index) {
        return static_cast<std::uint32_t>(update.fields[index]) |
            (static_cast<std::uint32_t>(update.fields[index + 1]) << 8U);
    };

    switch (update.opcode) {
    case 0x8C: write_u32(0x88, byte(0)); break;
    case 0x8D:
        write_u32(0x78, byte(0));
        write_u32(0x7C, byte(1));
        break;
    case 0x8E:
        write_u32(0x60, word(0));
        if (word(0) == 0) write_u32(0x64, word(2));
        else {
            write_u32(0x64, byte(2));
            write_u32(0x68, byte(3));
            write_u32(0x6C, byte(4));
            write_u32(0x70, byte(5));
            write_u32(0x74, byte(6));
        }
        break;
    case 0x8F: write_u32(0x8C, word(0)); break;
    case 0x90: {
        const auto prior_value = read_u32(0x94);
        if (byte(0) == 0 && prior_value != 0) {
            write_u32(0x9C, static_cast<std::uint32_t>(unix_time_seconds));
        } else {
            write_u32(0x9C, 0);
            write_u32(0x94, byte(0));
        }
        return read_u32(0x94) != 0;
    }
    case 0x91: write_u32(0x98, byte(0)); break;
    default: return false;
    }
    return false;
}

bool State6CreatureStateStore::apply(const State6CreatureValue& update,
                                     std::uint32_t relative_time_ms) {
    const auto found = creatures_.find(update.creature_id);
    if (found == creatures_.end()) return false;
    const auto index = static_cast<std::size_t>(0x80 -
        State6CreatureRuntimeState::first_native_offset);
    auto& fields = found->second.native_fields;
    fields[index] = update.value;
    fields[index + 1] = 0;
    fields[index + 2] = 0;
    fields[index + 3] = 0;
    const auto expires = relative_time_ms + 1000U;
    for (unsigned shift = 0; shift < 32; shift += 8)
        fields[index + 4 + shift / 8] = static_cast<std::uint8_t>(expires >> shift);
    return true;
}

bool State6CreatureStateStore::apply_move_state(
    std::uint32_t creature_id, const State6CreatureMoveStateEffect& effect) noexcept {
    const auto found = creatures_.find(creature_id);
    if (found == creatures_.end() || !effect.movement_active) return false;
    auto& fields = found->second.native_fields;
    const auto write_u32 = [&fields](std::uint16_t offset, std::uint32_t value) {
        const auto index = static_cast<std::size_t>(offset -
            State6CreatureRuntimeState::first_native_offset);
        for (unsigned shift = 0; shift < 32; shift += 8)
            fields[index + shift / 8] = static_cast<std::uint8_t>(value >> shift);
    };
    write_u32(0x30, static_cast<std::uint32_t>(effect.offset_x_subtiles));
    write_u32(0x34, static_cast<std::uint32_t>(effect.offset_y_subtiles));
    write_u32(0x38, effect.step_end_tick);
    write_u32(0x3c, effect.full_end_tick);
    write_u32(0x40, static_cast<std::uint32_t>(effect.distance_x_subtiles));
    write_u32(0x44, static_cast<std::uint32_t>(effect.distance_y_subtiles));
    write_u32(0x48, static_cast<std::uint32_t>(effect.step_duration_ms));
    if (effect.previous_direction)
        write_u32(0x50, *effect.previous_direction);
    write_u32(0x54, effect.direction);
    fields[0x4c - State6CreatureRuntimeState::first_native_offset] = 1;
    return true;
}

bool State6CreatureStateStore::reset_local_player_motion(std::uint32_t creature_id) noexcept {
    const auto found = creatures_.find(creature_id);
    if (found == creatures_.end()) return false;
    auto& fields = found->second.native_fields;
    const auto byte_index = [](std::uint16_t offset) {
        return static_cast<std::size_t>(offset - State6CreatureRuntimeState::first_native_offset);
    };
    const auto source = byte_index(0x50);
    const auto destination = byte_index(0x54);
    std::copy_n(fields.begin() + source, 4, fields.begin() + destination);
    fields[byte_index(0x4C)] = 0;
    std::fill_n(fields.begin() + byte_index(0x30), 8, 0);
    return true;
}

bool State6CreatureStateStore::store_local_player_key_state(std::uint32_t creature_id,
    std::uint8_t key_state) noexcept {
    const auto found = creatures_.find(creature_id);
    if (found == creatures_.end()) return false;
    constexpr auto offset = static_cast<std::size_t>(0x50 -
        State6CreatureRuntimeState::first_native_offset);
    found->second.native_fields[offset] = key_state;
    found->second.native_fields[offset + 1] = 0;
    found->second.native_fields[offset + 2] = 0;
    found->second.native_fields[offset + 3] = 0;
    return true;
}

bool apply_observed_state6_local_player_reset(State6LocalPlayerMovementGlobals& globals,
    State6CreatureStateStore& creatures, std::uint32_t local_player_id) noexcept {
    globals.at_0x00605A14 = 0;
    globals.at_0x006059DC = 0;
    globals.at_0x006059D8 = 0;
    globals.at_0x006059D4 = 0;
    globals.at_0x00605A20 = 0;
    globals.at_0x00605A1C = 0;
    return creatures.reset_local_player_motion(local_player_id);
}

void apply_observed_state6_b5_stall_reset(State6LocalPlayerMovementGlobals& globals) noexcept {
    globals.at_0x00605A14 = 0;
    globals.at_0x006059DC = 0;
    globals.at_0x006059D8 = 0;
    globals.at_0x006059D4 = 0;
    globals.at_0x0060599C = 0;
    globals.at_0x00605998 = 0;
}

const State6CreatureRuntimeState* State6CreatureStateStore::find(
    std::uint32_t creature_id) const noexcept {
    const auto found = creatures_.find(creature_id);
    return found == creatures_.end() ? nullptr : &found->second;
}

std::size_t State6CreatureStateStore::size() const noexcept { return creatures_.size(); }

const std::array<std::uint32_t, State6CreatureStateStore::battle_capacity>&
State6CreatureStateStore::battle_slots() const noexcept {
    return battle_slots_;
}

std::optional<State6NotificationEventEffect>
make_observed_state6_notification_event_effect(const State6OpaqueNotification& update) {
    if (update.opcode != 0x96 && update.opcode != 0x97) return std::nullopt;
    const auto expected_values = update.opcode == 0x96 ? 3U : 2U;
    const auto expected_strings = update.opcode == 0x96 ? 3U : 1U;
    if (update.value_count != expected_values || update.strings.size() != expected_strings)
        throw std::invalid_argument("state-6 notification record has an invalid native-event shape");
    State6NotificationEventEffect effect;
    effect.source_opcode = update.opcode;
    effect.native_event_type = update.opcode == 0x96 ? 0x0F : 0x10;
    effect.values = {update.values[0], update.values[1], update.values[2]};
    effect.value_count = update.value_count;
    effect.strings = update.strings;
    if (update.opcode == 0x96)
        std::swap(effect.strings[0], effect.strings[1]);
    return effect;
}

void apply_observed_state6_native_status(State6NativeStatusState& state,
                                         const State6OpaqueNotification& update) {
    const auto value = [&update](std::size_t index) -> std::uint32_t {
        if (index >= update.value_count)
            throw std::invalid_argument("state-6 status update is missing observed fields");
        return update.values[index];
    };
    switch (update.opcode) {
    case 0xA0:
        state.at_0x006059CC = value(0);
        state.at_0x006059C8 = value(1);
        state.at_0x006059A0 = value(2);
        state.at_0x006059C4 = value(3);
        state.at_0x006059C0 = value(4);
        state.at_0x006059B8 = value(5);
        state.at_0x006059B0 = value(6);
        state.at_0x006059AC = value(7);
        state.at_0x006059BC = value(8);
        state.at_0x006059B4 = value(9);
        state.at_0x006059A8 = value(10);
        state.at_0x006059A4 = value(11);
        break;
    case 0xA1:
        if (update.value_count != 14)
            throw std::invalid_argument("state-6 0xA1 update must contain seven pairs");
        for (std::size_t index = 0; index < 7; ++index) {
            const auto progress = value(index * 2 + 1);
            if (progress > 100)
                throw std::runtime_error("state-6 0xA1 progress exceeds the observed 0..100 range");
            state.at_0x00605978[index] = value(index * 2);
            state.at_0x0060595C[index] = progress;
        }
        break;
    case 0xA2: state.at_0x00605958 = value(0); break;
    case 0xA3:
        state.at_0x0060599C = 0;
        state.at_0x00605998 = 0;
        break;
    default: break;
    }
}

void register_observed_state6_notification_handlers(
    MessageDispatcher& dispatcher, State6OpaqueNotificationHandler consume_notification) {
    if (!consume_notification)
        throw std::invalid_argument("notification consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0x96,
        [consumer = consume_notification](PacketReader& reader) {
            State6OpaqueNotification message;
            message.opcode = 0x96;
            message.values[0] = reader.read_u32();
            message.values[1] = reader.read_u16();
            message.values[2] = reader.read_u16();
            message.value_count = 3;
            for (int i = 0; i < 3; ++i) message.strings.push_back(reader.read_string());
            consumer(message);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0x97,
        [consumer = consume_notification](PacketReader& reader) {
            State6OpaqueNotification message;
            message.opcode = 0x97;
            message.values[0] = reader.read_u8();
            message.values[1] = reader.read_u32();
            message.value_count = 2;
            auto text = reader.read_string();
            if (text.size() > 29) text.resize(29);
            message.strings.push_back(std::move(text));
            consumer(message);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xA0,
        [consumer = consume_notification](PacketReader& reader) {
            State6OpaqueNotification message;
            message.opcode = 0xA0;
            const auto add = [&message](std::uint32_t value) {
                message.values[message.value_count++] = value;
            };
            add(reader.read_u16()); add(reader.read_u16()); add(reader.read_u16());
            add(reader.read_u32()); add(reader.read_u16()); add(reader.read_u8());
            add(reader.read_u16()); add(reader.read_u16()); add(reader.read_u8());
            add(reader.read_u8()); add(reader.read_u8()); add(reader.read_u16());
            consumer(message);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xA1,
        [consumer = consume_notification](PacketReader& reader) {
            State6OpaqueNotification message;
            message.opcode = 0xA1;
            for (int i = 0; i < 14; ++i)
                message.values[message.value_count++] = reader.read_u8();
            consumer(message);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xA2,
        [consumer = consume_notification](PacketReader& reader) {
            State6OpaqueNotification message;
            message.opcode = 0xA2;
            message.values[0] = reader.read_u16();
            message.value_count = 1;
            consumer(message);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xA3,
        [consumer = std::move(consume_notification)](PacketReader&) {
            consumer(State6OpaqueNotification{0xA3, {}, 0, {}});
        });
}

void register_observed_state6_communication_handlers(
    MessageDispatcher& dispatcher, State6CommunicationUpdates consumers) {
    if (!consumers.speech || !consumers.channel_list || !consumers.channel_open)
        throw std::invalid_argument("communication consumers must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0xAA,
        [consumer = consumers.speech](PacketReader& reader) {
            State6SpeechMessage message;
            message.speaker_id = reader.read_u32();
            message.speaker_name = reader.read_string();
            if (message.speaker_name.size() > 29) message.speaker_name.resize(29);
            message.level = reader.read_u16();
            message.mode = reader.read_u8();
            switch (message.mode) {
            case 1: case 2: case 3: case 0x10: case 0x11:
                message.has_position = true;
                message.position = read_state6_wire_position(reader);
                break;
            case 5: case 10: case 12: case 14:
                message.mode_value_u16 = reader.read_u16();
                break;
            case 6:
                message.mode_value_u32 = reader.read_u32();
                break;
            case 4: case 7: case 8: case 9: case 0x0B:
                break;
            default:
                throw std::runtime_error("state-6 speech has an unobserved mode");
            }
            message.text = reader.read_string();
            if (message.text.size() > 511) message.text.resize(511);
            consumer(message);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xAB,
        [consumer = consumers.channel_list](PacketReader& reader) {
            const auto count = reader.read_u8();
            std::vector<State6ChannelEntry> channels;
            channels.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i) {
                State6ChannelEntry entry;
                entry.id = reader.read_u16();
                entry.name = reader.read_string();
                if (entry.name.size() > 29) entry.name.resize(29);
                channels.push_back(std::move(entry));
            }
            consumer(channels);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xAC,
        [consumer = std::move(consumers.channel_open)](PacketReader& reader) {
            State6ChannelOpen channel;
            channel.id = reader.read_u16();
            channel.name = reader.read_string();
            if (channel.name.size() > 29) channel.name.resize(29);
            consumer(channel);
        });
}

State6ChannelEventEffect make_observed_state6_channel_list_effect(
    const std::vector<State6ChannelEntry>& channels) {
    return State6ChannelEventEffect{0xAB, 0x13, channels};
}

State6ChannelEventEffect make_observed_state6_channel_open_effect(
    const State6ChannelOpen& channel) {
    return State6ChannelEventEffect{0xAC, 0x33, {State6ChannelEntry{channel.id, channel.name}}};
}

void register_observed_state6_communication_control_handlers(
    MessageDispatcher& dispatcher, State6CommunicationControlHandler consume_update) {
    if (!consume_update) throw std::invalid_argument("communication-control consumer must be callable");
    const auto register_text = [&dispatcher, &consume_update](std::uint8_t opcode,
        std::size_t native_capacity) {
        dispatcher.register_handler(DispatchPhase::state6, opcode,
            [consumer = consume_update, opcode, native_capacity](PacketReader& reader) {
                State6CommunicationControl update;
                update.opcode = opcode;
                update.text = reader.read_string();
                if (update.text.size() >= native_capacity)
                    update.text.resize(native_capacity - 1);
                consumer(update);
            });
    };
    register_text(0xAD, 30);
    dispatcher.register_handler(DispatchPhase::state6, 0xAE,
        [consumer = consume_update](PacketReader& reader) {
            State6CommunicationControl update;
            update.opcode = 0xAE;
            update.field_u16 = reader.read_u16();
            consumer(update);
        });
    register_text(0xAF, 30);
    register_text(0xB0, 30);
    dispatcher.register_handler(DispatchPhase::state6, 0xB1,
        [consumer = consume_update](PacketReader&) {
            consumer(State6CommunicationControl{0xB1, 0, 0, {}});
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xB2,
        [consumer = consume_update](PacketReader& reader) {
            State6CommunicationControl update;
            update.opcode = 0xB2;
            update.field_u16 = reader.read_u16();
            update.text = reader.read_string();
            if (update.text.size() > 39) update.text.resize(39);
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xB3,
        [consumer = consume_update](PacketReader& reader) {
            State6CommunicationControl update;
            update.opcode = 0xB3;
            update.field_u16 = reader.read_u16();
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state6, 0xB4,
        [consumer = std::move(consume_update)](PacketReader& reader) {
            State6CommunicationControl update;
            update.opcode = 0xB4;
            update.field_u8 = reader.read_u8();
            if (update.field_u8 > 0x1A)
                throw std::runtime_error("state-6 opcode 0xB4 has an unobserved mode");
            update.text = reader.read_string();
            if (update.text.size() > 511) update.text.resize(511);
            consumer(update);
        });
}

std::optional<State6CommunicationEffect> make_observed_state6_communication_effect(
    const State6CommunicationControl& update) {
    switch (update.opcode) {
    case 0xAD:
        return State6CommunicationEffect{
            State6CommunicationEffectKind::enqueue_server_text,
            0xAD, 0, 0x32, 0, false, false, false, {}, update.text};
    case 0xAE:
        return State6CommunicationEffect{
            State6CommunicationEffectKind::refresh_channel_buffer,
            0xAE, 6, 0x35, update.field_u16, true, false, false, {}, {}};
    case 0xAF:
        return State6CommunicationEffect{
            State6CommunicationEffectKind::remove_console_filter,
            0xAF, 6, 0, 0, false, true, true, update.text, {}};
    case 0xB0:
        return State6CommunicationEffect{
            State6CommunicationEffectKind::finish_request,
            0xB0, 8, 0, 0, false, false, false, update.text,
            update.text + " has finished the request."};
    case 0xB1:
        return State6CommunicationEffect{
            State6CommunicationEffectKind::close_request,
            0xB1, 7, 0, 0, false, false, false, {}, "Your request has been closed."};
    case 0xB2:
        return State6CommunicationEffect{
            State6CommunicationEffectKind::enqueue_channel_event,
            0xB2, 0, 0x34, update.field_u16, false, false, false, update.text, {}};
    case 0xB3:
        return State6CommunicationEffect{
            State6CommunicationEffectKind::close_channel,
            0xB3, 5, 0, update.field_u16, false, false, false, {},
            "The channel has been closed. You need to re-join the channel if you get invited."};
    case 0xB4: {
        State6CommunicationEffect effect{
            State6CommunicationEffectKind::route_talk_message,
            0xB4, 0, 0, 0, false, false, false, {}, update.text};
        effect.talk_mode = update.field_u8;
        return effect;
    }
    default:
        return std::nullopt;
    }
}

void State6CommunicationEffectQueue::enqueue_channel_list(
    const std::vector<State6ChannelEntry>& channels) {
    channel_events_.push_back(make_observed_state6_channel_list_effect(channels));
}

void State6CommunicationEffectQueue::enqueue_channel_open(const State6ChannelOpen& channel) {
    channel_events_.push_back(make_observed_state6_channel_open_effect(channel));
}

void State6CommunicationEffectQueue::enqueue_control(
    const State6CommunicationControl& update) {
    if (auto effect = make_observed_state6_communication_effect(update))
        control_events_.push_back(std::move(*effect));
}

const std::vector<State6ChannelEventEffect>&
State6CommunicationEffectQueue::channel_events() const noexcept {
    return channel_events_;
}

const std::vector<State6CommunicationEffect>&
State6CommunicationEffectQueue::control_events() const noexcept {
    return control_events_;
}

void State6CommunicationEffectQueue::clear() noexcept {
    channel_events_.clear();
    control_events_.clear();
}

void register_observed_state6_opcode_b5_handler(
    MessageDispatcher& dispatcher, State6OpcodeB5Handler consume_value) {
    if (!consume_value) throw std::invalid_argument("opcode-0xB5 consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state6, 0xB5,
        [consumer = std::move(consume_value)](PacketReader& reader) {
            consumer(reader.read_u8());
        });
}

State6OpcodeB5InputEffect make_observed_state6_opcode_b5_input_effect(
    std::uint8_t value) noexcept {
    return State6OpcodeB5InputEffect{value};
}

bool apply_observed_state6_opcode_b5_input(State6LocalPlayerMovementGlobals& globals,
    State6CreatureStateStore& creatures, std::uint32_t local_player_id,
    std::uint8_t key_state) noexcept {
    globals.at_0x006059DC = 0;
    if (!creatures.store_local_player_key_state(local_player_id, key_state)) {
        globals.at_0x00605A20 = 0;
        globals.at_0x00605A1C = 0;
        return false;
    }
    return true;
}

std::optional<State6OutfitEventEffect> make_observed_state6_outfit_event_effect(
    const State6RecoveredRecord& record) {
    if (record.opcode != 0xC8) return std::nullopt;
    if (record.values.size() < 7 || record.values[6] < 1 || record.values[6] > 15 ||
        record.values.size() != 7U + static_cast<std::size_t>(record.values[6]) * 2U ||
        record.strings.size() != record.values[6]) {
        throw std::invalid_argument("state-6 opcode 0xC8 record has an invalid outfit-event shape");
    }
    State6OutfitEventEffect effect;
    std::copy_n(record.values.begin(), effect.header_values.size(), effect.header_values.begin());
    effect.names = record.strings;
    effect.outfit_values.reserve(static_cast<std::size_t>(record.values[6]) * 2U);
    effect.outfit_values.insert(effect.outfit_values.end(), record.values.begin() + 7, record.values.end());
    return effect;
}

void State6SessionProjection::apply(const State6OpaqueNotification& update) {
    if (update.opcode == 0x96 || update.opcode == 0x97) {
        if (auto effect = make_observed_state6_notification_event_effect(update))
            notification_events_.push_back(std::move(*effect));
        return;
    }
    apply_observed_state6_native_status(native_status_, update);
}

void State6SessionProjection::apply(const State6RecoveredRecord& record) {
    if (record.opcode == 0xC8) {
        if (auto effect = make_observed_state6_outfit_event_effect(record))
            outfit_events_.push_back(std::move(*effect));
    } else if (record.opcode >= 0xD2 && record.opcode <= 0xD4) {
        (void)buddies_.apply(record);
    } else if (record.opcode == 0xF0 || record.opcode == 0xF1) {
        (void)quests_.apply(record);
    }
}

const State6NativeStatusState& State6SessionProjection::native_status() const noexcept {
    return native_status_;
}

const State6BuddyStore& State6SessionProjection::buddies() const noexcept { return buddies_; }

const State6QuestState& State6SessionProjection::quests() const noexcept { return quests_; }

const std::vector<State6NotificationEventEffect>&
State6SessionProjection::notification_events() const noexcept {
    return notification_events_;
}

const std::vector<State6OutfitEventEffect>&
State6SessionProjection::outfit_events() const noexcept {
    return outfit_events_;
}

State6DecodedMap decode_state6_map_description(
    const State6MapDescription& description,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte,
    const resources::TibiaDatFile& dat) {
    if (description.floor > 15) {
        throw std::runtime_error("state-6 map floor is outside the observed 0..15 range");
    }

    PacketReader reader(description.tile_stream);
    State6DecodedMap map;
    map.center_x = description.center_x;
    map.center_y = description.center_y;
    map.center_floor = description.floor;
    const auto [first_floor, end_floor] = map_floor_range(description.floor);
    const auto step = description.floor < 8 ? 1 : -1;
    const auto floor_count = static_cast<std::size_t>((end_floor - first_floor) * step);
    map.tiles.reserve(18U * 14U * floor_count);
    std::unordered_map<std::uint32_t, std::string> known_creatures;
    std::uint16_t skipped_tiles = 0;

    const auto read_creature_appearance = [&reader](bool with_name,
        State6MapObject& object) {
        if (with_name) {
            object.creature_appearance.replaced_creature_id = reader.read_u32();
            object.creature_id = reader.read_u32();
            object.creature_name = read_state6_creature_name(reader, object.creature_id);
        }
        auto& appearance = object.creature_appearance;
        appearance.health_percent = reader.read_u8();
        appearance.direction = reader.read_u8();
        appearance.has_direction = true;
        appearance.outfit_type = reader.read_u16();
        if (appearance.outfit_type == 0) {
            appearance.outfit_item = reader.read_u16();
        } else {
            for (auto& component : appearance.outfit_components)
                component = reader.read_u8();
        }
        appearance.light_level = reader.read_u8();
        appearance.light_color = reader.read_u8();
        appearance.speed = reader.read_u16();
        appearance.skull = reader.read_u8();
        appearance.party_shield = reader.read_u8();
        appearance.has_full_appearance = true;
    };

    for (int floor = first_floor; floor != end_floor; floor += step) {
        for (std::uint8_t x = 0; x < 18; ++x) {
            for (std::uint8_t y = 0; y < 14; ++y) {
                auto tile = project_map_viewport_tile(description.center_x,
                    description.center_y, description.floor, x, y, floor);
                if (skipped_tiles != 0) {
                    --skipped_tiles;
                    map.tiles.push_back(std::move(tile));
                    continue;
                }

                while (true) {
                    const auto type = reader.read_u16();
                    if (type > 0xFEFF) {
                        skipped_tiles = static_cast<std::uint16_t>(type - 0xFF00);
                        break;
                    }
                    if (type == 0) {
                        throw std::runtime_error("state-6 map contains invalid field type 0");
                    }

                    State6MapObject object;
                    object.type = type;
                    if (type == 0x61) {
                        read_creature_appearance(true, object);
                        if (object.creature_appearance.replaced_creature_id != 0)
                            known_creatures.erase(
                                object.creature_appearance.replaced_creature_id);
                        known_creatures.insert_or_assign(object.creature_id,
                            object.creature_name);
                    } else if (type == 0x62) {
                        object.creature_id = reader.read_u32();
                        if (const auto found = known_creatures.find(object.creature_id);
                            found != known_creatures.end()) {
                            object.creature_name = found->second;
                            read_creature_appearance(false, object);
                        } else {
                            skip_unknown_creature_update(reader);
                        }
                    } else if (type == 0x63) {
                        object.creature_id = reader.read_u32();
                        object.creature_appearance.direction = reader.read_u8();
                        object.creature_appearance.has_direction = true;
                        if (const auto found = known_creatures.find(object.creature_id);
                            found != known_creatures.end()) {
                            object.creature_name = found->second;
                        }
                    } else if (std::binary_search(item_ids_with_extra_byte.begin(),
                                   item_ids_with_extra_byte.end(), type)) {
                        object.extra = reader.read_u8();
                        object.has_extra = true;
                    }
                    tile.objects.push_back(object);
                }
                (void)tibia::map::refresh_native_tile_object_context(tile, dat);
                map.tiles.push_back(std::move(tile));
            }
        }
    }
    const auto tail = reader.read_bytes(reader.remaining());
    map.unparsed_tail.assign(tail.begin(), tail.end());
    return map;
}

void register_observed_state4_notice_handlers(
    MessageDispatcher& dispatcher,
    State4NoticeHandler present_notice) {
    if (!present_notice) {
        throw std::invalid_argument("notice presenter must be callable");
    }
    const auto add_notice = [&dispatcher, presenter = present_notice](
                                std::uint8_t opcode,
                                std::string_view title,
                                bool cleanup) {
        dispatcher.register_handler(DispatchPhase::state4, opcode,
            [presenter, title, cleanup](PacketReader& reader) {
                auto message = reader.read_string();
                presenter(State4Notice{title, std::move(message), cleanup});
            });
    };
    add_notice(0x0A, "Sorry", true);
    add_notice(0x0B, "For Your Information", false);
}

void register_observed_state4_text_handler(
    MessageDispatcher& dispatcher,
    StatusLineState& state,
    State4TextHandler consume_text) {
    if (!consume_text) {
        throw std::invalid_argument("state 4 text consumer must be callable");
    }
    dispatcher.register_handler(DispatchPhase::state4, 0x14,
        [consumer = std::move(consume_text), state = &state](PacketReader& reader) {
            const auto message = reader.read_string();
            if (state->consume(message)) consumer(state->snapshot().text);
        });
}

void register_observed_state4_opcode_28_handler(
    MessageDispatcher& dispatcher, State4Opcode28Handler consume_action) {
    if (!consume_action) throw std::invalid_argument("state-4 opcode 0x28 consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state4, 0x28,
        [consumer = std::move(consume_action)](PacketReader&) { consumer(); });
}

void register_observed_state4_patch_update_handler(
    MessageDispatcher& dispatcher,
    State4PatchUpdateHandler consume_update) {
    if (!consume_update) {
        throw std::invalid_argument("state 4 patch update consumer must be callable");
    }
    dispatcher.register_handler(DispatchPhase::state4, 0x1E,
        [consumer = consume_update](PacketReader& reader) {
            const State4PatchUpdate update{
                reader.read_u8(), reader.read_u32(), PatchPayloadMode::delta_stream};
            consumer(update);
        });
    dispatcher.register_handler(DispatchPhase::state4, 0x1F,
        [consumer = std::move(consume_update)](PacketReader& reader) {
            const State4PatchUpdate update{
                reader.read_u8(), reader.read_u32(), PatchPayloadMode::full_file};
            consumer(update);
        });
}

void register_observed_state4_patch_chunk_handler(
    MessageDispatcher& dispatcher,
    State4PatchChunkHandler consume_chunk) {
    if (!consume_chunk) {
        throw std::invalid_argument("state 4 patch chunk consumer must be callable");
    }
    dispatcher.register_handler(DispatchPhase::state4, 0x20,
        [consumer = std::move(consume_chunk)](PacketReader& reader) {
            const auto chunk_size = static_cast<std::size_t>(reader.read_u16());
            const auto chunk = reader.read_bytes(chunk_size);
            consumer(State4PatchChunk{chunk});
        });
}

DispatchResult MessageDispatcher::dispatch(
    DispatchPhase phase,
    std::uint8_t opcode,
    std::span<const std::uint8_t> message_payload) const {
    const auto handler = handlers_.find(handler_key(phase, opcode));
    if (handler == handlers_.end()) {
        return {DispatchStatus::unknown_opcode, phase, opcode, message_payload.size()};
    }

    PacketReader reader(message_payload);
    handler->second(reader);
    return {DispatchStatus::handled, phase, opcode, reader.remaining()};
}

}
