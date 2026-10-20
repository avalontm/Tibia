#pragma once

#include "network/packet_stream.hpp"
#include "map/model.hpp"
#include "navigation/navigation.hpp"
#include "navigation/movement_state.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tibia::resources { struct TibiaDatFile; }

namespace tibia::network {

class StatusLineState;

enum class DispatchPhase : std::uint8_t {
    state4 = 4,
    state6 = 6,
};

enum class DispatchStatus {
    handled,
    unknown_opcode,
};

struct DispatchResult {
    DispatchStatus status{};
    DispatchPhase phase{};
    std::uint8_t opcode{};
    std::size_t unconsumed_bytes{};
};
class MessageDispatcher {
public:
    using Handler = std::function<void(PacketReader&)>;

    void register_handler(DispatchPhase phase, std::uint8_t opcode, Handler handler);
    [[nodiscard]] DispatchResult dispatch(
        DispatchPhase phase,
        std::uint8_t opcode,
        std::span<const std::uint8_t> message_payload) const;

private:
    std::unordered_map<std::uint16_t, Handler> handlers_;
};

using EncryptedPayloadSender = std::function<void(std::span<const std::uint8_t>)>;
void register_observed_ping_reply_handler(
    MessageDispatcher& dispatcher,
    EncryptedPayloadSender send_encrypted_payload);

using State6ByteBlockHandler = std::function<void(const std::array<std::uint8_t, 32>&)>;
void register_observed_state6_byte_block_handler(
    MessageDispatcher& dispatcher,
    State6ByteBlockHandler consume_byte_block);

struct State6GameStart {
    std::uint32_t local_player_id{};
    std::uint16_t movement_tick_quantum{};
    std::uint8_t status_slot_0x28{};
};

using State6GameStartHandler = std::function<void(const State6GameStart&)>;
void register_observed_state6_game_start_handler(
    MessageDispatcher& dispatcher,
    State6GameStartHandler consume_game_start);

struct State6Notice {
    std::uint8_t opcode{};
    std::string_view title;
    std::string message;
    bool performs_followup_cleanup{};
};
using State6NoticeHandler = std::function<void(const State6Notice&)>;
void register_observed_state6_notice_handlers(
    MessageDispatcher& dispatcher, State6NoticeHandler present_notice);
struct State6Opcode16Message {
    std::string text;
    std::uint8_t value{};
};
using State6Opcode16Handler = std::function<void(const State6Opcode16Message&)>;
void register_observed_state6_opcode_16_handler(
    MessageDispatcher& dispatcher, State6Opcode16Handler consume_message);

struct State6RecoveredRecord {
    std::uint8_t opcode{};
    std::vector<std::uint32_t> values;
    std::vector<std::string> strings;
};
using State6RecoveredRecordHandler = std::function<void(const State6RecoveredRecord&)>;
void register_observed_state6_remaining_record_handlers(
    MessageDispatcher& dispatcher, State6RecoveredRecordHandler consume_record);

struct State6BuddyEntry {
    std::uint32_t id{};
    std::string name;
    bool online{};
};
class State6BuddyStore {
public:
    static constexpr std::size_t capacity = 200;
    [[nodiscard]] bool apply(const State6RecoveredRecord& record);
    [[nodiscard]] const State6BuddyEntry* find(std::uint32_t id) const noexcept;
    [[nodiscard]] const std::vector<State6BuddyEntry>& entries() const noexcept;
    [[nodiscard]] std::size_t online_count() const noexcept;

private:
    std::vector<State6BuddyEntry> entries_;
    std::size_t online_count_{};
};

struct State6QuestFlag {
    std::string name;
    std::string description;
};
struct State6QuestLine {
    std::uint16_t id{};
    std::string name;
    bool completed{};
    std::vector<State6QuestFlag> flags;
};
class State6QuestState {
public:
    [[nodiscard]] bool apply(const State6RecoveredRecord& record);
    [[nodiscard]] const State6QuestLine* find(std::uint16_t id) const noexcept;
    [[nodiscard]] const std::vector<State6QuestLine>& lines() const noexcept;
    [[nodiscard]] bool quest_lines_changed() const noexcept;
    [[nodiscard]] bool quest_flags_changed() const noexcept;
    [[nodiscard]] std::uint64_t quest_flags_revision() const noexcept;
    [[nodiscard]] std::optional<std::uint16_t> last_quest_flags_id() const noexcept;

private:
    std::vector<State6QuestLine> lines_;
    bool quest_lines_changed_{};
    bool quest_flags_changed_{};
    std::uint64_t quest_flags_revision_{};
    std::optional<std::uint16_t> last_quest_flags_id_;
};

struct State6MapDescription {
    std::uint16_t center_x{};
    std::uint16_t center_y{};
    std::uint8_t floor{};
    std::vector<std::uint8_t> tile_stream;
};

using State6MapObject = tibia::map::Object;
using State6MapTile = tibia::map::Tile;
using State6DecodedMap = tibia::map::DecodedMap;
enum class State6MapDirection : std::uint8_t { north, east, south, west };
struct State6MapStrip {
    State6MapDirection direction{};
    std::uint8_t center_floor{};
    std::vector<State6MapTile> tiles;
    std::vector<std::uint8_t> raw_stream;
    std::size_t wire_bytes_consumed{};
};

enum class State6FloorDirection : std::uint8_t { up, down };
struct State6FloorTransitionPacket {
    State6FloorDirection direction{};
    std::vector<std::uint8_t> raw_stream;
};
struct State6FloorTransitionResult {
    std::uint16_t center_x{};
    std::uint16_t center_y{};
    std::uint8_t center_floor{};
    std::int8_t floor_cache_shift{};
    std::vector<std::uint8_t> refreshed_floors;
    std::size_t decoded_tiles{};
    std::size_t wire_bytes_consumed{};
};
using State6FloorTransitionHandler = std::function<std::size_t(const State6FloorTransitionPacket&)>;
void register_observed_state6_floor_transition_handlers(
    MessageDispatcher& dispatcher, State6FloorTransitionHandler consume_transition);
[[nodiscard]] State6FloorTransitionResult apply_state6_floor_transition(
    State6DecodedMap& map, const State6FloorTransitionPacket& transition,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte,
    const resources::TibiaDatFile& dat);

using State6MapStripHandler = std::function<std::size_t(const State6MapStrip&)>;
using State6MapContextProvider = std::function<const State6DecodedMap*()>;
void register_observed_state6_map_strip_handlers(
    MessageDispatcher& dispatcher, State6MapStripHandler consume_strip);
[[nodiscard]] State6MapStrip decode_state6_map_strip(
    State6MapDirection direction, std::uint8_t center_floor,
    std::span<const std::uint8_t> stream,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte,
    const State6DecodedMap* current_map = nullptr);

struct State6MapTileUpdate {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
    State6MapTile tile;
};
using State6MapTileUpdateHandler = std::function<void(const State6MapTileUpdate&)>;
void register_observed_state6_map_tile_update_handler(
    MessageDispatcher& dispatcher, State6MapTileUpdateHandler consume_update,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte,
    State6MapContextProvider current_map = {});

struct State6MapThingAdd {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
    State6MapObject object;
};
using State6MapThingAddHandler = std::function<void(const State6MapThingAdd&)>;
void register_observed_state6_map_thing_add_handler(
    MessageDispatcher& dispatcher, State6MapThingAddHandler consume_add,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte,
    State6MapContextProvider current_map = {});

struct State6MapThingTransform {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
    std::uint8_t stack_index{};
    State6MapObject object;
};
using State6MapThingTransformHandler = std::function<void(const State6MapThingTransform&)>;
void register_observed_state6_map_thing_transform_handler(
    MessageDispatcher& dispatcher, State6MapThingTransformHandler consume_transform,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte,
    State6MapContextProvider current_map = {});

struct State6MapThingRemove {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
    std::uint8_t stack_index{};
};
using State6MapThingRemoveHandler = std::function<void(const State6MapThingRemove&)>;
void register_observed_state6_map_thing_remove_handler(
    MessageDispatcher& dispatcher, State6MapThingRemoveHandler consume_remove);

struct State6ContainerItem {
    std::uint16_t type{};
    std::uint8_t extra{};
    bool has_extra{};
};
struct State6Container {
    std::uint8_t id{};
    State6ContainerItem container_item;
    std::string name;
    std::uint8_t capacity{};
    bool has_parent{};
    std::vector<State6ContainerItem> items;
    std::uint64_t revision{};
};
enum class State6ContainerChangeKind : std::uint8_t { add, update, remove };
struct State6ContainerChange {
    State6ContainerChangeKind kind{};
    std::uint8_t container_id{};
    std::uint8_t slot{};
    State6ContainerItem item;
};
using State6ContainerOpenHandler = std::function<void(const State6Container&)>;
using State6ContainerCloseHandler = std::function<void(std::uint8_t)>;
using State6ContainerChangeHandler = std::function<void(const State6ContainerChange&)>;
void register_observed_state6_container_handlers(
    MessageDispatcher& dispatcher,
    State6ContainerOpenHandler consume_open,
    State6ContainerCloseHandler consume_close,
    State6ContainerChangeHandler consume_change,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte);
void apply_state6_container_open(std::vector<State6Container>& containers,
    const State6Container& container);
void apply_state6_container_close(std::vector<State6Container>& containers, std::uint8_t id);
void apply_state6_container_change(std::vector<State6Container>& containers,
    const State6ContainerChange& change);

enum class State6ShopUpdateKind : std::uint8_t { open, goods, close };
struct State6ShopUpdate {
    State6ShopUpdateKind kind{};
    std::string title;
    std::vector<State6ContainerItem> items;
};
class State6ShopState {
public:
    [[nodiscard]] bool apply(const State6ShopUpdate& update);
    [[nodiscard]] const std::string& own_offer_name() const noexcept;
    [[nodiscard]] const std::vector<State6ContainerItem>& own_offers() const noexcept;
    [[nodiscard]] const std::string& counter_offer_name() const noexcept;
    [[nodiscard]] const std::vector<State6ContainerItem>& counter_offers() const noexcept;

private:
    std::string own_offer_name_;
    std::vector<State6ContainerItem> own_offers_;
    std::string counter_offer_name_;
    std::vector<State6ContainerItem> counter_offers_;
};
using State6ShopUpdateHandler = std::function<void(const State6ShopUpdate&)>;
void register_observed_state6_shop_handlers(
    MessageDispatcher& dispatcher, State6ShopUpdateHandler consume_update,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte);

struct State6InventorySlot {
    std::uint8_t slot{};
    State6ContainerItem item;
};
struct State6InventoryUpdate {
    std::uint8_t slot{};
    bool remove{};
    State6ContainerItem item;
};
using State6InventoryUpdateHandler = std::function<void(const State6InventoryUpdate&)>;
void register_observed_state6_inventory_handlers(
    MessageDispatcher& dispatcher, State6InventoryUpdateHandler consume_update,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte);
void apply_state6_inventory_update(std::vector<State6InventorySlot>& inventory,
    const State6InventoryUpdate& update);

struct State6Opcode82Values {
    std::uint8_t value_0{};
    std::uint8_t value_1{};
};
struct State6Opcode82State {
    std::int32_t global_610c00{-1};
    std::int32_t global_755de8{};
    std::int32_t global_755dec{};
    std::int32_t global_755df0{};
    std::int32_t global_610bfc{};
    std::int32_t global_755df4{};
    std::int32_t global_755df8{};
    std::int32_t global_755dfc{};
};
[[nodiscard]] std::int32_t apply_observed_state6_opcode_82(
    State6Opcode82State& state, const State6Opcode82Values& values);
void advance_observed_state6_opcode_82(State6Opcode82State& state) noexcept;
using State6Opcode82Handler = std::function<void(const State6Opcode82Values&)>;
void register_observed_state6_opcode_82_handler(
    MessageDispatcher& dispatcher, State6Opcode82Handler consume_values);

struct State6WirePosition {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
};
struct State6MapLocalPosition {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t floor{};
};
[[nodiscard]] State6MapLocalPosition transform_state6_wire_position(
    const State6DecodedMap& map, const State6WirePosition& position);
struct State6PositionEffect {
    State6WirePosition position;
    State6MapLocalPosition local_position;
    std::uint8_t effect{};
};
struct State6TextEffect {
    State6WirePosition position;
    State6MapLocalPosition local_position;
    std::uint8_t effect{};
    std::string text;
};
struct State6ProjectileEffect {
    State6WirePosition from;
    State6MapLocalPosition local_from;
    State6WirePosition to;
    State6MapLocalPosition local_to;
    std::uint8_t effect{};
};
struct State6CreatureValue {
    std::uint32_t creature_id{};
    std::uint8_t value{};
};
struct State6VisualUpdates {
    std::function<State6MapLocalPosition(const State6WirePosition&)> transform_position;
    std::function<void(const State6PositionEffect&)> position_effect;
    std::function<void(const State6TextEffect&)> text_effect;
    std::function<void(const State6ProjectileEffect&)> projectile_effect;
    std::function<void(const State6CreatureValue&)> creature_value;
};
struct State6VisualEffectSlot {
    bool active{};
    std::uint8_t opcode{};
    std::uint8_t effect{};
    std::uint8_t projectile_direction{};
    std::uint64_t insertion_order{};
    State6WirePosition from;
    State6MapLocalPosition local_from;
    State6WirePosition to;
    State6MapLocalPosition local_to;
    std::string text;
    std::uint32_t update_ticks{};
    std::uint32_t projectile_deadline_ms{};
    std::uint32_t projectile_duration_ms{};
    std::int32_t projectile_initial_x_subtiles{};
    std::int32_t projectile_initial_y_subtiles{};
    std::int32_t projectile_remaining_x_subtiles{};
    std::int32_t projectile_remaining_y_subtiles{};
};
class State6VisualEffectPool {
public:
    static constexpr std::size_t capacity = 200;
    using EffectDurationLookup = std::function<std::uint32_t(std::uint8_t)>;
    [[nodiscard]] std::optional<std::size_t> add(const State6PositionEffect& effect);
    [[nodiscard]] std::optional<std::size_t> add(const State6TextEffect& effect);
    [[nodiscard]] std::optional<std::size_t> add(
        const State6ProjectileEffect& effect, std::uint32_t now_ms = 0);
    [[nodiscard]] bool release(std::size_t slot) noexcept;
    [[nodiscard]] bool advance_tick(const EffectDurationLookup& duration_for_effect,
                      std::uint32_t now_ms = 0);
    [[nodiscard]] const std::array<State6VisualEffectSlot, capacity>& slots() const noexcept;

private:
    [[nodiscard]] std::optional<std::size_t> reserve(std::uint8_t opcode);
    std::array<State6VisualEffectSlot, capacity> slots_{};
    std::uint64_t next_insertion_order_{};
    std::uint32_t last_effect_cycle_tick_ms_{};
    bool effect_cycle_tick_initialized_{};
};
void register_observed_state6_visual_update_handlers(
    MessageDispatcher& dispatcher, State6VisualUpdates consumers);

struct State6CreatureUpdate {
    std::uint32_t creature_id{};
    std::uint8_t opcode{};
    std::array<std::uint8_t, 7> fields{};
    std::uint8_t field_count{};
};
using State6CreatureUpdateHandler = std::function<void(const State6CreatureUpdate&)>;
struct State6CreatureRuntimeState {
    static constexpr std::uint16_t first_native_offset = 0x30;
    std::uint32_t creature_id{};
    std::array<std::uint8_t, 0x70> native_fields{};
};
class State6CreatureStateStore {
public:
    static constexpr std::size_t capacity = 600;
    static constexpr std::size_t battle_capacity = 150;
    void observe_map(const State6DecodedMap& map, bool reset);
    void set_map_presence(std::uint32_t creature_id, bool present) noexcept;
    void forget(std::uint32_t creature_id) noexcept;
    [[nodiscard]] bool apply(const State6CreatureUpdate& update,
                             std::int32_t unix_time_seconds);
    [[nodiscard]] bool apply(const State6CreatureValue& update,
                             std::uint32_t relative_time_ms);
    [[nodiscard]] bool apply_move_state(std::uint32_t creature_id,
        const State6CreatureMoveStateEffect& effect) noexcept;
    [[nodiscard]] bool reset_local_player_motion(std::uint32_t creature_id) noexcept;
    [[nodiscard]] bool store_local_player_key_state(std::uint32_t creature_id,
                                                    std::uint8_t key_state) noexcept;
    [[nodiscard]] const State6CreatureRuntimeState* find(std::uint32_t creature_id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::array<std::uint32_t, battle_capacity>& battle_slots() const noexcept;

private:
    std::unordered_map<std::uint32_t, State6CreatureRuntimeState> creatures_;
    std::array<std::uint32_t, battle_capacity> battle_slots_{};
};

using State6LocalPlayerMovementGlobals = tibia::navigation::LocalPlayerMovementState;
[[nodiscard]] bool apply_observed_state6_local_player_reset(
    State6LocalPlayerMovementGlobals& globals, State6CreatureStateStore& creatures,
    std::uint32_t local_player_id) noexcept;
void apply_observed_state6_b5_stall_reset(State6LocalPlayerMovementGlobals& globals) noexcept;
void register_observed_state6_creature_update_handlers(
    MessageDispatcher& dispatcher, State6CreatureUpdateHandler consume_update);

struct State6OpaqueNotification {
    std::uint8_t opcode{};
    std::array<std::uint32_t, 16> values{};
    std::uint8_t value_count{};
    std::vector<std::string> strings;
};
struct State6NotificationEventEffect {
    std::uint8_t source_opcode{};
    std::uint8_t native_event_type{};
    std::array<std::uint32_t, 3> values{};
    std::uint8_t value_count{};
    std::vector<std::string> strings;
};
[[nodiscard]] std::optional<State6NotificationEventEffect>
make_observed_state6_notification_event_effect(const State6OpaqueNotification& update);
struct State6NativeStatusState {
    std::uint32_t at_0x006059CC{}, at_0x006059C8{}, at_0x006059A0{};
    std::uint32_t at_0x006059C4{}, at_0x006059C0{}, at_0x006059B8{};
    std::uint32_t at_0x006059B0{}, at_0x006059AC{};
    std::uint32_t at_0x006059B4{}, at_0x006059BC{};
    std::uint32_t at_0x006059A8{}, at_0x006059A4{};
    std::array<std::uint32_t, 7> at_0x00605978{};
    std::array<std::uint32_t, 7> at_0x0060595C{};
    std::uint32_t at_0x00605958{}, at_0x0060599C{}, at_0x00605998{};
};
void apply_observed_state6_native_status(State6NativeStatusState& state,
                                         const State6OpaqueNotification& update);
using State6OpaqueNotificationHandler =
    std::function<void(const State6OpaqueNotification&)>;
void register_observed_state6_notification_handlers(
    MessageDispatcher& dispatcher, State6OpaqueNotificationHandler consume_notification);

struct State6SpeechMessage {
    std::uint32_t speaker_id{};
    std::string speaker_name;
    std::uint16_t level{};
    std::uint8_t mode{};
    bool has_position{};
    State6WirePosition position;
    std::uint16_t mode_value_u16{};
    std::uint32_t mode_value_u32{};
    std::string text;
};
struct State6ChannelEntry {
    std::uint16_t id{};
    std::string name;
};
struct State6ChannelOpen {
    std::uint16_t id{};
    std::string name;
};
struct State6CommunicationUpdates {
    std::function<void(const State6SpeechMessage&)> speech;
    std::function<void(const std::vector<State6ChannelEntry>&)> channel_list;
    std::function<void(const State6ChannelOpen&)> channel_open;
};
struct State6ChannelEventEffect {
    std::uint8_t source_opcode{};
    std::uint8_t native_event_type{};
    std::vector<State6ChannelEntry> channels;
};
[[nodiscard]] State6ChannelEventEffect
make_observed_state6_channel_list_effect(const std::vector<State6ChannelEntry>& channels);
[[nodiscard]] State6ChannelEventEffect
make_observed_state6_channel_open_effect(const State6ChannelOpen& channel);
void register_observed_state6_communication_handlers(
    MessageDispatcher& dispatcher, State6CommunicationUpdates consumers);

struct State6CommunicationControl {
    std::uint8_t opcode{};
    std::uint16_t field_u16{};
    std::uint8_t field_u8{};
    std::string text;
};
enum class State6CommunicationEffectKind : std::uint8_t {
    enqueue_server_text,
    refresh_channel_buffer,
    remove_console_filter,
    finish_request,
    close_request,
    enqueue_channel_event,
    close_channel,
    route_talk_message,
};
struct State6CommunicationEffect {
    State6CommunicationEffectKind kind{};
    std::uint8_t source_opcode{};
    std::uint8_t native_buffer_type{};
    std::uint16_t native_event_type{};
    std::uint16_t field_u16{};
    bool fallback_to_default_buffer{};
    bool remove_all_matching_filters{};
    bool refresh_after_apply{};
    std::string lookup_or_filter_text;
    std::string displayed_text;
    std::uint8_t talk_mode{};
};
[[nodiscard]] std::optional<State6CommunicationEffect>
make_observed_state6_communication_effect(const State6CommunicationControl& update);
class State6CommunicationEffectQueue {
public:
    void enqueue_channel_list(const std::vector<State6ChannelEntry>& channels);
    void enqueue_channel_open(const State6ChannelOpen& channel);
    void enqueue_control(const State6CommunicationControl& update);
    [[nodiscard]] const std::vector<State6ChannelEventEffect>& channel_events() const noexcept;
    [[nodiscard]] const std::vector<State6CommunicationEffect>& control_events() const noexcept;
    void clear() noexcept;

private:
    std::vector<State6ChannelEventEffect> channel_events_;
    std::vector<State6CommunicationEffect> control_events_;
};
using State6CommunicationControlHandler =
    std::function<void(const State6CommunicationControl&)>;
void register_observed_state6_communication_control_handlers(
    MessageDispatcher& dispatcher, State6CommunicationControlHandler consume_update);
using State6OpcodeB5Handler = std::function<void(std::uint8_t)>;
void register_observed_state6_opcode_b5_handler(
    MessageDispatcher& dispatcher, State6OpcodeB5Handler consume_value);
struct State6OpcodeB5InputEffect {
    std::uint8_t key_state{};
};
[[nodiscard]] State6OpcodeB5InputEffect
make_observed_state6_opcode_b5_input_effect(std::uint8_t value) noexcept;
[[nodiscard]] bool apply_observed_state6_opcode_b5_input(
    State6LocalPlayerMovementGlobals& globals, State6CreatureStateStore& creatures,
    std::uint32_t local_player_id, std::uint8_t key_state) noexcept;

struct State6OutfitEventEffect {
    std::uint16_t native_event_type{0x11};
    std::array<std::uint32_t, 6> header_values{};
    std::vector<std::uint32_t> outfit_values;
    std::vector<std::string> names;
};
[[nodiscard]] std::optional<State6OutfitEventEffect>
make_observed_state6_outfit_event_effect(const State6RecoveredRecord& record);
class State6SessionProjection {
public:
    void apply(const State6OpaqueNotification& update);
    void apply(const State6RecoveredRecord& record);
    [[nodiscard]] const State6NativeStatusState& native_status() const noexcept;
    [[nodiscard]] const State6BuddyStore& buddies() const noexcept;
    [[nodiscard]] const State6QuestState& quests() const noexcept;
    [[nodiscard]] const std::vector<State6NotificationEventEffect>& notification_events() const noexcept;
    [[nodiscard]] const std::vector<State6OutfitEventEffect>& outfit_events() const noexcept;

private:
    State6NativeStatusState native_status_;
    State6BuddyStore buddies_;
    State6QuestState quests_;
    std::vector<State6NotificationEventEffect> notification_events_;
    std::vector<State6OutfitEventEffect> outfit_events_;
};

struct State6ThingMove {
    std::uint16_t old_x{};
    std::uint16_t old_y{};
    std::uint8_t old_floor{};
    std::uint8_t stack_index{};
    std::uint16_t new_x{};
    std::uint16_t new_y{};
    std::uint8_t new_floor{};
};
using State6ThingMoveHandler = std::function<void(const State6ThingMove&)>;
void register_observed_state6_thing_move_handler(
    MessageDispatcher& dispatcher, State6ThingMoveHandler consume_move);
void apply_state6_thing_move(State6DecodedMap& map,
    const resources::TibiaDatFile& dat, const State6ThingMove& move);
[[nodiscard]] bool apply_observed_state6_thing_move(
    State6DecodedMap& map, State6CreatureStateStore& creatures,
    State6LocalPlayerMovementGlobals& movement,
    const resources::TibiaDatFile& dat, std::uint32_t first_timer_sample,
    std::uint32_t second_timer_sample, const State6ThingMove& move);
[[nodiscard]] State6DecodedMap decode_state6_map_description(
    const State6MapDescription& description,
    const std::vector<std::uint16_t>& item_ids_with_extra_byte,
    const resources::TibiaDatFile& dat);
void apply_state6_map_strip(State6DecodedMap& map, const State6MapStrip& strip,
    const resources::TibiaDatFile& dat);
void apply_state6_map_tile_update(State6DecodedMap& map,
    const State6MapTileUpdate& update, const resources::TibiaDatFile& dat);
void apply_state6_map_thing_add(State6DecodedMap& map,
    const resources::TibiaDatFile& dat, const State6MapThingAdd& add);
void apply_state6_map_thing_transform(State6DecodedMap& map,
    const State6MapThingTransform& transform, const resources::TibiaDatFile& dat);
void apply_state6_map_thing_remove(State6DecodedMap& map,
    const State6MapThingRemove& remove, const resources::TibiaDatFile& dat);

using State6MapDescriptionHandler = std::function<std::size_t(const State6MapDescription&)>;
void register_observed_state6_map_description_handler(
    MessageDispatcher& dispatcher,
    State6MapDescriptionHandler consume_map_description);

struct State4Notice {
    std::string_view title;
    std::string message;
    bool performs_followup_cleanup{};
};

using State4NoticeHandler = std::function<void(const State4Notice&)>;
void register_observed_state4_notice_handlers(
    MessageDispatcher& dispatcher,
    State4NoticeHandler present_notice);

using State4TextHandler = std::function<void(std::string_view)>;
void register_observed_state4_text_handler(
    MessageDispatcher& dispatcher,
    StatusLineState& state,
    State4TextHandler consume_text);

using State4Opcode28Handler = std::function<void()>;
void register_observed_state4_opcode_28_handler(
    MessageDispatcher& dispatcher, State4Opcode28Handler consume_action);

enum class State4PatchFile : std::uint8_t {
    tibia_exe = 0,
    tibia_dat = 1,
    tibia_spr = 2,
    tibia_pic = 3,
    patch_exe = 4,
};

enum class PatchPayloadMode : std::uint8_t {
    full_file,
    delta_stream,
};

struct State4PatchUpdate {
    std::uint8_t file_code{};
    std::uint32_t total_size{};
    PatchPayloadMode mode{PatchPayloadMode::full_file};
};

using State4PatchUpdateHandler = std::function<void(const State4PatchUpdate&)>;
void register_observed_state4_patch_update_handler(
    MessageDispatcher& dispatcher,
    State4PatchUpdateHandler consume_update);

struct State4PatchChunk {
    std::span<const std::uint8_t> bytes;
};

using State4PatchChunkHandler = std::function<void(const State4PatchChunk&)>;
void register_observed_state4_patch_chunk_handler(
    MessageDispatcher& dispatcher,
    State4PatchChunkHandler consume_chunk);

}
