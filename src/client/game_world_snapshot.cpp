#include "client/game_world_snapshot.hpp"
#include "map/tile_object_order.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace tibia::client {
namespace {

std::optional<std::int32_t> projectile_tile(
    std::int32_t destination, std::int32_t remaining) noexcept {
    const auto pixel = static_cast<std::int64_t>(destination) * 32 + remaining;
    const auto shifted = pixel + 31;
    const auto adjusted = shifted + (shifted < 0 ? 31 : 0);
    auto tile = adjusted / 32;
    if (adjusted < 0 && adjusted % 32 != 0) --tile;
    if (tile < INT32_MIN || tile > INT32_MAX) return std::nullopt;
    return static_cast<std::int32_t>(tile);
}

}

void GameWorldSnapshotMailbox::publish(
    const std::optional<network::State6DecodedMap>& map,
    const network::State6CreatureStateStore& creature_states,
    const navigation::LocalPlayerMovementState& local_player_movement,
    const TalkSystem& talk_system, const MessageLineList& primary_console_messages,
    const resources::TibiaDatFile& dat,
    const network::State6VisualEffectPool& visual_effects,
    const std::vector<network::State6InventorySlot>& inventory,
    const std::vector<network::State6Container>& containers,
    const network::State6BuddyStore& buddies,
    const network::State6QuestState& quest_state,
    const std::array<std::uint8_t, 32>& server_right_values,
    bool server_right_values_received,
    const network::State6Opcode82State& world_light,
    const network::State6NativeStatusState& native_status) {
    auto snapshot = std::make_shared<GameWorldSnapshot>();
    snapshot->map = map;
    snapshot->inventory = inventory;
    snapshot->containers = containers;
    snapshot->vip_contacts = buddies.entries();
    snapshot->quest_lines = quest_state.lines();
    snapshot->quest_log_entries = make_quest_log_entries(quest_state);
    snapshot->quest_flags_revision = quest_state.quest_flags_revision();
    snapshot->last_quest_flags_id = quest_state.last_quest_flags_id();
    snapshot->server_right_values = server_right_values;
    snapshot->server_right_values_received = server_right_values_received;
    snapshot->world_light = world_light;
    snapshot->battle_creature_slots = creature_states.battle_slots();
    snapshot->player_statistics.hit_points = native_status.at_0x006059CC;
    snapshot->player_statistics.maximum_hit_points = native_status.at_0x006059C8;
    snapshot->player_statistics.level = native_status.at_0x006059C0;
    snapshot->player_statistics.level_progress = native_status.at_0x006059B8;
    snapshot->player_statistics.capacity = native_status.at_0x006059A0;
    snapshot->player_statistics.experience = native_status.at_0x006059C4;
    snapshot->player_statistics.mana = native_status.at_0x006059B0;
    snapshot->player_statistics.maximum_mana = native_status.at_0x006059AC;
    snapshot->player_statistics.stamina_minutes = native_status.at_0x006059A4;
    snapshot->player_statistics.stamina_progress = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(native_status.at_0x006059A4) * 100U / 0xD20U);
    snapshot->player_statistics.magic_level = native_status.at_0x006059BC;
    snapshot->player_statistics.magic_level_progress = native_status.at_0x006059B4;
    snapshot->player_statistics.soul_points = native_status.at_0x006059A8;
    snapshot->player_statistics.skills = native_status.at_0x00605978;
    snapshot->player_statistics.skill_progress = native_status.at_0x0060595C;
    snapshot->local_player_movement = local_player_movement;
    if (map) {
        for (std::size_t index = 0; index < visual_effects.slots().size(); ++index) {
            const auto& effect = visual_effects.slots()[index];
            if (!effect.active) continue;
            if (effect.opcode == 0x85) {
                GameWorldMapEffectSnapshot projectile;
                projectile.index = index;
                projectile.insertion_order = effect.insertion_order;
                projectile.opcode = 0x85;
                projectile.renderable = false;
                projectile.effect = effect.effect;
                projectile.projectile_direction = effect.projectile_direction;
                const auto& source = effect.local_from;
                const auto& destination = effect.local_to;
                const auto remaining_x = effect.projectile_remaining_x_subtiles;
                const auto remaining_y = effect.projectile_remaining_y_subtiles;
                const auto current_tile_x = projectile_tile(destination.x, remaining_x);
                const auto current_tile_y = projectile_tile(destination.y, remaining_y);
                if (!current_tile_x || !current_tile_y) {
                    snapshot->has_unresolved_projectile_effects = true;
                    projectile.local_x = source.x;
                    projectile.local_y = source.y;
                    projectile.local_floor = destination.floor;
                    snapshot->map_effects.push_back(std::move(projectile));
                    continue;
                }
                projectile.local_x = *current_tile_x;
                projectile.local_y = *current_tile_y;
                projectile.local_floor = destination.floor;
                const auto* missile = dat.missiles.find(effect.effect);
                if (missile == nullptr || missile->width != 1 || missile->height != 1 ||
                    missile->layers == 0 || missile->pattern_x != 3 ||
                    missile->pattern_y != 3 || missile->pattern_z != 1) {
                    snapshot->has_unresolved_projectile_effects = true;
                    snapshot->map_effects.push_back(std::move(projectile));
                    continue;
                }
                const auto pixel_x = static_cast<std::int64_t>(*current_tile_x) * 32;
                const auto pixel_y = static_cast<std::int64_t>(*current_tile_y) * 32;
                const auto offset_x = (static_cast<std::int64_t>(destination.x) -
                    *current_tile_x) * 32 + remaining_x;
                const auto offset_y = (static_cast<std::int64_t>(destination.y) -
                    *current_tile_y) * 32 + remaining_y;
                if (pixel_x < INT32_MIN || pixel_x > INT32_MAX ||
                    pixel_y < INT32_MIN || pixel_y > INT32_MAX ||
                    offset_x < INT32_MIN || offset_x > INT32_MAX ||
                    offset_y < INT32_MIN || offset_y > INT32_MAX) {
                    snapshot->has_unresolved_projectile_effects = true;
                    snapshot->map_effects.push_back(std::move(projectile));
                    continue;
                }
                projectile.renderable = true;
                projectile.projectile_pixel_x = static_cast<std::int32_t>(pixel_x);
                projectile.projectile_pixel_y = static_cast<std::int32_t>(pixel_y);
                projectile.projectile_offset_x = static_cast<std::int32_t>(offset_x);
                projectile.projectile_offset_y = static_cast<std::int32_t>(offset_y);
                snapshot->map_effects.push_back(std::move(projectile));
                continue;
            }
            if (effect.opcode != 0x83 && effect.opcode != 0x84) continue;
            const auto position = network::transform_state6_wire_position(*map, effect.from);
            GameWorldMapEffectSnapshot visual;
            visual.index = index;
            visual.insertion_order = effect.insertion_order;
            visual.local_x = position.x;
            visual.local_y = position.y;
            visual.local_floor = position.floor;
            visual.opcode = effect.opcode;
            visual.effect = effect.effect;
            visual.text_color = effect.opcode == 0x84 ? effect.effect : std::uint8_t{};
            visual.update_ticks = effect.update_ticks;
            visual.text = effect.text;
            snapshot->map_effects.push_back(std::move(visual));
        }
        std::unordered_set<std::uint32_t> seen_creatures;
        for (const auto& tile : map->tiles) {
            const auto objects = tibia::map::resolve_native_tile_object_order(tile, dat);
            if (!objects) continue;
            for (const auto* object : *objects) {
                if ((object->type != 0x61 && object->type != 0x62 && object->type != 0x63) ||
                    object->creature_id == 0 ||
                    !seen_creatures.insert(object->creature_id).second)
                    continue;
                if (const auto* creature = creature_states.find(object->creature_id))
                    snapshot->creatures.push_back({
                        *creature, tile.x, tile.y, tile.floor,
                        tibia::map::viewport_map_position(*map, tile),
                        object->creature_name});
            }
        }
        const auto highest_floor = tibia::map::compute_highest_active_render_floor(*map, dat);
        if (highest_floor)
            snapshot->render_filter_grid = tibia::map::compute_map_render_filter_grid(
                *map, dat, *highest_floor);
        const auto player = std::find_if(snapshot->creatures.begin(),
            snapshot->creatures.end(), [&](const GameWorldCreatureSnapshot& creature) {
                return creature.state.creature_id == map->local_player_id;
            });
    }
    snapshot->talkboxes = talk_system.talkboxes();
    snapshot->primary_console_messages.assign(primary_console_messages.lines().begin(),
        primary_console_messages.lines().end());
    snapshot->status_text = talk_system.status_text();
    snapshot->status_visible = talk_system.has_active_status_message();
    const std::lock_guard lock(mutex_);
    std::erase_if(removed_vip_contact_ids_, [&](std::uint32_t id) {
        return std::none_of(buddies.entries().begin(), buddies.entries().end(),
            [id](const network::State6BuddyEntry& entry) { return entry.id == id; });
    });
    std::erase_if(snapshot->vip_contacts, [&](const network::State6BuddyEntry& entry) {
        return removed_vip_contact_ids_.contains(entry.id);
    });
    if (map) {
        const auto player = std::find_if(snapshot->creatures.begin(),
            snapshot->creatures.end(), [&](const GameWorldCreatureSnapshot& creature) {
                return creature.state.creature_id == map->local_player_id;
            });
        if (player != snapshot->creatures.end()) {
            automap_dat_ = &dat;
            map::update_automap_player_position(automap_view_state_,
                player->world_x, player->world_y, player->world_floor);
            snapshot->automap_view = automap_store_.update(*map, dat,
                player->world_x, player->world_y, player->world_floor,
                automap_view_state_);
        }
    }
    snapshot->sequence = next_sequence_++;
    snapshot_ = std::move(snapshot);
}

