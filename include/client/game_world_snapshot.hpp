#pragma once

#include "client/talk_system.hpp"
#include "client/message_line_list.hpp"
#include "client/quest_log_view.hpp"
#include "map/render_cell_filter.hpp"
#include "map/automap_store.hpp"
#include "network/message_dispatcher.hpp"
#include "resources/tibia_dat.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tibia::client {

struct GameWorldCreatureSnapshot {
    network::State6CreatureRuntimeState state;
    std::uint16_t world_x{};
    std::uint16_t world_y{};
    std::uint8_t world_floor{};
    std::optional<map::ViewportMapPosition> viewport_map_position;
    std::string name;
};

struct GameWorldMapEffectSnapshot {
    std::size_t index{};
    std::uint64_t insertion_order{};
    std::int32_t local_x{};
    std::int32_t local_y{};
    std::int32_t local_floor{};
    std::uint8_t opcode{};
    bool renderable{true};
    std::uint8_t effect{};
    std::uint8_t text_color{};
    std::uint8_t projectile_direction{};
    std::uint32_t update_ticks{};
    std::int32_t projectile_pixel_x{};
    std::int32_t projectile_pixel_y{};
    std::int32_t projectile_offset_x{};
    std::int32_t projectile_offset_y{};
    std::string text;
};

struct GamePlayerStatistics {
    std::uint32_t hit_points{};
    std::uint32_t maximum_hit_points{};
    std::uint32_t level{};
    std::uint32_t level_progress{};
    std::uint32_t capacity{};
    std::uint32_t experience{};
    std::uint32_t mana{};
    std::uint32_t maximum_mana{};
    std::uint32_t stamina_minutes{};
    std::uint32_t stamina_progress{};
    std::uint32_t magic_level{};
    std::uint32_t magic_level_progress{};
    std::uint32_t soul_points{};
    std::array<std::uint32_t, 7> skills{};
    std::array<std::uint32_t, 7> skill_progress{};
};

struct GameWorldSnapshot {
    std::optional<network::State6DecodedMap> map;
    std::vector<network::State6InventorySlot> inventory;
    std::vector<network::State6Container> containers;
    std::vector<network::State6BuddyEntry> vip_contacts;
    std::vector<network::State6QuestLine> quest_lines;
    std::vector<QuestLogEntry> quest_log_entries;
    std::uint64_t quest_flags_revision{};
    std::optional<std::uint16_t> last_quest_flags_id;
    std::array<std::uint8_t, 32> server_right_values{};
    bool server_right_values_received{};
    network::State6Opcode82State world_light;
    std::optional<map::MapRenderFilterGrid> render_filter_grid;
    std::optional<map::AutomapView> automap_view;
    std::vector<GameWorldCreatureSnapshot> creatures;
    std::array<std::uint32_t, network::State6CreatureStateStore::battle_capacity>
        battle_creature_slots{};
    std::vector<GameWorldMapEffectSnapshot> map_effects;
    GamePlayerStatistics player_statistics;
    bool has_unresolved_projectile_effects{};
    navigation::LocalPlayerMovementState local_player_movement;
    std::array<Talkbox, TalkSystem::talkbox_count> talkboxes{};
    std::vector<MessageLine> primary_console_messages;
    std::string status_text;
    bool status_visible{};
    std::uint64_t sequence{};
};

class GameWorldSnapshotMailbox {
public:
    void publish(const std::optional<network::State6DecodedMap>& map,
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
        const network::State6NativeStatusState& native_status);
    [[nodiscard]] std::shared_ptr<const GameWorldSnapshot> read() const;
    void set_automap_zoom(std::int32_t zoom) noexcept;
    [[nodiscard]] std::int32_t automap_zoom() const noexcept;
    void apply_automap_action(map::AutomapAction action);
    void clear() noexcept;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const GameWorldSnapshot> snapshot_;
    map::AutomapStore automap_store_;
    map::AutomapViewState automap_view_state_;
    const resources::TibiaDatFile* automap_dat_{};
    std::uint64_t next_sequence_{};
};

}