std::shared_ptr<const GameWorldSnapshot> GameWorldSnapshotMailbox::read() const {
    const std::lock_guard lock(mutex_);
    return snapshot_;
}

std::optional<std::vector<network::State6PathSearchCell>>
GameWorldSnapshotMailbox::path_search_grid(
    network::State6PlayerPosition origin) {
    const std::lock_guard lock(mutex_);
    return automap_store_.path_search_grid(origin);
}

void GameWorldSnapshotMailbox::set_automap_zoom(std::int32_t zoom) noexcept {
    const std::lock_guard lock(mutex_);
    if (zoom != 1 && zoom != 2 && zoom != 4 && zoom != 8) zoom = 2;
    automap_view_state_.zoom = zoom;
}

std::int32_t GameWorldSnapshotMailbox::automap_zoom() const noexcept {
    const std::lock_guard lock(mutex_);
    return automap_view_state_.zoom;
}

void GameWorldSnapshotMailbox::apply_automap_action(map::AutomapAction action) {
    const std::lock_guard lock(mutex_);
    if (!snapshot_ || !snapshot_->map || automap_dat_ == nullptr) return;
    const auto player = std::find_if(snapshot_->creatures.begin(),
        snapshot_->creatures.end(), [&](const GameWorldCreatureSnapshot& creature) {
            return creature.state.creature_id == snapshot_->map->local_player_id;
        });
    if (player == snapshot_->creatures.end()) return;
    map::apply_automap_action(automap_view_state_, action,
        player->world_x, player->world_y, player->world_floor);
    auto updated = std::make_shared<GameWorldSnapshot>(*snapshot_);
    updated->automap_view = automap_store_.update(*updated->map, *automap_dat_,
        player->world_x, player->world_y, player->world_floor,
        automap_view_state_);
    updated->sequence = next_sequence_++;
    snapshot_ = std::move(updated);
}

void GameWorldSnapshotMailbox::remove_vip_contact(std::uint32_t character_id) {
    const std::lock_guard lock(mutex_);
    removed_vip_contact_ids_.insert(character_id);
    if (!snapshot_) return;
    auto updated = std::make_shared<GameWorldSnapshot>(*snapshot_);
    std::erase_if(updated->vip_contacts, [character_id](
        const network::State6BuddyEntry& entry) { return entry.id == character_id; });
    updated->sequence = next_sequence_++;
    snapshot_ = std::move(updated);
}

void GameWorldSnapshotMailbox::clear() noexcept {
    const std::lock_guard lock(mutex_);
    snapshot_.reset();
    removed_vip_contact_ids_.clear();
    next_sequence_ = 0;
}

}
