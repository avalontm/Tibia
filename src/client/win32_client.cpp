#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <shellapi.h>
#include <bcrypt.h>

#include "render/graphics_renderer.hpp"
#include "render/object_compositor.hpp"
#include "client/communication_session.hpp"
#include "client/console_chat_input.hpp"
#include "client/console_context_menu.hpp"
#include "client/message_dialog_layout.hpp"
#include "client/new_channel_dialog_layout.hpp"
#include "client/client_communication_preferences.hpp"
#include "client/hints_catalog.hpp"
#include "client/hint_markup.hpp"
#include "client/combat_control_layout.hpp"
#include "client/automap_panel_layout.hpp"
#include "client/game_screen_control_layout.hpp"
#include "client/console_options_page.hpp"
#include "client/hotkey_options_page.hpp"
#include "client/advanced_graphics_options_page.hpp"
#include "client/account_management.hpp"
#include "client/general_options_page.hpp"
#include "client/graphics_options_page.hpp"
#include "client/network_options_page.hpp"
#include "client/game_world_snapshot.hpp"
#include "client/quest_log_layout.hpp"
#include "client/ui_scrollbar_layout.hpp"
#include "client/report_violation_dialog.hpp"
#include "client/rule_violation_request_form.hpp"
#include "client/report_violation_buffer.hpp"
#include "client/console_panel_renderer.hpp"
#include "client/game_mouse_cursor.hpp"
#include "client/inventory_layout.hpp"
#include "client/player_status_bars_layout.hpp"
#include "client/inventory_item_visual.hpp"
#include "client/inventory_panel_renderer.hpp"
#include "client/container_panel_renderer.hpp"
#include "client/battle_panel_model.hpp"
#include "client/battle_panel_interaction.hpp"
#include "client/battle_panel_renderer.hpp"
#include "client/battle_panel_window_layout.hpp"
#include "client/content_window_frame_layout.hpp"
#include "client/content_window_state.hpp"
#include "client/content_window_stack_layout.hpp"
#include "client/skills_panel_render_plan.hpp"
#include "client/skills_panel_scroll.hpp"
#include "client/skills_panel_window_layout.hpp"
#include "client/vip_panel_window_layout.hpp"
#include "client/vip_context_menu.hpp"
#include "client/skills_panel_header_layout.hpp"
#include "client/skills_context_menu.hpp"
#include "client/map_window_camera.hpp"
#include "client/map_window_creature_renderer.hpp"
#include "client/map_window_effect_renderer.hpp"
#include "client/map_window_lighting_renderer.hpp"
#include "client/map_window_item_frame.hpp"
#include "client/map_window_layout.hpp"
#include "client/map_window_frame_layout.hpp"
#include "client/map_window_tile_surface_renderer.hpp"
#include "client/talk_system.hpp"
#include "client/talkbox_renderer.hpp"
#include "client/ui_skins_792.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_pic.hpp"
#include "resources/tibia_spr.hpp"
#include "network/login_response.hpp"
#include "network/login_request.hpp"
#include "network/login_session.hpp"
#include "network/packet_stream.hpp"
#include "network/channel_request.hpp"
#include "network/speech_request.hpp"
#include "network/console_filter_request.hpp"
#include "network/rule_violation_request.hpp"
#include "network/encrypted_frame_stream.hpp"
#include "network/message_dispatcher.hpp"
#include "network/status_line_state.hpp"
#include "network/tcp_connection.hpp"
#include "map/window_tile_cache.hpp"

#include <cstdint>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

const std::uint32_t animation_epoch_tick = timeGetTime();

bool contains_point(const RECT& rect, POINT point);

std::wstring decode_ui_string(const std::string& value);
std::wstring error_to_wide(const char* error);

struct Tile {
    std::uint32_t texture{};
    int x{};
    int y{};
};

struct BitmapGlyph {
    std::uint32_t texture{};
    int width{};
    int height{};
    int advance{};
};

struct SkinChunk {
    std::uint32_t texture{};
    int source_x{};
    int source_y{};
    int width{};
    int height{};
};

struct CharacterChoice {
    std::wstring name;
    std::wstring world;
    std::string wire_name;
    std::array<std::uint8_t, 4> server_ipv4{};
    std::uint16_t server_port{};
};

struct LoginUiEvent {
    std::optional<tibia::network::CharacterListResponse> characters;
    std::wstring status;
    bool finished{};
    bool game_session{};
    bool message_of_the_day{};
    std::wstring title;
};

struct LoginMailbox {
    std::mutex mutex;
    std::optional<LoginUiEvent> pending;
};

enum class UiScreen { main_menu, login, character_selection, game_world };

constexpr UINT login_result_message = WM_APP + 17;
constexpr UINT escape_ui_message = WM_APP + 18;
constexpr UINT game_world_message = WM_APP + 19;
constexpr UINT channel_list_message = WM_APP + 20;
constexpr UINT game_frame_timer_id = 0x71;
constexpr UINT game_cycle_timer_id = 1;

struct ConsoleTabUnreadState {
    std::size_t message_count{};
    std::uint64_t flash_until_tick{};
    bool initialized{};
    bool unread{};
};

struct ConsoleTranscriptRow {
    std::wstring text;
    std::uint32_t color{};
    int indent{};
    std::wstring message;
    std::wstring speaker_name;
    std::uint32_t report_statement_id{};
    std::wstring statement_text;
    bool ends_message{};
};

struct ClientHost {
    tibia::render::GraphicsBackend backend{tibia::render::GraphicsBackend::direct3d9};
    std::unique_ptr<tibia::render::GraphicsRenderer> renderer;
    bool renderer_initialized{};
    std::optional<tibia::network::PreparedLogin> prepared_login;
    tibia::network::StatusLineState status_line_state;
    std::vector<std::uint8_t> initial_wire_request;
    std::shared_ptr<tibia::network::TcpConnection> login_connection;
    std::function<void(std::uint8_t)> send_game_command;
    std::function<void(std::span<const std::uint8_t>)> send_game_payload;
    std::function<void(std::uint8_t, std::uint32_t)> send_game_target_command;
    std::function<void(std::uint16_t)> request_quest_flags;
    std::function<void()> request_tutor_help;
    std::function<void(std::wstring_view)> submit_rule_violation;
    std::function<void(std::wstring_view)> send_report_violation_reply;
    std::shared_ptr<tibia::client::ReportViolationBuffer> report_violation_buffer;
    std::shared_ptr<std::atomic_bool> stop_action_reset_pending;
    std::jthread login_thread;
    std::shared_ptr<LoginMailbox> login_mailbox{std::make_shared<LoginMailbox>()};
    std::shared_ptr<tibia::client::GameWorldSnapshotMailbox> game_world_mailbox{
        std::make_shared<tibia::client::GameWorldSnapshotMailbox>()};
    std::shared_ptr<tibia::client::CommunicationSessionMailbox>
        communication_session_mailbox{
            std::make_shared<tibia::client::CommunicationSessionMailbox>()};
    const tibia::resources::TibiaDatFile* dat{};
    const tibia::resources::TibiaSprFile* tibia_spr{};
    const tibia::resources::TibiaPicFile* tibia_pic{};
    const std::vector<std::uint8_t>* tibia_spr_bytes{};
    const std::vector<std::uint8_t>* tibia_pic_bytes{};
    std::unique_ptr<tibia::map::MapWindowTileCache> map_tile_cache;
    std::unique_ptr<tibia::client::MapWindowTileSurfaceRenderer> map_surface_renderer;
    tibia::client::MapWindowCreatureRenderer map_creature_renderer;
    tibia::client::MapWindowEffectRenderer map_effect_renderer;
    tibia::client::MapWindowLightingRenderer map_lighting_renderer;
    tibia::client::TalkboxRenderer talkbox_renderer;
    tibia::client::InventoryPanelRenderer inventory_panel_renderer;
    tibia::client::ContainerPanelRenderer container_panel_renderer;
    std::vector<tibia::client::ContainerPanelState> container_panel_states;
    int pressed_container_header_id{-1};
    int pressed_container_header_button{-1};
    int moving_container_id{-1};
    int container_move_start_y{};
    int container_move_start_position_y{};
    std::uint32_t next_container_z_order{};
    int resizing_container_id{-1};
    int container_resize_start_y{};
    int container_resize_start_height{};
    std::vector<tibia::client::RestoredContentWindow> restored_content_windows;
    tibia::client::BattlePanelModel battle_panel_model;
    std::vector<tibia::client::BattleCreatureRow> battle_panel_rows;
    tibia::client::BattleTargetState battle_targets;
    std::optional<tibia::client::BattleContextMenuPlan> battle_context_menu;
    int pressed_battle_context_menu_action{-1};
    std::uint32_t pressed_battle_creature_id{};
    int battle_scroll_value{};
    int pressed_battle_scrollbar_part{-1};
    bool battle_scrollbar_dragging{};
    int battle_scrollbar_grab_offset{};
    bool battle_window_open{};
    bool battle_window_minimized{};
    int battle_window_restore_height{};
    int battle_window_natural_height{};
    bool battle_window_resize_dragging{};
    int battle_window_resize_start_y{};
    int battle_window_resize_start_height{};
    int battle_window_resized_height{};
    int pressed_battle_header_button{-1};
    bool vip_window_open{};
    bool vip_window_minimized{};
    int vip_window_restore_height{};
    int vip_window_natural_height{};
    bool vip_window_resize_dragging{};
    int vip_window_resize_start_y{};
    int vip_window_resize_start_height{};
    int vip_window_resized_height{};
    int vip_scroll_value{};
    int pressed_vip_scrollbar_part{-1};
    bool vip_scrollbar_dragging{};
    int vip_scrollbar_grab_offset{};
    int pressed_vip_header_button{-1};
    std::optional<tibia::client::VipContextMenuPlan> vip_context_menu;
    int pressed_vip_context_menu_action{-1};
    std::wstring vip_context_player_name;
    tibia::client::SkillsPanelBarVisibility skills_bar_visibility;
    int skills_scroll_value{};
    int pressed_skills_scrollbar_part{-1};
    bool skills_scrollbar_dragging{};
    int skills_scrollbar_grab_offset{};
    bool skills_window_open{};
    bool skills_window_minimized{};
    int skills_window_restore_height{};
    bool skills_window_resize_dragging{};
    int skills_window_resize_start_y{};
    int skills_window_resize_start_height{};
    int skills_window_resized_height{};
    int pressed_skills_header_button{-1};
    std::optional<tibia::client::SkillsContextMenuPlan> skills_context_menu;
    int pressed_skills_context_menu_action{-1};
    bool inventory_minimized{};
    int pressed_inventory_control{-1};
    int pressed_combat_control{-1};
    int pressed_combat_action{-1};
    int pressed_game_screen_control{-1};
    int pressed_automap_control{-1};
    int pressed_automap_compass{-1};
    std::int32_t combat_attack_mode{2};
    std::int32_t combat_chase_mode{};
    bool combat_secure_mode{true};
    std::vector<Tile> tiles;
    std::array<std::array<BitmapGlyph, 224>, 4> ui_fonts{};
    std::array<tibia::client::TalkboxGlyphMetrics, 224> talkbox_font_glyphs{};
    std::array<std::vector<tibia::resources::TibiaSprImage>, 4> ui_font_images;
    std::unordered_map<std::uint32_t, std::uint32_t> map_text_glyph_textures;
    std::unordered_map<std::uint64_t, std::uint32_t> creature_info_glyph_textures;
    std::unordered_map<std::uint32_t, bool> creature_info_solid_textures;
    std::unordered_map<std::uint32_t, std::uint32_t> console_message_glyph_textures;
    std::unordered_map<std::uint64_t, std::uint32_t> hint_text_glyph_textures;
    bool console_timestamps_enabled{};
    bool console_levels_enabled{};
    std::array<int, 4> ui_font_heights{};
    std::array<int, 4> ui_font_cell_widths{};
    std::array<int, 4> ui_font_cell_heights{};
    std::array<int, 4> ui_font_line_advances{12, 15, 10, 14};
    std::array<int, 4> ui_font_missing_widths{12, 15, 10, 14};
    std::array<int, 4> ui_font_spacing{1, -1, 0, 1};
    std::array<std::array<std::array<BitmapGlyph, 224>, 4>, 4> ui_font_tints{};
    std::array<std::vector<SkinChunk>, 267> skins;
    std::array<std::array<std::uint8_t, 4>, 86> login_bevel_top{};
    std::array<std::array<std::uint8_t, 4>, 86> login_bevel_bottom{};
    std::array<std::array<std::uint8_t, 4>, 16> login_bevel_left{};
    std::array<std::array<std::uint8_t, 4>, 16> login_bevel_right{};
    std::vector<std::array<std::uint8_t, 4>> login_bevel_palette;
    std::vector<std::uint16_t> map_item_ids_with_extra_byte;
    std::vector<bool> keys = std::vector<bool>(256);
    POINT mouse{};
    bool mouse_position_known{};
    unsigned mouse_buttons{};
    int wheel_delta{};
    int wheel_remainder{};
    int console_transcript_scroll_from_bottom{};
    int console_transcript_item_count{};
    int console_transcript_visible_rows{};
    std::vector<ConsoleTranscriptRow> console_transcript_rows;
    std::optional<tibia::client::ConsoleContextMenuPlan> console_context_menu;
    int pressed_console_context_menu_action{-1};
    std::wstring console_context_message;
    std::wstring console_context_speaker_name;
    std::wstring console_context_statement;
    std::uint32_t console_context_statement_id{};
    RECT console_transcript_clip{};
    RECT console_transcript_scrollbar_viewport{};
    bool console_transcript_scrollbar_geometry_valid{};
    int console_transcript_line_height{};
    int console_transcript_first_line_height{};
    bool console_transcript_selecting{};
    bool console_transcript_selection_active{};
    int console_transcript_selection_start_row{-1};
    int console_transcript_selection_start_offset{};
    int console_transcript_selection_end_row{-1};
    int console_transcript_selection_end_offset{};
    int pressed_console_transcript_scrollbar_part{-1};
    bool console_transcript_scrollbar_dragging{};
    int console_transcript_scrollbar_grab_offset{};
    ConsoleTabUnreadState default_console_tab_state;
    ConsoleTabUnreadState report_console_tab_state;
    std::unordered_map<std::uint16_t, ConsoleTabUnreadState> channel_console_tab_states;
    bool report_violation_tab_selected{};
    bool report_violation_buffer_was_open{};
    int channel_tab_scroll_offset{};
    std::uint8_t console_volume_state{};
    std::wstring typed_text;
    std::wstring console_input_text;
    std::size_t console_input_caret{};
    std::size_t console_input_anchor{};
    bool console_input_focused{};
    std::wstring login_account_text;
    std::wstring login_password_text;
    std::wstring connection_status_text;
    std::wstring notice_text;
    std::wstring notice_title{L"Tibia"};
    POINT notice_dialog_offset{};
    int pressed_notice_button{-1};
    std::wstring resource_summary;
    HWND account_edit{};
    HWND password_edit{};
    HWND account_label{};
    HWND password_label{};
    HWND header_label{};
    HWND character_heading{};
    HWND account_status_label{};
    HWND validation_label{};
    HWND login_button{};
    HWND character_list{};
    HWND connect_button{};
    HWND back_button{};
    std::array<HWND, 5> menu_buttons{};
    HWND options_heading{};
    HWND engine_label{};
    std::array<HWND, 3> engine_radios{};
    HWND ambient_label{};
    HWND ambient_slider{};
    HWND light_effects_check{};
    HWND stretch_check{};
    HWND pointer_check{};
    HWND framerate_label{};
    HWND framerate_slider{};
    HWND current_framerate_label{};
    HWND options_help_button{};
    HWND options_ok_button{};
    HWND options_cancel_button{};
    HFONT ui_font{};
    HFONT ui_title_font{};
    HFONT ui_menu_font{};
    HBRUSH ui_edit_brush{};
    std::wstring login_server{L"127.0.0.1"};
    unsigned login_port{7171};
    bool test_server_login{};
    std::vector<CharacterChoice> characters;
    int selected_character{-1};
    int character_list_scroll{};
    int pressed_character_scroll_button{-1};
    POINT character_selection_dialog_offset{};
    std::uint16_t account_status{};
    UiScreen screen{UiScreen::main_menu};
    bool options_open{};
    bool options_from_game{};
    bool console_options_open{};
    bool hotkey_options_open{};
    bool general_options_open{};
    bool network_options_open{};
    bool graphics_options_open{};
    bool advanced_graphics_options_open{};
    bool info_open{};
    bool help_open{};
    bool hints_open{};
    bool hint_content_open{};
    std::vector<tibia::client::AvailableHint> hints;
    std::vector<tibia::client::HintLine> hint_content_lines;
    int hints_selected{-1};
    int hints_scroll{};
    int hint_content_scroll{};
    int pressed_hints_button{-1};
    int pressed_hint_content_button{-1};
    int pressed_hints_scroll_part{-1};
    int pressed_hint_content_scroll_part{-1};
    bool hints_scroll_dragging{};
    bool hint_content_scroll_dragging{};
    int hints_scroll_grab_offset{};
    int hint_content_scroll_grab_offset{};
    const tibia::client::AvailableHint* active_hint{};
    tibia::client::ReportViolationDialogState report_violation_dialog;
    tibia::client::RuleViolationRequestForm rule_violation_request_form;
    int rule_violation_request_reason_scroll{};
    int rule_violation_request_action_scroll{};
    int pressed_rule_violation_request_button{-1};
    bool pressed_rule_violation_request_checkbox{};
    int rule_violation_request_focus_field{-1};
    POINT rule_violation_request_dialog_offset{};
    bool quest_log_open{};
    bool quest_line_open{};
    bool quest_request_sent{};
    std::uint16_t quest_line_id{};
    std::optional<std::uint16_t> last_quest_flags_request_id;
    std::uint64_t quest_line_request_revision{};
    bool quest_line_flags_pending{};
    int quest_log_selected{-1};
    int quest_log_scroll{};
    int quest_flag_selected{-1};
    int quest_flag_scroll{};
    int pressed_quest_scroll_button{-1};
    bool quest_scroll_dragging{};
    bool quest_scroll_drag_on_flags{};
    int quest_scroll_drag_grab_offset{};
    int pressed_quest_log_button{-1};
    int pressed_quest_line_button{-1};
    bool motd_open{};
    int pressed_help_button{-1};
    std::wstring motd_text;
    int pressed_motd_button{-1};
    bool account_data_warning_open{};
    int pressed_account_data_warning_button{-1};
    bool new_channel_dialog_open{};
    std::vector<tibia::network::State6ChannelEntry> available_channels;
    int new_channel_selected{-1};
    int new_channel_scroll{};
    int pressed_new_channel_scroll_button{-1};
    bool new_channel_scrollbar_dragging{};
    int new_channel_scrollbar_grab_offset{};
    std::wstring private_channel_name;
    bool private_channel_focused{};
    int pressed_new_channel_button{-1};
    POINT new_channel_dialog_offset{};
    bool show_light_effects{true};
    bool show_textual_effects{true};
    std::int32_t creature_info_mode{2};
    bool no_stretching{};
    bool windows_mouse_pointer{true};
    int ambient_light{100};
    int framerate_limit{60};
    std::uint32_t animation_clock{};
    tibia::render::GraphicsBackend options_backend{tibia::render::GraphicsBackend::direct3d9};
    HWND window{};
    std::filesystem::path communication_config_path;
    std::int32_t console_height{};
    bool console_resize_dragging{};
    bool dragging{};
    enum class DraggedDialog { none, login, character_selection, options, info, help, hints, hint_content, quest_log, quest_line, console_options, hotkey_options, general_options, network_options, graphics_options, advanced_graphics_options, notice, motd, account_data_warning, report_rule_violation, rule_violation_request, new_channel } dragged_dialog{DraggedDialog::none};
    POINT login_dialog_offset{};
    POINT options_dialog_offset{};
    POINT info_dialog_offset{};
    POINT help_dialog_offset{};
    POINT hints_dialog_offset{};
    POINT hint_content_dialog_offset{};
    POINT quest_log_dialog_offset{};
    POINT quest_line_dialog_offset{};
    POINT console_options_dialog_offset{};
    POINT hotkey_options_dialog_offset{};
    POINT general_options_dialog_offset{};
    POINT network_options_dialog_offset{};
    POINT graphics_options_dialog_offset{};
    POINT advanced_graphics_options_dialog_offset{};
    POINT motd_dialog_offset{};
    POINT account_data_warning_dialog_offset{};
    POINT report_rule_violation_dialog_offset{};
    POINT dialog_drag_anchor{};
    POINT dialog_drag_start_offset{};
    int pressed_menu_index{-1};
    int login_focus_field{};
    int pressed_login_button{-1};
    int pressed_info_button{-1};
    int pressed_options_button{-1};
    int pressed_console_options_control{-1};
    int pressed_hotkey_options_control{-1};
    enum class ListScrollbarTarget {
        none,
        graphics_modes,
        hotkeys,
        rule_violation_reasons,
        rule_violation_actions
    };
    ListScrollbarTarget list_scrollbar_target{ListScrollbarTarget::none};
    int pressed_list_scrollbar_part{-1};
    bool list_scrollbar_dragging{};
    int list_scrollbar_grab_offset{};
    std::array<tibia::client::HotkeyPreference, tibia::client::hotkey_count> hotkey_options_values{};
    std::size_t selected_hotkey{};
    std::size_t hotkey_list_scroll{};
    std::optional<std::size_t> pending_hotkey_object_selection;
    std::uint8_t interaction_mode{};
    std::wstring hotkey_text;
    bool hotkey_send_automatically{};
    bool hotkey_text_focused{};
    std::int32_t hotkey_object_preview_id{-1};
    int pressed_general_options_control{-1};
    int pressed_network_options_control{-1};
    int pressed_graphics_options_control{-1};
    int pressed_advanced_graphics_control{-1};
    std::array<bool, 6> console_options_values{};
    std::array<bool, 5> general_options_values{};
    bool network_options_value{};
    bool graphics_fullscreen_value{};
    bool graphics_initial_fullscreen{};
    int graphics_initial_selected_mode{-1};
    bool fullscreen_window{};
    bool windowed_state_saved{};
    bool fullscreen_display_mode_saved{};
    LONG_PTR windowed_style{};
    LONG_PTR windowed_extended_style{};
    RECT windowed_window_rect{};
    DEVMODEW original_display_mode{};
    std::vector<tibia::client::GraphicsDisplayMode> graphics_display_modes;
    int graphics_selected_mode{-1};
    int graphics_mode_scroll{};
    int advanced_graphics_engine{-1};
    int advanced_graphics_brightness{32};
    int advanced_graphics_framerate{201};
    int advanced_graphics_original_framerate{201};
    bool advanced_graphics_light_effects{true};
    bool advanced_graphics_no_stretching{};
    bool advanced_graphics_windows_mouse_pointer{true};
    std::chrono::steady_clock::time_point framerate_measurement_start{};
    int framerate_measurement_frames{};
    float measured_framerate{};
    int login_caret_phase{-1};
};

enum class ActiveDialog {
    connection_status,
    none,
    motd,
    account_data_warning,
    new_channel,
    hint_content,
    hints,
    quest_line,
    quest_log,
    notice,
    rule_violation_request,
    report_violation,
    advanced_graphics_options,
    general_options,
    network_options,
    graphics_options,
    console_options,
    hotkey_options,
    info,
    help,
    options
};

ActiveDialog active_dialog(const ClientHost& host) {
    if (!host.connection_status_text.empty()) return ActiveDialog::connection_status;
    if (!host.notice_text.empty()) return ActiveDialog::notice;
    if (host.motd_open) return ActiveDialog::motd;
    if (host.account_data_warning_open) return ActiveDialog::account_data_warning;
    if (host.new_channel_dialog_open) return ActiveDialog::new_channel;
    if (host.hint_content_open) return ActiveDialog::hint_content;
    if (host.hints_open) return ActiveDialog::hints;
    if (host.quest_line_open) return ActiveDialog::quest_line;
    if (host.quest_log_open) return ActiveDialog::quest_log;
    if (host.rule_violation_request_form.open)
        return ActiveDialog::rule_violation_request;
    if (host.report_violation_dialog.open) return ActiveDialog::report_violation;
    if (host.advanced_graphics_options_open) return ActiveDialog::advanced_graphics_options;
    if (host.general_options_open) return ActiveDialog::general_options;
    if (host.network_options_open) return ActiveDialog::network_options;
    if (host.graphics_options_open) return ActiveDialog::graphics_options;
    if (host.console_options_open) return ActiveDialog::console_options;
    if (host.hotkey_options_open) return ActiveDialog::hotkey_options;
    if (host.info_open) return ActiveDialog::info;
    if (host.help_open) return ActiveDialog::help;
    if (host.options_open) return ActiveDialog::options;
    return ActiveDialog::none;
}

void show_notice(ClientHost& host, std::wstring_view title, std::wstring_view text) {
    host.notice_title.assign(title);
    host.notice_text.assign(text);
    host.notice_dialog_offset = {};
    host.pressed_notice_button = -1;
    if (host.validation_label) SetWindowTextW(host.validation_label, L"");
    if (host.window) InvalidateRect(host.window, nullptr, FALSE);
}

std::string encode_ui_string(std::wstring_view value);

void upload_hotkey_object_preview(ClientHost& host, std::int32_t object_id);
void draw_ui_scrollbar_parts(ClientHost& host, const RECT& list,
    const tibia::client::UiScrollbarGeometry& geometry,
    bool pressed_up, bool pressed_down);

constexpr int login_button_id = 1003;
constexpr int back_button_id = 1007;
constexpr std::array<int, 5> menu_button_ids{1101, 1102, 1103, 1104, 1105};
constexpr int engine_directdraw_id = 1201;
constexpr int engine_d3d9_id = 1202;
constexpr int engine_opengl_id = 1203;
constexpr int ambient_slider_id = 1204;
constexpr int light_effects_id = 1205;
constexpr int stretch_game_id = 1206;
constexpr int pointer_id = 1207;
constexpr int framerate_slider_id = 1208;
constexpr int options_help_id = 1209;
constexpr int options_ok_id = 1210;
constexpr int options_cancel_id = 1211;
constexpr std::uint32_t ui_glyph_texture_base = 0xf2000000U;
constexpr std::uint32_t ui_skin_texture_base = 0xf3000000U;
constexpr std::uint32_t ui_tinted_glyph_texture_base = 0xf4000000U;
constexpr UINT_PTR login_caret_timer_id = 0x5449;
constexpr std::uint32_t login_field_background_texture = 0xf5000001U;
constexpr std::uint32_t login_caret_texture = 0xf5000003U;
constexpr std::uint32_t ui_separator_shadow_texture = 0xf5000004U;
constexpr std::uint32_t ui_separator_highlight_texture = 0xf5000005U;
constexpr std::uint32_t graphics_list_background_texture = 0xf5000006U;
constexpr std::uint32_t graphics_list_selection_texture = 0xf5000007U;
constexpr std::uint32_t hotkey_object_preview_texture = 0xf5000008U;
constexpr std::uint32_t console_input_focused_texture = 0xf5000009U;
constexpr std::uint32_t console_input_selection_texture = 0xf500000aU;
constexpr std::uint32_t skills_bar_track_texture = 0xf500000bU;
constexpr std::uint32_t skills_bar_red_texture = 0xf500000cU;
constexpr std::uint32_t skills_bar_green_texture = 0xf500000dU;
constexpr std::uint32_t skills_bar_yellow_texture = 0xf500000eU;
constexpr std::uint32_t skills_bar_orange_texture = 0xf500000fU;
constexpr std::uint32_t skills_separator_texture = 0xf5000010U;
constexpr std::uint32_t skills_menu_selection_texture = 0xf5000011U;
constexpr std::uint32_t battle_health_track_texture = 0xf5000012U;
constexpr std::uint32_t battle_hover_frame_texture = 0xf5000013U;
constexpr std::uint32_t battle_attack_frame_texture = 0xf5000014U;
constexpr std::uint32_t battle_follow_frame_texture = 0xf5000015U;
constexpr std::uint32_t battle_attack_hover_frame_texture = 0xf5000016U;
constexpr std::uint32_t battle_follow_hover_frame_texture = 0xf5000017U;
constexpr std::uint32_t automap_empty_viewport_texture = 0xf5000018U;
constexpr std::uint32_t battle_health_color_texture_base = 0xf5000020U;
constexpr std::uint32_t battle_creature_texture_base = 0xe7000000U;
constexpr std::uint32_t login_bevel_texture_base = 0xf6000000U;
constexpr std::uint32_t map_text_texture_base = 0xf7000000U;
constexpr std::uint32_t console_message_texture_base = 0xf8000000U;
constexpr std::uint32_t hint_text_texture_base = 0xf9000000U;
constexpr std::uint32_t creature_info_texture_base = 0xfa000000U;
constexpr std::uint32_t creature_info_solid_texture_base = 0xfb000000U;
constexpr std::array<std::uint8_t, 4> ui_font_text_shades{144, 176, 192, 224};

float ui_scale(HWND window);

RECT panel_rect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const float scale = ui_scale(window);
    const auto actual_width = std::min(static_cast<int>(std::lround(520.0F * scale)),
        std::max(1, client_width - 24));
    const auto actual_height = std::min(static_cast<int>(std::lround(340.0F * scale)),
        std::max(1, client_height - 24));
    const auto left = std::max(12, (client_width - actual_width) / 2);
    const auto top = std::max(12, (client_height - actual_height) / 2);
    return {left, top, left + actual_width, top + actual_height};
}

float ui_scale(HWND) {
    return 1.0F;
}

RECT menu_rect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const float scale = ui_scale(window);
    const int width = static_cast<int>(std::lround(116.0F * scale));
    const int height = static_cast<int>(std::lround(170.0F * scale));
    const int left = static_cast<int>(std::lround(62.0F * scale));
    const int bottom_margin = static_cast<int>(std::lround(70.0F * scale));
    const int client_height = static_cast<int>(client.bottom - client.top);
    return {left, client_height - bottom_margin - height,
        left + width, client_height - bottom_margin};
}

RECT clamp_dialog_rect(HWND window, RECT rect, POINT offset) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int max_left = std::max(0, static_cast<int>(client.right - client.left) - width);
    const int max_top = std::max(0, static_cast<int>(client.bottom - client.top) - height);
    rect.left = std::clamp<int>(static_cast<int>(rect.left) + static_cast<int>(offset.x), 0, max_left);
    rect.top = std::clamp<int>(static_cast<int>(rect.top) + static_cast<int>(offset.y), 0, max_top);
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
    return rect;
}

int ui_units(HWND window, int value);

RECT options_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const float scale = ui_scale(window);
    // Main Options overview measured from the 7.92 reference capture.
    const int width = static_cast<int>(std::lround(286.0F * scale));
    const int height = static_cast<int>(std::lround(293.0F * scale));
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT console_options_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, tibia::client::console_options_dialog_width);
    const int height = ui_units(window, tibia::client::console_options_dialog_height);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT general_options_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, tibia::client::general_options_dialog_width);
    const int height = ui_units(window, tibia::client::general_options_dialog_height);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT general_options_row_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = general_options_dialog_rect(window, offset);
    const int x = dialog.left + ui_units(window, tibia::client::general_options_content_x);
    const int y = dialog.top + ui_units(window, tibia::client::general_options_content_y +
        tibia::client::general_options_row_y[index]);
    return {x, y, x + ui_units(window, tibia::client::general_options_content_width),
        y + ui_units(window, tibia::client::general_options_row_height)};
}

RECT general_options_button_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = general_options_dialog_rect(window, offset);
    const int x = tibia::client::general_options_dialog_width - 13 - 43 - 53 +
        static_cast<int>(index) * 53;
    const int y = tibia::client::general_options_dialog_height - 30;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + 43), dialog.top + ui_units(window, y + 20)};
}

RECT network_options_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, tibia::client::network_options_dialog_width);
    const int height = ui_units(window, tibia::client::network_options_dialog_height);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT graphics_options_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, tibia::client::graphics_options_dialog_width);
    const int height = ui_units(window, tibia::client::graphics_options_dialog_height);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT graphics_options_fullscreen_rect(HWND window, POINT offset) {
    const auto dialog = graphics_options_dialog_rect(window, offset);
    const int x = dialog.left + ui_units(window, tibia::client::graphics_options_content_x);
    const int y = dialog.top + ui_units(window, tibia::client::graphics_options_content_y +
        tibia::client::graphics_options_fullscreen_y);
    return {x, y, x + ui_units(window, tibia::client::graphics_options_content_width),
        y + ui_units(window, 22)};
}

RECT graphics_options_list_rect(HWND window, POINT offset) {
    const auto dialog = graphics_options_dialog_rect(window, offset);
    const int x = dialog.left + ui_units(window, tibia::client::graphics_options_content_x);
    const int y = dialog.top + ui_units(window, tibia::client::graphics_options_content_y +
        tibia::client::graphics_options_list_y);
    return {x, y, x + ui_units(window, tibia::client::graphics_options_content_width),
        y + ui_units(window, tibia::client::graphics_options_list_height)};
}

RECT graphics_options_advanced_rect(HWND window, POINT offset) {
    const auto dialog = graphics_options_dialog_rect(window, offset);
    const int x = dialog.left + ui_units(window, tibia::client::graphics_options_content_x +
        tibia::client::graphics_options_button_x);
    const int y = dialog.top + ui_units(window, tibia::client::graphics_options_content_y +
        tibia::client::graphics_options_advanced_y);
    return {x, y, x + ui_units(window, tibia::client::graphics_options_button_width),
        y + ui_units(window, 20)};
}

RECT graphics_options_button_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = graphics_options_dialog_rect(window, offset);
    const int x = tibia::client::graphics_options_dialog_width - 13 - 43 - 106 +
        static_cast<int>(index) * 53;
    const int y = tibia::client::graphics_options_button_y;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + 43), dialog.top + ui_units(window, y + 20)};
}

RECT advanced_graphics_options_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, tibia::client::advanced_graphics_dialog_width);
    const int height = ui_units(window, tibia::client::advanced_graphics_dialog_height);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT advanced_graphics_engine_button_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = advanced_graphics_options_dialog_rect(window, offset);
    const int x = tibia::client::advanced_graphics_content_x +
        static_cast<int>(index) * (tibia::client::advanced_graphics_engine_button_width +
            tibia::client::advanced_graphics_engine_button_gap);
    const int y = tibia::client::advanced_graphics_content_y +
        tibia::client::advanced_graphics_engine_buttons_y;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + tibia::client::advanced_graphics_engine_button_width),
        dialog.top + ui_units(window, y + 20)};
}

RECT advanced_graphics_slider_rect(HWND window, POINT offset, int y) {
    const auto dialog = advanced_graphics_options_dialog_rect(window, offset);
    const int x = tibia::client::advanced_graphics_content_x;
    y += tibia::client::advanced_graphics_content_y;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + tibia::client::advanced_graphics_content_width),
        dialog.top + ui_units(window, y + 12)};
}

RECT advanced_graphics_checkbox_rect(HWND window, POINT offset, int y) {
    const auto dialog = advanced_graphics_options_dialog_rect(window, offset);
    const int x = tibia::client::advanced_graphics_content_x;
    y += tibia::client::advanced_graphics_content_y;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + tibia::client::advanced_graphics_content_width),
        dialog.top + ui_units(window, y + 22)};
}

RECT advanced_graphics_button_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = advanced_graphics_options_dialog_rect(window, offset);
    const int x = tibia::client::advanced_graphics_dialog_width - 13 -
        tibia::client::advanced_graphics_button_width - 106 +
        static_cast<int>(index) * (tibia::client::advanced_graphics_button_width +
            tibia::client::advanced_graphics_button_gap);
    const int y = tibia::client::advanced_graphics_button_y;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + tibia::client::advanced_graphics_button_width),
        dialog.top + ui_units(window, y + 20)};
}

std::vector<tibia::client::GraphicsDisplayMode> enumerate_graphics_display_modes() {
    std::vector<tibia::client::GraphicsDisplayMode> modes;
    for (DWORD index = 0;; ++index) {
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (!EnumDisplaySettingsW(nullptr, index, &mode)) break;
        if (mode.dmPelsWidth <= 549 || mode.dmPelsHeight <= 449) continue;
        tibia::client::GraphicsDisplayMode entry;
        entry.width = static_cast<int>(mode.dmPelsWidth);
        entry.height = static_cast<int>(mode.dmPelsHeight);
        entry.bits_per_pixel = static_cast<int>(mode.dmBitsPerPel);
        entry.refresh_rate = static_cast<int>(mode.dmDisplayFrequency);
        wchar_t label[96]{};
        if (entry.refresh_rate != 0) {
            swprintf_s(label, L"%d*%d, %d Bits, %d Hz", entry.width, entry.height,
                entry.bits_per_pixel, entry.refresh_rate);
        } else {
            swprintf_s(label, L"%d*%d, %d Bits", entry.width, entry.height,
                entry.bits_per_pixel);
        }
        entry.label = label;
        modes.push_back(std::move(entry));
    }
    return modes;
}

void load_graphics_options(ClientHost& host) {
    host.graphics_display_modes = enumerate_graphics_display_modes();
    host.graphics_mode_scroll = 0;
    host.graphics_selected_mode = host.graphics_display_modes.empty() ? -1 : 0;
    const auto preferences = tibia::client::read_communication_preferences(
        host.communication_config_path);
    host.graphics_fullscreen_value = host.fullscreen_window;
    bool selected = false;
    if (preferences.fullscreen_mode_configured) {
        for (std::size_t index = 0; index < host.graphics_display_modes.size(); ++index) {
            const auto& mode = host.graphics_display_modes[index];
            if (mode.width == preferences.fullscreen_mode[0] &&
                mode.height == preferences.fullscreen_mode[1] &&
                mode.bits_per_pixel == preferences.fullscreen_mode[2]) {
                host.graphics_selected_mode = static_cast<int>(index);
                selected = true;
                break;
            }
        }
    }
    DEVMODEW current{};
    current.dmSize = sizeof(current);
    if (!selected && EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &current)) {
        for (std::size_t index = 0; index < host.graphics_display_modes.size(); ++index) {
            const auto& mode = host.graphics_display_modes[index];
            if (mode.width == static_cast<int>(current.dmPelsWidth) &&
                mode.height == static_cast<int>(current.dmPelsHeight) &&
                mode.bits_per_pixel == static_cast<int>(current.dmBitsPerPel)) {
                host.graphics_selected_mode = static_cast<int>(index);
                selected = true;
                break;
            }
        }
    }
    host.graphics_initial_fullscreen = host.graphics_fullscreen_value;
    host.graphics_initial_selected_mode = host.graphics_selected_mode;
}

void load_advanced_graphics_options(ClientHost& host) {
    const auto preferences = tibia::client::read_communication_preferences(
        host.communication_config_path);
    host.advanced_graphics_engine = preferences.engine;
    if (host.advanced_graphics_engine < 0 || host.advanced_graphics_engine > 2) {
        host.advanced_graphics_engine = host.backend == tibia::render::GraphicsBackend::direct_draw
            ? 0 : host.backend == tibia::render::GraphicsBackend::direct3d9 ? 2 : 1;
    }
    host.advanced_graphics_brightness = std::clamp(preferences.basic_brightness, 0, 128);
    host.advanced_graphics_light_effects = preferences.light_effects;
    host.advanced_graphics_no_stretching = preferences.no_stretching;
    host.advanced_graphics_windows_mouse_pointer = preferences.windows_mouse_pointer;
    host.advanced_graphics_original_framerate = preferences.confine_framerate <
        tibia::client::advanced_graphics_slider_minimum
        ? tibia::client::advanced_graphics_slider_maximum
        : std::clamp(preferences.confine_framerate,
            tibia::client::advanced_graphics_slider_minimum,
            tibia::client::advanced_graphics_slider_maximum);
    host.advanced_graphics_framerate = host.advanced_graphics_original_framerate;
    host.advanced_graphics_options_dialog_offset = {};
    host.advanced_graphics_options_open = true;
}

const wchar_t* graphics_backend_name(tibia::render::GraphicsBackend backend) {
    switch (backend) {
    case tibia::render::GraphicsBackend::direct_draw: return L"DirectDraw (software)";
    case tibia::render::GraphicsBackend::direct3d9: return L"DirectX 9";
    case tibia::render::GraphicsBackend::open_gl: return L"OpenGL";
    }
    return L"Unknown";
}

void invalidate_map_graphics_cache(ClientHost& host) noexcept {
    if (host.map_tile_cache) host.map_tile_cache->invalidate();
    if (host.map_surface_renderer) host.map_surface_renderer->invalidate();
}

void activate_graphics_backend(ClientHost& host, tibia::render::GraphicsBackend backend) {
    if (!host.renderer || !host.renderer_initialized)
        throw std::logic_error("graphics renderer is not initialized");
    if (host.renderer->backend() == backend) {
        host.backend = backend;
        return;
    }
    RECT client{};
    if (!GetClientRect(host.window, &client))
        throw std::runtime_error("could not query client area for graphics engine switch");
    const auto width = static_cast<std::uint32_t>(client.right - client.left);
    const auto height = static_cast<std::uint32_t>(client.bottom - client.top);
    try {
        host.renderer->initialize(backend, host.window, width, height);
    } catch (...) {
        host.backend = host.renderer->backend();
        throw;
    }
    host.backend = backend;
    invalidate_map_graphics_cache(host);
    OutputDebugStringW((std::wstring(L"Tibia graphics engine active: ") +
        graphics_backend_name(backend) + L"\n").c_str());
}

void select_graphics_backend(ClientHost& host, tibia::render::GraphicsBackend backend) {
    const auto previous_backend = host.renderer->backend();
    activate_graphics_backend(host, backend);
    try {
        auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        preferences.engine = static_cast<int>(backend);
        tibia::client::write_communication_preferences(
            host.communication_config_path, preferences);
    } catch (...) {
        if (host.renderer->backend() != previous_backend) {
            try { activate_graphics_backend(host, previous_backend); }
            catch (...) {}
        }
        throw;
    }
    host.advanced_graphics_engine = static_cast<int>(backend);
}

void update_advanced_graphics_slider(ClientHost& host, int control_id, int mouse_x) {
    const int y = control_id == 3
        ? tibia::client::advanced_graphics_ambient_slider_y
        : tibia::client::advanced_graphics_framerate_slider_y;
    const auto slider = advanced_graphics_slider_rect(host.window,
        host.advanced_graphics_options_dialog_offset, y);
    const int edge = ui_units(host.window, 12) + ui_units(host.window, 16);
    const int minimum_x = static_cast<int>(slider.left) + edge;
    const int maximum_x = std::max(minimum_x, static_cast<int>(slider.right) - edge);
    const float ratio = maximum_x == minimum_x ? 0.0F :
        static_cast<float>(std::clamp(mouse_x, minimum_x, maximum_x) - minimum_x) /
        static_cast<float>(maximum_x - minimum_x);
    if (control_id == 3) {
        host.advanced_graphics_brightness = static_cast<int>(std::lround(ratio * 128.0F));
    } else {
        host.advanced_graphics_framerate = static_cast<int>(std::lround(
            tibia::client::advanced_graphics_slider_minimum + ratio *
            (tibia::client::advanced_graphics_slider_maximum -
                tibia::client::advanced_graphics_slider_minimum)));
        host.framerate_limit = host.advanced_graphics_framerate;
    }
}

void set_fullscreen_display_mode(const tibia::client::GraphicsDisplayMode& mode) {
    DEVMODEW display{};
    display.dmSize = sizeof(display);
    display.dmPelsWidth = static_cast<DWORD>(mode.width);
    display.dmPelsHeight = static_cast<DWORD>(mode.height);
    display.dmBitsPerPel = static_cast<DWORD>(mode.bits_per_pixel);
    display.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    if (mode.refresh_rate > 1) {
        display.dmDisplayFrequency = static_cast<DWORD>(mode.refresh_rate);
        display.dmFields |= DM_DISPLAYFREQUENCY;
    }
    const LONG result = ChangeDisplaySettingsW(&display, CDS_FULLSCREEN);
    if (result != DISP_CHANGE_SUCCESSFUL)
        throw std::runtime_error("Windows rejected the selected fullscreen display mode");
}

void set_window_fullscreen(ClientHost& host, bool fullscreen,
    const tibia::client::GraphicsDisplayMode* mode) {
    if (!host.window) throw std::runtime_error("client window is unavailable");
    if (fullscreen) {
        if (!mode) throw std::runtime_error("no display mode is selected");
        if (!host.fullscreen_window) {
            host.windowed_style = GetWindowLongPtrW(host.window, GWL_STYLE);
            host.windowed_extended_style = GetWindowLongPtrW(host.window, GWL_EXSTYLE);
            if (!GetWindowRect(host.window, &host.windowed_window_rect))
                throw std::runtime_error("cannot save the windowed client position");
            host.windowed_state_saved = true;
            host.original_display_mode = {};
            host.original_display_mode.dmSize = sizeof(host.original_display_mode);
            host.fullscreen_display_mode_saved = EnumDisplaySettingsW(nullptr,
                ENUM_CURRENT_SETTINGS, &host.original_display_mode) != FALSE;
            if (!host.fullscreen_display_mode_saved)
                throw std::runtime_error("cannot save the current display mode");
        }
        set_fullscreen_display_mode(*mode);
        MONITORINFO monitor{sizeof(monitor)};
        const HMONITOR handle = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
        if (!GetMonitorInfoW(handle, &monitor)) {
            ChangeDisplaySettingsW(nullptr, 0);
            throw std::runtime_error("cannot read the fullscreen monitor bounds");
        }
        SetWindowLongPtrW(host.window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtrW(host.window, GWL_EXSTYLE, 0);
        if (!SetWindowPos(host.window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left,
            monitor.rcMonitor.bottom - monitor.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOACTIVATE)) {
            ChangeDisplaySettingsW(nullptr, 0);
            throw std::runtime_error("cannot resize the client to fullscreen");
        }
        host.fullscreen_window = true;
        return;
    }
    if (!host.fullscreen_window) return;
    const LONG restore_result = host.fullscreen_display_mode_saved
        ? ChangeDisplaySettingsW(&host.original_display_mode, 0)
        : ChangeDisplaySettingsW(nullptr, 0);
    if (restore_result != DISP_CHANGE_SUCCESSFUL)
        throw std::runtime_error("cannot restore the desktop display mode");
    if (host.windowed_state_saved) {
        SetWindowLongPtrW(host.window, GWL_STYLE, host.windowed_style);
        SetWindowLongPtrW(host.window, GWL_EXSTYLE, host.windowed_extended_style);
        if (!SetWindowPos(host.window, nullptr, host.windowed_window_rect.left,
            host.windowed_window_rect.top,
            host.windowed_window_rect.right - host.windowed_window_rect.left,
            host.windowed_window_rect.bottom - host.windowed_window_rect.top,
            SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE))
            throw std::runtime_error("cannot restore the windowed client position");
    }
    host.fullscreen_window = false;
    host.fullscreen_display_mode_saved = false;
    host.windowed_state_saved = false;
}

RECT network_options_row_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = network_options_dialog_rect(window, offset);
    const int x = dialog.left + ui_units(window, tibia::client::network_options_content_x);
    const int y = dialog.top + ui_units(window, tibia::client::network_options_content_y +
        tibia::client::network_options_row_y[index]);
    return {x, y, x + ui_units(window, tibia::client::network_options_content_width),
        y + ui_units(window, tibia::client::network_options_row_height)};
}

RECT network_options_button_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = network_options_dialog_rect(window, offset);
    const int x = tibia::client::network_options_dialog_width - 13 - 43 - 106 +
        static_cast<int>(index) * 53;
    const int y = tibia::client::network_options_dialog_height - 30;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + 43), dialog.top + ui_units(window, y + 20)};
}

RECT console_options_row_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = console_options_dialog_rect(window, offset);
    const auto y = dialog.top + ui_units(window, tibia::client::console_options_content_y +
        tibia::client::console_options_row_y[index]);
    const auto x = dialog.left + ui_units(window, tibia::client::console_options_content_x);
    return {x, y, x + ui_units(window, tibia::client::console_options_content_width),
        y + ui_units(window, tibia::client::console_options_row_height)};
}

RECT console_options_button_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = console_options_dialog_rect(window, offset);
    const int x = tibia::client::console_options_dialog_width - 13 - 43 - 106 +
        static_cast<int>(index) * 53;
    const int y = tibia::client::console_options_dialog_height - 30;
    return {dialog.left + ui_units(window, x), dialog.top + ui_units(window, y),
        dialog.left + ui_units(window, x + 43), dialog.top + ui_units(window, y + 20)};
}

RECT hotkey_options_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, tibia::client::hotkey_options_dialog_width);
    const int height = ui_units(window, tibia::client::hotkey_options_dialog_height);
    const int left = (static_cast<int>(client.right) - width) / 2;
    const int top = (static_cast<int>(client.bottom) - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT hotkey_options_list_rect(HWND window, POINT offset) {
    const auto dialog = hotkey_options_dialog_rect(window, offset);
    const int x = dialog.left + ui_units(window, tibia::client::hotkey_options_content_x);
    const int y = dialog.top + ui_units(window, tibia::client::hotkey_options_content_y +
        tibia::client::hotkey_list_y);
    return {x, y, x + ui_units(window, tibia::client::hotkey_options_content_width),
        y + ui_units(window, tibia::client::hotkey_list_height)};
}

RECT hotkey_options_control_rect(HWND window, POINT offset, int x, int y,
    int width, int height) {
    const auto dialog = hotkey_options_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window,
        tibia::client::hotkey_options_content_x + x);
    const int top = dialog.top + ui_units(window,
        tibia::client::hotkey_options_content_y + y);
    const int right = width == 0
        ? dialog.right - ui_units(window, tibia::client::hotkey_options_content_x)
        : left + ui_units(window, width);
    return {left, top, right, top + ui_units(window, height)};
}

RECT hotkey_options_footer_button_rect(HWND window, POINT offset, std::size_t index) {
    const auto dialog = hotkey_options_dialog_rect(window, offset);
    const int x = dialog.left + ui_units(window,
        tibia::client::hotkey_footer_x[index]);
    const int y = dialog.top + ui_units(window,
        tibia::client::hotkey_footer_y);
    return {x, y, x + ui_units(window, 43), y + ui_units(window, 20)};
}

int ui_units(HWND window, int value) {
    return std::max(1, static_cast<int>(std::lround(
        static_cast<float>(value) * ui_scale(window))));
}

RECT info_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    // FUN_0045b0d0 expands the Info panel's 190x111 child by its 9-unit skin
    // insets on each side before FUN_004b1ac0 sizes the outer dialog.
    const int width = ui_units(window, 244);
    const int height = ui_units(window, 221);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT help_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, 304);
    const int height = ui_units(window, 295);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = (client_width - width) / 2;
    const int top = (client_height - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT hints_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, 236);
    const int height = ui_units(window, 211);
    const int left = (client.right - client.left - width) / 2;
    const int top = (client.bottom - client.top - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT hint_content_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, 436);
    const int height = ui_units(window, 432);
    const int left = (client.right - client.left - width) / 2;
    const int top = (client.bottom - client.top - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT hints_list_rect(HWND window, POINT offset = {}) {
    const auto dialog = hints_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 18), dialog.top + ui_units(window, 47),
        dialog.left + ui_units(window, 218), dialog.top + ui_units(window, 151)};
}

RECT hints_button_rect(HWND window, POINT offset, int index) {
    const auto dialog = hints_dialog_rect(window, offset);
    const int left = index == 0 ? 180 : 129;
    return {dialog.left + ui_units(window, left), dialog.top + ui_units(window, 181),
        dialog.left + ui_units(window, left + 43), dialog.top + ui_units(window, 201)};
}

RECT hint_content_view_rect(HWND window, POINT offset = {}) {
    const auto dialog = hint_content_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 18), dialog.top + ui_units(window, 32),
        dialog.left + ui_units(window, 418), dialog.top + ui_units(window, 372)};
}

RECT hint_content_ok_rect(HWND window, POINT offset = {}) {
    const auto dialog = hint_content_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 380), dialog.top + ui_units(window, 402),
        dialog.left + ui_units(window, 423), dialog.top + ui_units(window, 422)};
}

RECT help_ok_rect(HWND window, POINT offset = {}) {
    const auto dialog = help_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 248), dialog.top + ui_units(window, 265),
        dialog.left + ui_units(window, 291), dialog.top + ui_units(window, 285)};
}

RECT help_action_button_rect(HWND window, POINT offset, int index) {
    constexpr std::array<int, 6> row_y{0, 33, 66, 99, 132, 165};
    const auto dialog = help_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window, 27);
    const int top = dialog.top + ui_units(window, 41 + row_y.at(static_cast<std::size_t>(index)));
    return {left, top, left + ui_units(window, 86), top + ui_units(window, 20)};
}

RECT report_violation_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, 286);
    const int height = ui_units(window, 179);
    const int left = (client.right - client.left - width) / 2;
    const int top = (client.bottom - client.top - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT report_violation_input_rect(HWND window, POINT offset = {}) {
    const auto dialog = report_violation_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window, 18);
    const int top = dialog.top + ui_units(window, 32 + 71);
    return {left, top, left + ui_units(window, 250), top + ui_units(window, 16)};
}

RECT report_violation_button_rect(HWND window, POINT offset, int index) {
    const auto dialog = report_violation_dialog_rect(window, offset);
    const int left = dialog.right - ui_units(window, index == 0 ? 109 : 56);
    const int top = dialog.bottom - ui_units(window, 30);
    return {left, top, left + ui_units(window, 43), top + ui_units(window, 20)};
}

RECT rule_violation_request_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, 336);
    const int height = ui_units(window, 438);
    const int left = (client.right - client.left - width) / 2;
    const int top = (client.bottom - client.top - height) / 2;
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT rule_violation_request_content_rect(HWND window, POINT offset = {}) {
    const auto dialog = rule_violation_request_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 18), dialog.top + ui_units(window, 32),
        dialog.left + ui_units(window, 318), dialog.top + ui_units(window, 378)};
}

RECT rule_violation_request_control_rect(HWND window, POINT offset,
    int x, int y, int width, int height) {
    const auto content = rule_violation_request_content_rect(window, offset);
    return {content.left + ui_units(window, x), content.top + ui_units(window, y),
        content.left + ui_units(window, x + width),
        content.top + ui_units(window, y + height)};
}

RECT rule_violation_request_list_rect(HWND window, POINT offset, bool action) {
    return rule_violation_request_control_rect(window, offset, 0,
        action ? 214 : 107, 300, action ? 49 : 77);
}

RECT rule_violation_request_button_rect(HWND window, POINT offset, int index) {
    const auto dialog = rule_violation_request_dialog_rect(window, offset);
    const int right_offset = index == 0 ? 109 : 56;
    return {dialog.right - ui_units(window, right_offset),
        dialog.bottom - ui_units(window, 30),
        dialog.right - ui_units(window, right_offset - 43),
        dialog.bottom - ui_units(window, 10)};
}

RECT rule_violation_request_edit_rect(HWND window, POINT offset, int field) {
    constexpr std::array<int, 4> y{15, 74, 0, 330};
    constexpr std::array<int, 4> width{300, 300, 0, 300};
    const auto content = rule_violation_request_content_rect(window, offset);
    if (field == 2) return {};
    return {content.left + ui_units(window, 0),
        content.top + ui_units(window, y[static_cast<std::size_t>(field)]),
        content.left + ui_units(window, width[static_cast<std::size_t>(field)]),
        content.top + ui_units(window, y[static_cast<std::size_t>(field)] + 16)};
}

RECT rule_violation_request_checkbox_rect(HWND window, POINT offset) {
    return rule_violation_request_control_rect(window, offset, 0, 278, 300, 22);
}

RECT quest_dialog_rect(HWND window, POINT offset, int width, int height) {
    RECT client{};
    GetClientRect(window, &client);
    const int scaled_width = ui_units(window, width);
    const int scaled_height = ui_units(window, height);
    const int left = (client.right - client.left - scaled_width) / 2;
    const int top = (client.bottom - client.top - scaled_height) / 2;
    return clamp_dialog_rect(window,
        {left, top, left + scaled_width, top + scaled_height}, offset);
}

RECT quest_log_dialog_rect(HWND window, POINT offset = {}) {
    return quest_dialog_rect(window, offset, 336, 360);
}

RECT quest_line_dialog_rect(HWND window, POINT offset = {}) {
    return quest_dialog_rect(window, offset, 336, 350);
}

RECT quest_log_list_rect(HWND window, POINT offset = {}) {
    const auto dialog = quest_log_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window, 18);
    const int top = dialog.top + ui_units(window, 32);
    return {left, top, left + ui_units(window, 300), top + ui_units(window, 268)};
}

RECT quest_line_flags_rect(HWND window, POINT offset = {}) {
    const auto dialog = quest_line_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window, 18);
    const int top = dialog.top + ui_units(window, 50);
    return {left, top, left + ui_units(window, 300), top + ui_units(window, 118)};
}

tibia::client::QuestLogScrollbarGeometry quest_list_scrollbar_geometry(
    HWND window, const RECT& list, std::size_t entry_count, int first_visible) {
    return tibia::client::quest_log_scrollbar_geometry(
        list.right - list.left, list.bottom - list.top, entry_count, first_visible,
        ui_units(window, tibia::client::quest_log_row_height),
        ui_units(window, tibia::client::quest_log_content_margin),
        ui_units(window, tibia::client::quest_scrollbar_width),
        ui_units(window, tibia::client::quest_scrollbar_arrow_height),
        ui_units(window, tibia::client::quest_scrollbar_thumb_cap_height));
}

RECT quest_scrollbar_part_rect(HWND window, const RECT& list,
    const tibia::client::QuestLogRect& part) {
    return {list.left + part.left, list.top + part.top,
        list.left + part.right, list.top + part.bottom};
}

RECT quest_log_button_rect(HWND window, POINT offset, int index) {
    const auto dialog = quest_log_dialog_rect(window, offset);
    const int right_offset = index == 0 ? 109 : 56;
    const int left = dialog.right - ui_units(window, right_offset);
    const int top = dialog.bottom - ui_units(window, 30);
    return {left, top, left + ui_units(window, 43), top + ui_units(window, 20)};
}

RECT quest_line_close_rect(HWND window, POINT offset = {}) {
    const auto dialog = quest_line_dialog_rect(window, offset);
    const int left = dialog.right - ui_units(window, 56);
    const int top = dialog.bottom - ui_units(window, 30);
    return {left, top, left + ui_units(window, 43), top + ui_units(window, 20)};
}

RECT info_ok_rect(HWND window, POINT offset = {}) {
    const auto dialog = info_dialog_rect(window, offset);
    // Info removes the default Cancel control; its single OK stays right-anchored.
    return {dialog.left + ui_units(window, 188), dialog.top + ui_units(window, 191),
        dialog.left + ui_units(window, 231), dialog.top + ui_units(window, 211)};
}

RECT info_website_rect(HWND window, POINT offset = {}) {
    const auto dialog = info_dialog_rect(window, offset);
    // The 190-unit inner row is split into a 104-unit label and the 86x20 link button.
    return {dialog.left + ui_units(window, 18 + 9 + 104),
        dialog.top + ui_units(window, 32 + 9 + 91),
        dialog.left + ui_units(window, 18 + 9 + 190),
        dialog.top + ui_units(window, 32 + 9 + 111)};
}

RECT options_list_scrollbar_part_rect(const RECT& list,
    const tibia::client::UiScrollbarRect& part) {
    return {list.left + part.left, list.top + part.top,
        list.left + part.right, list.top + part.bottom};
}

tibia::client::UiScrollbarGeometry options_list_scrollbar_geometry(
    HWND window, const RECT& list, std::size_t item_count, int visible_items,
    int first_visible) {
    return tibia::client::ui_scrollbar_geometry(list.right - list.left,
        list.bottom - list.top, item_count, visible_items, first_visible,
        ui_units(window, 3), ui_units(window, 12), ui_units(window, 12),
        ui_units(window, 6));
}

RECT options_list_rect(const ClientHost& host,
    ClientHost::ListScrollbarTarget target) {
    if (target == ClientHost::ListScrollbarTarget::graphics_modes)
        return graphics_options_list_rect(host.window, host.graphics_options_dialog_offset);
    if (target == ClientHost::ListScrollbarTarget::hotkeys)
        return hotkey_options_list_rect(host.window, host.hotkey_options_dialog_offset);
    return rule_violation_request_list_rect(host.window,
        host.rule_violation_request_dialog_offset,
        target == ClientHost::ListScrollbarTarget::rule_violation_actions);
}

std::size_t options_list_item_count(const ClientHost& host,
    ClientHost::ListScrollbarTarget target) {
    switch (target) {
    case ClientHost::ListScrollbarTarget::graphics_modes:
        return host.graphics_display_modes.size();
    case ClientHost::ListScrollbarTarget::hotkeys:
        return tibia::client::hotkey_count;
    case ClientHost::ListScrollbarTarget::rule_violation_reasons:
        return host.rule_violation_request_form.available_reasons.size();
    case ClientHost::ListScrollbarTarget::rule_violation_actions:
        return host.rule_violation_request_form.available_actions.size();
    case ClientHost::ListScrollbarTarget::none:
        break;
    }
    return 0;
}

int options_list_visible_items(const ClientHost& host,
    ClientHost::ListScrollbarTarget target) {
    if (target == ClientHost::ListScrollbarTarget::graphics_modes)
        return tibia::client::graphics_options_visible_rows;
    if (target == ClientHost::ListScrollbarTarget::hotkeys)
        return static_cast<int>(tibia::client::hotkey_list_visible_rows);
    const auto list = options_list_rect(host, target);
    const int margin = ui_units(host.window, 3);
    const int row_height = ui_units(host.window,
        std::max(1, host.ui_font_line_advances[3]));
    return std::max(1, (static_cast<int>(list.bottom - list.top) - 2 * margin) /
        row_height);
}

int options_list_first_visible(const ClientHost& host,
    ClientHost::ListScrollbarTarget target) {
    switch (target) {
    case ClientHost::ListScrollbarTarget::graphics_modes:
        return host.graphics_mode_scroll;
    case ClientHost::ListScrollbarTarget::hotkeys:
        return static_cast<int>(host.hotkey_list_scroll);
    case ClientHost::ListScrollbarTarget::rule_violation_reasons:
        return host.rule_violation_request_reason_scroll;
    case ClientHost::ListScrollbarTarget::rule_violation_actions:
        return host.rule_violation_request_action_scroll;
    case ClientHost::ListScrollbarTarget::none:
        break;
    }
    return 0;
}

void set_options_list_first_visible(ClientHost& host,
    ClientHost::ListScrollbarTarget target, int value) {
    const int maximum = std::max(0, static_cast<int>(
        options_list_item_count(host, target)) - options_list_visible_items(host, target));
    value = std::clamp(value, 0, maximum);
    if (target == ClientHost::ListScrollbarTarget::graphics_modes) {
        host.graphics_mode_scroll = value;
    } else if (target == ClientHost::ListScrollbarTarget::hotkeys) {
        host.hotkey_list_scroll = static_cast<std::size_t>(value);
    } else if (target == ClientHost::ListScrollbarTarget::rule_violation_reasons) {
        host.rule_violation_request_reason_scroll = value;
    } else if (target == ClientHost::ListScrollbarTarget::rule_violation_actions) {
        host.rule_violation_request_action_scroll = value;
    }
}

bool begin_options_list_scrollbar_interaction(ClientHost& host,
    ClientHost::ListScrollbarTarget target, POINT mouse) {
    const auto list = options_list_rect(host, target);
    const auto geometry = options_list_scrollbar_geometry(host.window, list,
        options_list_item_count(host, target), options_list_visible_items(host, target),
        options_list_first_visible(host, target));
    const RECT up = options_list_scrollbar_part_rect(list, geometry.up_arrow);
    const RECT down = options_list_scrollbar_part_rect(list, geometry.down_arrow);
    const RECT track = options_list_scrollbar_part_rect(list, geometry.track);
    const int count = static_cast<int>(options_list_item_count(host, target));
    const int visible = options_list_visible_items(host, target);
    int first = options_list_first_visible(host, target);

    host.list_scrollbar_target = target;
    host.pressed_list_scrollbar_part = -1;
    host.list_scrollbar_dragging = false;
    if (contains_point(up, mouse)) {
        host.pressed_list_scrollbar_part = 0;
        set_options_list_first_visible(host, target, first - 1);
        return true;
    }
    if (contains_point(down, mouse)) {
        host.pressed_list_scrollbar_part = 1;
        set_options_list_first_visible(host, target, first + 1);
        return true;
    }
    if (!geometry.thumb_visible) return contains_point(track, mouse);

    const RECT thumb = options_list_scrollbar_part_rect(list, geometry.thumb);
    if (contains_point(thumb, mouse)) {
        host.list_scrollbar_dragging = true;
        host.list_scrollbar_grab_offset = mouse.y - thumb.top;
        return true;
    }
    if (contains_point(track, mouse)) {
        host.pressed_list_scrollbar_part = 2;
        const int local_y = mouse.y - list.top;
        set_options_list_first_visible(host, target,
            tibia::client::ui_scrollbar_scroll_after_track_click(local_y,
                geometry, first, static_cast<std::size_t>(count), visible));
        return true;
    }
    host.list_scrollbar_target = ClientHost::ListScrollbarTarget::none;
    return false;
}

void update_options_list_scrollbar_drag(ClientHost& host) {
    if (!host.list_scrollbar_dragging ||
        host.list_scrollbar_target == ClientHost::ListScrollbarTarget::none) return;
    const auto target = host.list_scrollbar_target;
    const auto list = options_list_rect(host, target);
    const int count = static_cast<int>(options_list_item_count(host, target));
    const int visible = options_list_visible_items(host, target);
    const auto geometry = options_list_scrollbar_geometry(host.window, list,
        static_cast<std::size_t>(count), visible,
        options_list_first_visible(host, target));
    const int first = tibia::client::ui_scrollbar_scroll_after_thumb_drag(
        host.mouse.y - list.top, host.list_scrollbar_grab_offset, geometry,
        static_cast<std::size_t>(count), visible);
    set_options_list_first_visible(host, target, first);
}

void finish_options_list_scrollbar_interaction(ClientHost& host) {
    host.list_scrollbar_target = ClientHost::ListScrollbarTarget::none;
    host.pressed_list_scrollbar_part = -1;
    host.list_scrollbar_dragging = false;
}

// The original manager owns the final screen origin. Centering matches the
// centered original Options dialog; login's exact origin is still unconfirmed.
RECT login_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, 236);
    const int height = ui_units(window, 178);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = std::max(0, (client_width - width) / 2);
    const int top = std::max(0, (client_height - height) / 2);
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT login_rect(HWND window, POINT offset = {}) {
    const auto dialog = login_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window, 18);
    const int top = dialog.top + ui_units(window, 32);
    return {left, top, left + ui_units(window, 200), top + ui_units(window, 86)};
}

RECT login_account_field_rect(HWND window, POINT offset = {}) {
    const auto panel = login_rect(window, offset);
    const int left = panel.left + ui_units(window, 114);
    const int top = panel.top;
    return {left, top, left + ui_units(window, 86), top + ui_units(window, 16)};
}

RECT login_password_field_rect(HWND window, POINT offset = {}) {
    const auto panel = login_rect(window, offset);
    const int left = panel.left + ui_units(window, 114);
    const int top = panel.top + ui_units(window, 29);
    return {left, top, left + ui_units(window, 86), top + ui_units(window, 16)};
}

RECT login_ok_rect(HWND window, POINT offset = {}) {
    const auto dialog = login_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window, 127);
    const int top = dialog.top + ui_units(window, 148);
    return {left, top, left + ui_units(window, 43), top + ui_units(window, 20)};
}

RECT login_cancel_rect(HWND window, POINT offset = {}) {
    const auto dialog = login_dialog_rect(window, offset);
    const int left = dialog.left + ui_units(window, 180);
    const int top = dialog.top + ui_units(window, 148);
    return {left, top, left + ui_units(window, 43), top + ui_units(window, 20)};
}

void draw_bitmap_text(ClientHost& host, std::wstring_view text, int x, int y,
    int max_width = 0, bool center = false, std::size_t font_index = 2,
    std::uint8_t tint = 255, bool use_window_scale = true) {
    if (!host.renderer) return;
    font_index = std::min(font_index, host.ui_fonts.size() - 1);
    const float scale = use_window_scale && host.window ? ui_scale(host.window) : 1.0F;
    const auto scaled = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    const auto width_of = [&host, font_index, &scaled](wchar_t ch) {
        if (ch == L' ') return scaled(host.ui_fonts[font_index][12].advance);
        if (ch < 32 || ch > 255) ch = L'?';
        return scaled(host.ui_fonts[font_index][static_cast<std::size_t>(ch - 32)].advance);
    };
    int total_width = 0;
    for (const wchar_t ch : text) total_width += width_of(ch);
    const int letter_spacing = static_cast<int>(std::lround(
        static_cast<float>(host.ui_font_spacing[font_index]) * scale));
    if (text.size() > 1)
        total_width += static_cast<int>(text.size() - 1) * letter_spacing;
    const int clip_right = max_width > 0 ? x + max_width : 0;
    if (max_width > 0 && center) x += std::max(0, (max_width - total_width) / 2);
    int tint_index = -1;
    for (std::size_t i = 0; i < ui_font_text_shades.size(); ++i)
        if (ui_font_text_shades[i] == tint) tint_index = static_cast<int>(i);
    for (std::size_t text_index = 0; text_index < text.size(); ++text_index) {
        const wchar_t ch = text[text_index];
        if (ch == L' ') {
            x += width_of(ch);
            if (text_index + 1 < text.size()) x += letter_spacing;
            continue;
        }
        const wchar_t code = ch < 32 || ch > 255 ? L'?' : ch;
        const auto glyph_index = static_cast<std::size_t>(code - 32);
        const auto& glyph = tint_index >= 0
            ? host.ui_font_tints[font_index][static_cast<std::size_t>(tint_index)][glyph_index]
            : host.ui_fonts[font_index][glyph_index];
        if (max_width > 0 && !center && x + scaled(glyph.width) > clip_right) break;
        if (glyph.texture && glyph.width > 0 && glyph.height > 0)
            host.renderer->draw(glyph.texture, x, y, scaled(glyph.width), scaled(glyph.height));
        x += width_of(ch);
        if (text_index + 1 < text.size()) x += letter_spacing;
    }
}

int centered_bitmap_text_y(const ClientHost& host, int top, int height,
    std::size_t font_index) {
    font_index = std::min(font_index, host.ui_font_heights.size() - 1);
    const float scale = host.window ? ui_scale(host.window) : 1.0F;
    const int text_height = std::max(1, static_cast<int>(std::lround(
        static_cast<float>(host.ui_font_heights[font_index]) * scale)));
    return top + (height - text_height) / 2;
}

void draw_bitmap_text_clipped(ClientHost& host, std::wstring_view text,
    int x, int y, const RECT& clip, std::size_t font_index, std::uint8_t tint) {
    if (!host.renderer) return;
    font_index = std::min(font_index, host.ui_fonts.size() - 1);
    const float scale = host.window ? ui_scale(host.window) : 1.0F;
    const auto scaled = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    int tint_index = -1;
    for (std::size_t index = 0; index < ui_font_text_shades.size(); ++index)
        if (ui_font_text_shades[index] == tint) tint_index = static_cast<int>(index);
    for (const wchar_t character : text) {
        if (character == L' ') {
            x += scaled(host.ui_fonts[font_index][12].advance) +
                static_cast<int>(std::lround(host.ui_font_spacing[font_index] * scale));
            continue;
        }
        const wchar_t code = character < 32 || character > 255 ? L'?' : character;
        const auto glyph_index = static_cast<std::size_t>(code - 32);
        const auto& glyph = tint_index >= 0
            ? host.ui_font_tints[font_index][static_cast<std::size_t>(tint_index)][glyph_index]
            : host.ui_fonts[font_index][glyph_index];
        if (glyph.texture && glyph.width > 0 && glyph.height > 0)
            host.renderer->draw_clipped(glyph.texture, x, y, scaled(glyph.width),
                scaled(glyph.height), clip.left, clip.top,
                clip.right - clip.left, clip.bottom - clip.top);
        x += scaled(glyph.advance) +
            static_cast<int>(std::lround(host.ui_font_spacing[font_index] * scale));
    }
}

std::uint32_t map_text_glyph_texture(ClientHost& host, std::uint8_t color,
    std::size_t glyph_index) {
    const auto key = static_cast<std::uint32_t>(color) * 224U +
        static_cast<std::uint32_t>(glyph_index);
    if (const auto found = host.map_text_glyph_textures.find(key);
        found != host.map_text_glyph_textures.end())
        return found->second;
    const auto red = static_cast<std::uint8_t>(std::min(255U,
        static_cast<unsigned>(color / 36U) * 0x33U));
    const auto green = static_cast<std::uint8_t>(static_cast<unsigned>((color / 6U) % 6U) * 0x33U);
    const auto blue = static_cast<std::uint8_t>(static_cast<unsigned>(color % 6U) * 0x33U);
    auto image = host.ui_font_images[1][glyph_index];
    for (std::size_t pixel = 0; pixel < 32U * 32U; ++pixel) {
        const auto offset = pixel * 4U;
        if (image.rgba[offset + 3] == 0) continue;
        image.rgba[offset] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset]) * red / 255U);
        image.rgba[offset + 1] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 1]) * green / 255U);
        image.rgba[offset + 2] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 2]) * blue / 255U);
    }
    const auto texture = map_text_texture_base + key;
    host.renderer->upload(texture, image);
    host.map_text_glyph_textures.emplace(key, texture);
    return texture;
}

std::uint32_t colored_ui_font_glyph_texture(ClientHost& host,
    std::size_t font_index, std::uint32_t rgb, std::size_t glyph_index) {
    font_index = std::min(font_index, host.ui_font_images.size() - 1);
    const auto key = (static_cast<std::uint64_t>(font_index) << 40U) |
        (static_cast<std::uint64_t>(rgb) << 8U) |
        static_cast<std::uint64_t>(glyph_index);
    if (const auto found = host.creature_info_glyph_textures.find(key);
        found != host.creature_info_glyph_textures.end())
        return found->second;
    const auto red = static_cast<std::uint8_t>((rgb >> 16U) & 0xffU);
    const auto green = static_cast<std::uint8_t>((rgb >> 8U) & 0xffU);
    const auto blue = static_cast<std::uint8_t>(rgb & 0xffU);
    auto image = host.ui_font_images[font_index][glyph_index];
    for (std::size_t pixel = 0; pixel < 32U * 32U; ++pixel) {
        const auto offset = pixel * 4U;
        if (image.rgba[offset + 3] == 0) continue;
        image.rgba[offset] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset]) * red / 255U);
        image.rgba[offset + 1] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 1]) * green / 255U);
        image.rgba[offset + 2] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 2]) * blue / 255U);
    }
    const auto texture = creature_info_texture_base +
        static_cast<std::uint32_t>(host.creature_info_glyph_textures.size());
    host.renderer->upload(texture, image);
    host.creature_info_glyph_textures.emplace(key, texture);
    return texture;
}

std::uint32_t creature_info_glyph_texture(ClientHost& host, std::uint32_t rgb,
    std::size_t glyph_index) {
    return colored_ui_font_glyph_texture(host, 2, rgb, glyph_index);
}

std::uint32_t creature_info_solid_texture(ClientHost& host,
    const std::array<std::uint8_t, 3>& color) {
    const auto rgb = (static_cast<std::uint32_t>(color[0]) << 16U) |
        (static_cast<std::uint32_t>(color[1]) << 8U) | color[2];
    const auto texture = creature_info_solid_texture_base + rgb;
    if (!host.creature_info_solid_textures.contains(rgb)) {
        tibia::resources::TibiaSprImage image{};
        image.color_key = {color[0], color[1], color[2]};
        for (std::size_t offset = 0; offset < image.rgba.size(); offset += 4) {
            image.rgba[offset] = color[0];
            image.rgba[offset + 1] = color[1];
            image.rgba[offset + 2] = color[2];
            image.rgba[offset + 3] = 255;
        }
        host.renderer->upload(texture, image);
        host.creature_info_solid_textures.emplace(rgb, true);
    }
    return texture;
}

void draw_creature_info_name(ClientHost& host, std::string_view name,
    int x, int y, const std::array<std::uint8_t, 3>& color,
    const tibia::client::MapWindowLayout& clip) {
    if (!host.renderer || name.empty()) return;
    const auto text = decode_ui_string(std::string(name));
    const auto rgb = (static_cast<std::uint32_t>(color[0]) << 16U) |
        (static_cast<std::uint32_t>(color[1]) << 8U) | color[2];
    const auto spacing = host.ui_font_spacing[2];
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto character = text[index] < 32 || text[index] > 255
            ? L'?' : text[index];
        if (character == L' ') {
            x += host.ui_fonts[2][12].advance;
        } else {
            const auto glyph_index = static_cast<std::size_t>(character - 32);
            const auto& glyph = host.ui_fonts[2][glyph_index];
            if (glyph.width > 0 && glyph.height > 0)
                host.renderer->draw_clipped(
                    creature_info_glyph_texture(host, rgb, glyph_index),
                    x, y, glyph.width, glyph.height, clip.x, clip.y,
                    clip.width, clip.height);
            x += glyph.advance;
        }
        if (index + 1 < text.size()) x += spacing;
    }
}

std::uint32_t console_message_glyph_texture(ClientHost& host, std::uint32_t rgb,
    std::size_t glyph_index) {
    const auto key = (rgb << 8U) | static_cast<std::uint32_t>(glyph_index);
    if (const auto found = host.console_message_glyph_textures.find(key);
        found != host.console_message_glyph_textures.end())
        return found->second;
    const auto red = static_cast<std::uint8_t>((rgb >> 16U) & 0xffU);
    const auto green = static_cast<std::uint8_t>((rgb >> 8U) & 0xffU);
    const auto blue = static_cast<std::uint8_t>(rgb & 0xffU);
    auto image = host.ui_font_images[3][glyph_index];
    for (std::size_t pixel = 0; pixel < 32U * 32U; ++pixel) {
        const auto offset = pixel * 4U;
        if (image.rgba[offset + 3] == 0) continue;
        image.rgba[offset] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset]) * red / 255U);
        image.rgba[offset + 1] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 1]) * green / 255U);
        image.rgba[offset + 2] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 2]) * blue / 255U);
    }
    const auto texture = console_message_texture_base +
        static_cast<std::uint32_t>(host.console_message_glyph_textures.size());
    host.renderer->upload(texture, image);
    host.console_message_glyph_textures.emplace(key, texture);
    return texture;
}

std::uint32_t hint_text_glyph_texture(ClientHost& host, std::uint32_t rgb,
    std::size_t font_index, std::size_t glyph_index) {
    font_index = std::min(font_index, host.ui_fonts.size() - 1);
    const auto key = (static_cast<std::uint64_t>(rgb) << 16U) |
        (static_cast<std::uint64_t>(font_index) << 8U) |
        static_cast<std::uint64_t>(glyph_index);
    if (const auto found = host.hint_text_glyph_textures.find(key);
        found != host.hint_text_glyph_textures.end())
        return found->second;
    auto image = host.ui_font_images[font_index][glyph_index];
    const auto red = static_cast<std::uint8_t>((rgb >> 16U) & 0xffU);
    const auto green = static_cast<std::uint8_t>((rgb >> 8U) & 0xffU);
    const auto blue = static_cast<std::uint8_t>(rgb & 0xffU);
    for (std::size_t pixel = 0; pixel < 32U * 32U; ++pixel) {
        const auto offset = pixel * 4U;
        if (image.rgba[offset + 3] == 0) continue;
        image.rgba[offset] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset]) * red / 255U);
        image.rgba[offset + 1] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 1]) * green / 255U);
        image.rgba[offset + 2] = static_cast<std::uint8_t>(
            static_cast<unsigned>(image.rgba[offset + 2]) * blue / 255U);
    }
    const auto texture = hint_text_texture_base +
        static_cast<std::uint32_t>(host.hint_text_glyph_textures.size());
    host.renderer->upload(texture, image);
    host.hint_text_glyph_textures.emplace(key, texture);
    return texture;
}

std::uint32_t report_violation_message_rgb(std::uint8_t mode) noexcept {
    if (mode == 8) return 0x00f0f000U;
    if (mode == 0x12) return 0x00f86060U;
    return 0x00f0f0f0U;
}

std::optional<std::uint32_t> console_message_rgb(std::uint8_t mode) noexcept {
    switch (mode) {
    case 1: case 2: case 3: case 5: case 7: case 8:
        return 0x00f0f000U;
    case 4:
        return 0x0060f8f8U;
    case 6: case 0x13: case 0x14: case 0x15: case 0x17:
        return 0x00f0f0f0U;
    case 9: case 10: case 0x0b: case 0x0d: case 0x0e: case 0x0f:
    case 0x12: case 0x19:
        return 0x00f86060U;
    case 0x0c:
        return 0x00f0b400U;
    case 0x10: case 0x11:
        return 0x00ff6600U;
    case 0x16:
        return 0x0000f000U;
    case 0x18:
        return 0x00a0a0ffU;
    default:
        return std::nullopt;
    }
}

void draw_map_text_effect(ClientHost& host, std::string_view text, int center_x,
    int y, std::uint8_t color, const tibia::client::MapWindowLayout& clip) {
    if (!host.renderer || text.empty()) return;
    constexpr std::size_t font_index = 1;
    const auto scaled = [](int value) {
        return std::max(1, value);
    };
    constexpr int letter_spacing = -1;
    int total_width{};
    for (const unsigned char character : text) {
        const auto code = character < 32 ? static_cast<unsigned char>('?') : character;
        total_width += scaled(host.ui_fonts[font_index][static_cast<std::size_t>(code - 32)].advance);
    }
    if (text.size() > 1)
        total_width += static_cast<int>(text.size() - 1) * letter_spacing;
    int x = center_x - total_width / 2;
    for (const unsigned char character : text) {
        if (character == ' ') {
            x += scaled(host.ui_fonts[font_index][12].advance) + letter_spacing;
            continue;
        }
        const auto code = character < 32 ? static_cast<unsigned char>('?') : character;
        const auto glyph_index = static_cast<std::size_t>(code - 32);
        const auto& glyph = host.ui_fonts[font_index][glyph_index];
        if (glyph.width > 0 && glyph.height > 0) {
            host.renderer->draw_clipped(map_text_glyph_texture(host, color, glyph_index),
                x, y, scaled(glyph.width), scaled(glyph.height), clip.x, clip.y,
                clip.width, clip.height);
        }
        x += scaled(glyph.advance) + letter_spacing;
    }
}

void upload_solid_ui_texture(ClientHost& host, std::uint32_t texture, std::uint8_t shade) {
    tibia::resources::TibiaSprImage image{};
    image.color_key = {shade, shade, shade};
    for (std::size_t offset = 0; offset < image.rgba.size(); offset += 4) {
        image.rgba[offset] = shade;
        image.rgba[offset + 1] = shade;
        image.rgba[offset + 2] = shade;
        image.rgba[offset + 3] = 255;
    }
    host.renderer->upload(texture, image);
}

void upload_solid_ui_color(ClientHost& host, std::uint32_t texture,
    std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    tibia::resources::TibiaSprImage image{};
    image.color_key = {red, green, blue};
    for (std::size_t offset = 0; offset < image.rgba.size(); offset += 4) {
        image.rgba[offset] = red;
        image.rgba[offset + 1] = green;
        image.rgba[offset + 2] = blue;
        image.rgba[offset + 3] = 255;
    }
    host.renderer->upload(texture, image);
}

int bitmap_text_width(const ClientHost& host, std::wstring_view text,
    std::size_t font_index, HWND window, bool use_window_scale = true) {
    font_index = std::min(font_index, host.ui_fonts.size() - 1);
    const float scale = use_window_scale ? ui_scale(window) : 1.0F;
    int width = 0;
    std::size_t character_count = 0;
    for (wchar_t character : text) {
        const int advance = character == L' '
            ? host.ui_fonts[font_index][12].advance
            : host.ui_fonts[font_index][static_cast<std::size_t>(
                (character < 32 || character > 255 ? L'?' : character) - 32)].advance;
        width += std::max(1, static_cast<int>(std::lround(advance * scale)));
        ++character_count;
    }
    if (character_count > 1)
        width += static_cast<int>(character_count - 1) * static_cast<int>(std::lround(
            static_cast<float>(host.ui_font_spacing[font_index]) * scale));
    return width;
}

void draw_console_tab_title(ClientHost& host, std::wstring_view title,
    const tibia::client::ConsolePanelRect& rect, int text_y,
    std::size_t font_index, std::uint32_t rgb) {
    if (!host.renderer || !host.window || title.empty()) return;
    font_index = std::min(font_index, host.ui_fonts.size() - 1);
    std::wstring fitted_title;
    const int full_width = bitmap_text_width(host, title, font_index, host.window);
    if (full_width > rect.width) {
        std::size_t fitting_characters = 0;
        while (fitting_characters < title.size() &&
            bitmap_text_width(host, title.substr(0, fitting_characters + 1),
                font_index, host.window) <= rect.width)
            ++fitting_characters;
        const auto prefix_length = fitting_characters > 3
            ? fitting_characters - 3 : 0;
        fitted_title.assign(title.substr(0, prefix_length));
        fitted_title.append(std::min<std::size_t>(3, title.size()), L'.');
        title = fitted_title;
    }
    const int width = bitmap_text_width(host, title, font_index, host.window);
    int x = rect.x + (rect.width - width) / 2;
    const float scale = ui_scale(host.window);
    const auto scaled = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(value * scale)));
    };
    const RECT clip{rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
    for (std::size_t index = 0; index < title.size(); ++index) {
        wchar_t character = title[index];
        if (character == L' ') {
            x += scaled(host.ui_fonts[font_index][12].advance);
        } else {
            if (character < 32 || character > 255) character = L'?';
            const auto glyph_index = static_cast<std::size_t>(character - 32);
            const auto& glyph = host.ui_fonts[font_index][glyph_index];
            const int y = rect.y + text_y;
            if (glyph.width > 0 && glyph.height > 0 && x < clip.right &&
                x + scaled(glyph.width) > clip.left && y < clip.bottom &&
                y + scaled(glyph.height) > clip.top) {
                host.renderer->draw_clipped(
                    hint_text_glyph_texture(host, rgb, font_index, glyph_index),
                    x, y, scaled(glyph.width), scaled(glyph.height), clip.left,
                    clip.top, clip.right - clip.left, clip.bottom - clip.top);
            }
            x += scaled(glyph.advance);
        }
        if (index + 1 < title.size())
            x += static_cast<int>(std::lround(
                host.ui_font_spacing[font_index] * scale));
    }
}

void update_console_tab_unread_state(ConsoleTabUnreadState& state,
    std::size_t message_count, bool active, std::uint64_t current_tick) {
    if (!state.initialized) {
        state.message_count = message_count;
        state.initialized = true;
    } else if (state.message_count != message_count) {
        state.message_count = message_count;
        state.unread = true;
        state.flash_until_tick = current_tick + 10;
    }
    if (active) state.unread = false;
}

std::uint32_t console_tab_text_rgb(const ConsoleTabUnreadState& state,
    bool active, std::uint64_t current_tick) {
    if (active) return 0x00e0e0e0U;
    if (!state.unread) return 0x00808080U;
    return current_tick < state.flash_until_tick
        ? 0x00f8f8f8U : 0x00f86060U;
}

std::size_t console_input_caret_for_x(const ClientHost& host,
    std::wstring_view text, std::size_t font_index, HWND window, int x) {
    for (std::size_t index = 1; index <= text.size(); ++index) {
        const int next_width = bitmap_text_width(host, text.substr(0, index),
            font_index, window);
        if (x < next_width) return index - 1;
    }
    return text.size();
}

std::size_t console_transcript_caret_for_x(const ClientHost& host,
    std::wstring_view text, HWND window, int x) {
    if (text.empty()) return 0;
    for (std::size_t index = 1; index <= text.size(); ++index) {
        const int next_width = bitmap_text_width(host, text.substr(0, index),
            3, window);
        if (x < next_width) return index - 1;
    }
    return text.size();
}

int console_transcript_visible_rows(const ClientHost& host, int row_count,
    int viewport_height) {
    if (row_count <= 0 || viewport_height <= 0) return 0;
    const float scale = ui_scale(host.window);
    const int first_height = std::max(1, static_cast<int>(std::lround(
        static_cast<float>(host.ui_font_heights[3]) * scale)));
    const int line_advance = std::max(1, static_cast<int>(std::lround(
        static_cast<float>(host.ui_font_line_advances[3]) * scale)));
    int remaining_height = viewport_height - first_height;
    if (remaining_height < 0) return 0;
    int visible_rows = 1;
    while (visible_rows < row_count && remaining_height >= line_advance) {
        remaining_height -= line_advance;
        ++visible_rows;
    }
    return visible_rows;
}

int console_transcript_row_at(const ClientHost& host, POINT point) {
    if (host.console_transcript_rows.empty() ||
        host.console_transcript_line_height <= 0 ||
        host.console_transcript_first_line_height <= 0 ||
        host.console_transcript_visible_rows <= 0)
        return -1;
    point.y = std::clamp(point.y, host.console_transcript_clip.top,
        host.console_transcript_clip.bottom);
    const int row_count = static_cast<int>(host.console_transcript_rows.size());
    const int displayed_rows = std::min(row_count,
        host.console_transcript_visible_rows);
    const int maximum = std::max(0, row_count - displayed_rows);
    const int first_visible = std::clamp(maximum -
        host.console_transcript_scroll_from_bottom, 0, maximum);
    const int last_visible = std::min(row_count - 1,
        first_visible + displayed_rows - 1);
    int row_index = last_visible;
    int row_bottom = static_cast<int>(host.console_transcript_clip.bottom);
    while (row_index >= first_visible) {
        const int height = row_index == last_visible
            ? host.console_transcript_first_line_height
            : host.console_transcript_line_height;
        const int row_top = row_bottom - height;
        if (point.y >= row_top && point.y <= row_bottom) return row_index;
        row_bottom = row_top;
        --row_index;
    }
    return 0;
}

bool update_console_transcript_selection(ClientHost& host, POINT point) {
    if (host.console_transcript_rows.empty() ||
        host.console_transcript_line_height <= 0)
        return false;
    point.x = std::clamp(point.x, host.console_transcript_clip.left,
        host.console_transcript_clip.right);
    const int row_index = console_transcript_row_at(host, point);
    if (row_index < 0 || static_cast<std::size_t>(row_index) >=
        host.console_transcript_rows.size())
        return false;
    const auto& row = host.console_transcript_rows[
        static_cast<std::size_t>(row_index)];
    host.console_transcript_selection_end_row = row_index;
    host.console_transcript_selection_end_offset = static_cast<int>(
        console_transcript_caret_for_x(host, row.text, host.window,
            point.x - host.console_transcript_clip.left - row.indent));
    return true;
}

void apply_console_transcript_track_click(ClientHost& host, HWND window,
    POINT point) {
    if (!host.console_transcript_scrollbar_geometry_valid) return;
    const int maximum = std::max(0, host.console_transcript_item_count -
        host.console_transcript_visible_rows);
    const auto& viewport = host.console_transcript_scrollbar_viewport;
    const int first_visible = std::clamp(maximum -
        host.console_transcript_scroll_from_bottom, 0, maximum);
    const auto geometry = tibia::client::resolve_console_message_scrollbar_geometry(
        static_cast<int>(viewport.right - viewport.left),
        static_cast<int>(viewport.bottom - viewport.top),
        host.console_transcript_item_count,
        host.console_transcript_visible_rows, first_visible);
    const RECT track{viewport.left + geometry.track.x,
        viewport.top + geometry.track.y,
        viewport.left + geometry.track.x + geometry.track.width,
        viewport.top + geometry.track.y + geometry.track.height};
    if (!geometry.thumb_visible || point.x < track.left ||
        point.x > track.right || point.y < track.top || point.y > track.bottom)
        return;
    const int thumb_top = viewport.top + geometry.thumb.y;
    const int thumb_bottom = thumb_top + geometry.thumb.height;
    int first = first_visible;
    if (point.y < thumb_top) first -= 1;
    else if (point.y > thumb_bottom) first += 1;
    first = std::clamp(first, 0, maximum);
    host.console_transcript_scroll_from_bottom = maximum - first;
    InvalidateRect(window, nullptr, FALSE);
}

bool copy_console_input_selection(ClientHost& host, HWND owner) {
    const auto start = std::min(host.console_input_anchor,
        host.console_input_caret);
    const auto end = std::max(host.console_input_anchor,
        host.console_input_caret);
    if (start == end || !OpenClipboard(owner)) return false;
    const auto text = std::wstring_view(host.console_input_text).substr(start,
        end - start);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!memory) {
        CloseClipboard();
        return false;
    }
    auto* destination = static_cast<char*>(GlobalLock(memory));
    if (!destination) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    for (std::size_t index = 0; index < text.size(); ++index) {
        auto value = static_cast<unsigned char>(text[index] & 0xff);
        if ((value > '@' && value < '[') || (value > 0xbf && value < 0xdf))
            value = static_cast<unsigned char>(value + 0x20);
        destination[index] = static_cast<char>(value);
    }
    destination[text.size()] = '\0';
    GlobalUnlock(memory);
    const bool emptied = EmptyClipboard() != FALSE;
    const bool copied = emptied && SetClipboardData(CF_TEXT, memory) != nullptr;
    if (!copied) GlobalFree(memory);
    CloseClipboard();
    return copied;
}

bool copy_text_to_clipboard(HWND owner, std::wstring_view text) {
    if (text.empty() || !OpenClipboard(owner)) return false;
    const auto encoded = encode_ui_string(text);
    if (encoded.empty()) {
        CloseClipboard();
        return false;
    }
    const auto memory = GlobalAlloc(GMEM_MOVEABLE, encoded.size() + 1);
    if (!memory) {
        CloseClipboard();
        return false;
    }
    auto* destination = static_cast<char*>(GlobalLock(memory));
    if (!destination) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    std::copy(encoded.begin(), encoded.end(), destination);
    destination[encoded.size()] = '\0';
    GlobalUnlock(memory);
    const bool copied = EmptyClipboard() != FALSE &&
        SetClipboardData(CF_TEXT, memory) != nullptr;
    if (!copied) GlobalFree(memory);
    CloseClipboard();
    return copied;
}

bool copy_console_transcript_selection(ClientHost& host, HWND owner) {
    if (!host.console_transcript_selection_active ||
        host.console_transcript_rows.empty())
        return false;
    auto start_row = host.console_transcript_selection_start_row;
    auto start_offset = host.console_transcript_selection_start_offset;
    auto end_row = host.console_transcript_selection_end_row;
    auto end_offset = host.console_transcript_selection_end_offset;
    if (std::pair{end_row, end_offset} < std::pair{start_row, start_offset}) {
        std::swap(start_row, end_row);
        std::swap(start_offset, end_offset);
    }
    start_row = std::clamp(start_row, 0,
        static_cast<int>(host.console_transcript_rows.size()) - 1);
    end_row = std::clamp(end_row, start_row,
        static_cast<int>(host.console_transcript_rows.size()) - 1);
    std::wstring text;
    for (int index = start_row; index <= end_row; ++index) {
        const auto& row = host.console_transcript_rows[
            static_cast<std::size_t>(index)];
        const auto begin = static_cast<std::size_t>(std::clamp(
            index == start_row ? start_offset : 0, 0,
            static_cast<int>(row.text.size())));
        const auto finish = static_cast<std::size_t>(std::clamp(
            index == end_row ? end_offset : static_cast<int>(row.text.size()),
            0, static_cast<int>(row.text.size())));
        if (finish > begin) text.append(row.text, begin, finish - begin);
        if (index < end_row && row.ends_message) text.push_back(L'\n');
    }
    return copy_text_to_clipboard(owner, text);
}

void select_all_console_transcript(ClientHost& host) {
    if (host.console_transcript_rows.empty()) return;
    host.console_transcript_selection_start_row = 0;
    host.console_transcript_selection_start_offset = 0;
    host.console_transcript_selection_end_row = static_cast<int>(
        host.console_transcript_rows.size()) - 1;
    host.console_transcript_selection_end_offset = static_cast<int>(
        host.console_transcript_rows.back().text.size());
    host.console_transcript_selection_active = true;
}

void paste_console_input_text(ClientHost& host, HWND owner) {
    if (!OpenClipboard(owner)) return;
    const HANDLE clipboard = GetClipboardData(CF_TEXT);
    if (!clipboard) {
        CloseClipboard();
        return;
    }
    const auto* source = static_cast<const char*>(GlobalLock(clipboard));
    if (!source) {
        CloseClipboard();
        return;
    }
    std::wstring pasted;
    const auto available = GlobalSize(clipboard);
    pasted.reserve(std::min<std::size_t>(available, 255));
    for (std::size_t index = 0; index < available && source[index] != '\0'; ++index) {
        if (source[index] == '\r') continue;
        pasted.push_back(source[index] == '\n'
            ? L' ' : static_cast<unsigned char>(source[index]));
    }
    GlobalUnlock(clipboard);
    CloseClipboard();
    if (pasted.empty()) return;
    const auto start = std::min(host.console_input_anchor,
        host.console_input_caret);
    const auto end = std::max(host.console_input_anchor,
        host.console_input_caret);
    host.console_input_text.erase(start, end - start);
    host.console_input_caret = start;
    const auto count = std::min<std::size_t>(pasted.size(),
        255 - std::min<std::size_t>(255, host.console_input_text.size()));
    host.console_input_text.insert(start, pasted.data(), count);
    host.console_input_caret += count;
    host.console_input_anchor = host.console_input_caret;
}

RECT message_dialog_rect(const ClientHost& host, std::wstring_view text,
    POINT offset = {}) {
    int line_count = 1;
    int content_width = 0;
    std::size_t line_start = 0;
    for (std::size_t index = 0; index <= text.size(); ++index) {
        if (index != text.size() && text[index] != L'\n') continue;
        content_width = std::max(content_width, bitmap_text_width(host,
            text.substr(line_start, index - line_start), 3,
            host.window));
        if (index < text.size()) {
            ++line_count;
            line_start = index + 1;
        }
    }

    RECT client{};
    GetClientRect(host.window, &client);
    const int width = content_width + ui_units(host.window, 36);
    const int height = ui_units(host.window,
        std::max(1, host.ui_font_heights[3]) * line_count + 92);
    const auto layout = tibia::client::layout_message_dialog(
        static_cast<int>(client.right - client.left),
        static_cast<int>(client.bottom - client.top), width, height,
        static_cast<int>(offset.x), static_cast<int>(offset.y));
    return {layout.left, layout.top, layout.left + layout.width,
        layout.top + layout.height};
}

constexpr std::wstring_view account_data_warning_text =
    L"Warning!\nNever give your account number or your password to anybody!\n"
    L"Not even gamemasters or CipSoft members will need this data.\n"
    L"People asking you for this information are trying to hack you!";

RECT motd_dialog_rect(const ClientHost& host, POINT offset = {}) {
    return message_dialog_rect(host, host.motd_text, offset);
}

RECT account_data_warning_dialog_rect(const ClientHost& host, POINT offset = {}) {
    return message_dialog_rect(host, account_data_warning_text, offset);
}

std::vector<std::wstring> wrap_bitmap_lines(const ClientHost& host,
    std::wstring_view text, int max_width, std::size_t font_index,
    bool use_window_scale = true) {
    std::vector<std::wstring> lines;
    if (max_width <= 0) return lines;
    std::wstring line;
    std::wstring word;
    const auto flush_word = [&] {
        if (word.empty()) return;
        auto candidate = line;
        if (!candidate.empty()) candidate.push_back(L' ');
        candidate += word;
        if (!line.empty() && bitmap_text_width(host, candidate, font_index,
                host.window, use_window_scale) > max_width) {
            lines.push_back(std::move(line));
            line.clear();
        }
        if (!line.empty()) line.push_back(L' ');
        line += word;
        word.clear();
    };
    for (const wchar_t ch : text) {
        if (ch == L' ' || ch == L'\n' || ch == L'\r') {
            flush_word();
            if ((ch == L'\n' || ch == L'\r') && !line.empty()) {
                lines.push_back(std::move(line));
                line.clear();
            }
        } else {
            word.push_back(ch);
        }
    }
    flush_word();
    if (!line.empty()) lines.push_back(std::move(line));
    return lines;
}

std::vector<std::pair<std::wstring, int>> layout_console_message_lines(
    const ClientHost& host, std::wstring_view text, int width,
    int continuation_indent, HWND window) {
    constexpr std::size_t font_index = 3;
    std::vector<std::pair<std::wstring, int>> lines;
    if (width <= 0) return lines;
    int indent = 0;
    std::size_t start = 0;
    while (start < text.size()) {
        const int available = std::max(1, width - indent);
        std::size_t fit_end = start;
        while (fit_end < text.size() &&
            bitmap_text_width(host, text.substr(start, fit_end - start + 1),
                font_index, window) <= available)
            ++fit_end;
        std::size_t cursor = start;
        std::size_t last_break = std::wstring_view::npos;
        while (cursor < fit_end) {
            const wchar_t character = text[cursor];
            if (character == L' ' || character == L'\f' || character == L'\n' ||
                character == L'\r' || character == L'\t' || character == L'\v')
                last_break = cursor;
            if (character == L'\n') break;
            ++cursor;
        }
        std::size_t end = cursor;
        std::size_t next = cursor;
        if (cursor < fit_end && text[cursor] == L'\n') {
            next = cursor + 1;
        } else if (fit_end == text.size()) {
            end = text.size();
            next = text.size();
        } else if (last_break != std::wstring_view::npos) {
            end = last_break;
            next = last_break + 1;
        } else {
            end = fit_end;
            next = fit_end;
            if (next == start) next = std::min(start + 1, text.size());
        }
        if (end > start) lines.emplace_back(text.substr(start, end - start), indent);
        start = next;
        indent = continuation_indent;
    }
    return lines;
}

void draw_wrapped_bitmap_text(ClientHost& host, std::wstring_view text, int x, int y,
    int max_width, int max_height, int line_spacing, std::size_t font_index,
    std::uint8_t tint, bool center = false) {
    if (max_width <= 0 || max_height <= 0 || line_spacing <= 0) return;
    const int spacing = std::max(1, static_cast<int>(std::lround(
        static_cast<float>(line_spacing) * ui_scale(host.window))));
    const int line_count = std::max(1, max_height / spacing);
    const auto lines = wrap_bitmap_lines(host, text, max_width, font_index);

    for (int i = 0; i < line_count && i < static_cast<int>(lines.size()); ++i)
        draw_bitmap_text(host, lines[static_cast<std::size_t>(i)], x,
            y + i * spacing, max_width, center, font_index, tint);
}

void draw_console_message_text(ClientHost& host, std::wstring_view text, int x,
    int y, int max_width, int max_height, std::uint32_t rgb,
    const RECT& clip) {
    if (!host.renderer || text.empty() || max_width <= 0 || max_height <= 0) return;
    constexpr std::size_t font_index = 3;
    const float scale = ui_scale(host.window);
    const auto scaled = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    const int spacing = static_cast<int>(std::lround(
        static_cast<float>(host.ui_font_spacing[font_index]) * scale));
    for (std::size_t index = 0; index < text.size(); ++index) {
        wchar_t character = text[index];
        if (character == L' ') {
            x += scaled(host.ui_fonts[font_index][12].advance);
        } else {
            if (character < 32 || character > 255) character = L'?';
            const auto glyph_index = static_cast<std::size_t>(character - 32);
            const auto& glyph = host.ui_fonts[font_index][glyph_index];
            if (glyph.width > 0 && glyph.height > 0 && x < clip.right &&
                x + scaled(glyph.width) > clip.left && y < clip.bottom &&
                y + scaled(glyph.height) > clip.top) {
                host.renderer->draw_clipped(
                    console_message_glyph_texture(host, rgb, glyph_index), x, y,
                    scaled(glyph.width), scaled(glyph.height), clip.left, clip.top,
                    clip.right - clip.left, clip.bottom - clip.top);
            }
            x += scaled(glyph.advance);
        }
        if (index + 1 < text.size()) x += spacing;
        if (x >= clip.left + max_width) break;
    }
    (void)max_height;
}

void draw_skin(ClientHost& host, std::uint16_t skin_id, int x, int y,
    int width, int height);
void draw_skin_tiled(ClientHost& host, std::uint16_t skin_id, int x, int y,
    int width, int height);
void draw_skin_tiled_with_source_offset(ClientHost& host, std::uint16_t skin_id,
    int x, int y, int width, int height, int source_x, int source_y);

void apply_console_transcript_arrow_click(ClientHost& host, HWND window,
    POINT point, int pressed_part) {
    if (!host.console_transcript_scrollbar_geometry_valid) return;
    const int maximum = std::max(0, host.console_transcript_item_count -
        host.console_transcript_visible_rows);
    if (maximum <= 0) return;
    const auto& viewport = host.console_transcript_scrollbar_viewport;
    const int first_visible = std::clamp(maximum -
        host.console_transcript_scroll_from_bottom, 0, maximum);
    const auto geometry = tibia::client::resolve_console_message_scrollbar_geometry(
        static_cast<int>(viewport.right - viewport.left),
        static_cast<int>(viewport.bottom - viewport.top),
        host.console_transcript_item_count,
        host.console_transcript_visible_rows, first_visible);
    const auto& arrow = pressed_part == 0
        ? geometry.up_arrow : geometry.down_arrow;
    const RECT button{viewport.left + arrow.x, viewport.top + arrow.y,
        viewport.left + arrow.x + arrow.width,
        viewport.top + arrow.y + arrow.height};
    if (point.x < button.left || point.x > button.right ||
        point.y < button.top || point.y > button.bottom)
        return;
    host.console_transcript_scroll_from_bottom = std::clamp(
        host.console_transcript_scroll_from_bottom +
            (pressed_part == 0 ? 1 : -1), 0, maximum);
    InvalidateRect(window, nullptr, FALSE);
}

template<typename Messages, typename LineAccessor, typename ColorResolver>
void draw_console_transcript(ClientHost& host,
    const tibia::client::ConsolePanelLayout& layout, int console_x, int console_y,
    const Messages& messages, bool visible, LineAccessor line_accessor,
    ColorResolver color_resolver, bool selectable) {
    if (!host.window || !host.renderer || !visible) {
        host.console_transcript_item_count = 0;
        host.console_transcript_visible_rows = 0;
        host.console_transcript_rows.clear();
        host.console_transcript_scrollbar_geometry_valid = false;
        return;
    }
    const float scale = ui_scale(host.window);
    const auto scaled = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    const auto& view = layout.message_view;
    const RECT viewport{console_x + view.x, console_y + view.y,
        console_x + view.x + view.width, console_y + view.y + view.height};
    const int line_height = scaled(std::max(1, host.ui_font_line_advances[3]));
    const int first_line_height = scaled(std::max(1, host.ui_font_heights[3]));
    const RECT clip = viewport;
    std::vector<ConsoleTranscriptRow> rows;
    for (const auto& message : messages) {
        const auto& line = line_accessor(message);
        const auto color = color_resolver(line.mode);
        if (!color) continue;
        const auto formatted = line.format(host.console_timestamps_enabled,
            host.console_levels_enabled);
        const auto text = decode_ui_string(formatted);
        const auto message = text;
        const auto speaker_name = decode_ui_string(line.speaker_name);
        const auto statement_text = decode_ui_string(line.text);
        const int continuation_indent = line.mode >= 0x12 && line.mode <= 0x16
            ? 0 : scaled(10);
        const auto message_lines = layout_console_message_lines(host, text,
            clip.right - clip.left, continuation_indent, host.window);
        for (std::size_t index = 0; index < message_lines.size(); ++index) {
            rows.push_back({message_lines[index].first, *color,
                message_lines[index].second, message, speaker_name,
                line.speaker_id, statement_text,
                index + 1 == message_lines.size()});
        }
    }
    const int visible_rows = console_transcript_visible_rows(host,
        static_cast<int>(rows.size()), static_cast<int>(clip.bottom - clip.top));
    const auto previous_max_scroll = std::max(0,
        host.console_transcript_item_count - host.console_transcript_visible_rows);
    const auto previous_first_visible = previous_max_scroll -
        host.console_transcript_scroll_from_bottom;
    const auto row_count = static_cast<int>(std::min<std::size_t>(
        rows.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
    const auto max_scroll = std::max(0, row_count - visible_rows);
    if (host.console_transcript_scroll_from_bottom > 0) {
        host.console_transcript_scroll_from_bottom = std::max(0,
            max_scroll - previous_first_visible);
    }
    host.console_transcript_item_count = row_count;
    host.console_transcript_visible_rows = visible_rows;
    host.console_transcript_scroll_from_bottom = std::clamp(
        host.console_transcript_scroll_from_bottom, 0,
        max_scroll);
    const auto first_visible = static_cast<std::size_t>(max_scroll -
        host.console_transcript_scroll_from_bottom);
    host.console_transcript_rows = selectable
        ? rows : std::vector<ConsoleTranscriptRow>{};
    host.console_transcript_clip = clip;
    const auto last_visible = rows.empty() ? 0 : std::min(row_count - 1,
        static_cast<int>(first_visible) + visible_rows - 1);
    host.console_transcript_line_height = line_height;
    host.console_transcript_first_line_height = first_line_height;
    if (!selectable) host.console_transcript_selection_active = false;
    int y = clip.bottom - first_line_height;
    for (int index = last_visible; !rows.empty() &&
         index >= static_cast<int>(first_visible) && y >= clip.top; --index) {
        const auto& row = rows[static_cast<std::size_t>(index)];
        if (selectable && host.console_transcript_selection_active) {
            auto start_row = host.console_transcript_selection_start_row;
            auto start_offset = host.console_transcript_selection_start_offset;
            auto end_row = host.console_transcript_selection_end_row;
            auto end_offset = host.console_transcript_selection_end_offset;
            if (std::pair{end_row, end_offset} < std::pair{start_row, start_offset}) {
                std::swap(start_row, end_row);
                std::swap(start_offset, end_offset);
            }
            if (index >= start_row && index <= end_row) {
                const auto begin = static_cast<std::size_t>(std::clamp(
                    index == start_row ? start_offset : 0, 0,
                    static_cast<int>(row.text.size())));
                const auto finish = static_cast<std::size_t>(std::clamp(
                    index == end_row ? end_offset :
                        static_cast<int>(row.text.size()),
                    0, static_cast<int>(row.text.size())));
                if (finish > begin) {
                    const int selection_height = start_row == end_row &&
                        index == start_row ? first_line_height : line_height;
                    const int selection_x = clip.left + row.indent +
                        bitmap_text_width(host,
                            std::wstring_view(row.text).substr(0, begin), 3,
                            host.window);
                    const int selection_width = bitmap_text_width(host,
                        std::wstring_view(row.text).substr(begin, finish - begin),
                        3, host.window);
                    if (selection_width > 0)
                        host.renderer->draw_clipped(console_input_selection_texture,
                            selection_x, y, selection_width, selection_height,
                            clip.left, clip.top, clip.right - clip.left,
                            clip.bottom - clip.top);
                }
            }
        }
        draw_console_message_text(host, row.text, clip.left + row.indent, y,
            clip.right - clip.left - row.indent, first_line_height, row.color, clip);
        y -= line_height;
    }
    const auto& scrollbar_area = layout.message_scrollbar;
    const RECT scrollbar_viewport{console_x + scrollbar_area.x,
        console_y + scrollbar_area.y,
        console_x + scrollbar_area.x + scrollbar_area.width,
        console_y + scrollbar_area.y + scrollbar_area.height};
    const auto scrollbar = tibia::client::resolve_console_message_scrollbar_geometry(
        scrollbar_area.width, scrollbar_area.height,
        static_cast<int>(rows.size()), visible_rows, static_cast<int>(first_visible));
    host.console_transcript_scrollbar_viewport = scrollbar_viewport;
    host.console_transcript_scrollbar_geometry_valid = true;
    const RECT track{scrollbar_viewport.left + scrollbar.track.x,
        scrollbar_viewport.top + scrollbar.track.y,
        scrollbar_viewport.left + scrollbar.track.x + scrollbar.track.width,
        scrollbar_viewport.top + scrollbar.track.y + scrollbar.track.height};
    draw_skin_tiled(host, 153, track.left, track.top,
        track.right - track.left, track.bottom - track.top);
    if (scrollbar.thumb_visible) {
        const int thumb_top = scrollbar_viewport.top + scrollbar.thumb.y;
        const int thumb_height = scrollbar.thumb.height;
        draw_skin(host, 154, scrollbar_viewport.left, thumb_top,
            scrollbar_area.width, tibia::client::ui_skins[154].height);
        draw_skin_tiled(host, 155, scrollbar_viewport.left,
            thumb_top + tibia::client::ui_skins[154].height,
            scrollbar_area.width,
            thumb_height - tibia::client::ui_skins[154].height -
                tibia::client::ui_skins[156].height);
        draw_skin(host, 156, scrollbar_viewport.left,
            thumb_top + thumb_height - tibia::client::ui_skins[156].height,
            scrollbar_area.width, tibia::client::ui_skins[156].height);
    }
    const RECT up_arrow{scrollbar_viewport.left + scrollbar.up_arrow.x,
        scrollbar_viewport.top + scrollbar.up_arrow.y,
        scrollbar_viewport.left + scrollbar.up_arrow.x + scrollbar.up_arrow.width,
        scrollbar_viewport.top + scrollbar.up_arrow.y + scrollbar.up_arrow.height};
    const RECT down_arrow{scrollbar_viewport.left + scrollbar.down_arrow.x,
        scrollbar_viewport.top + scrollbar.down_arrow.y,
        scrollbar_viewport.left + scrollbar.down_arrow.x + scrollbar.down_arrow.width,
        scrollbar_viewport.top + scrollbar.down_arrow.y + scrollbar.down_arrow.height};
    const bool left_button_down = (host.mouse_buttons & 1U) != 0;
    const bool up_pressed = left_button_down &&
        host.pressed_console_transcript_scrollbar_part == 0 &&
        host.mouse.x >= up_arrow.left && host.mouse.x <= up_arrow.right &&
        host.mouse.y >= up_arrow.top && host.mouse.y <= up_arrow.bottom;
    const bool down_pressed = left_button_down &&
        host.pressed_console_transcript_scrollbar_part == 1 &&
        host.mouse.x >= down_arrow.left && host.mouse.x <= down_arrow.right &&
        host.mouse.y >= down_arrow.top && host.mouse.y <= down_arrow.bottom;
    draw_skin(host, up_pressed ? 158 : 157,
        scrollbar_viewport.left + scrollbar.up_arrow.x,
        scrollbar_viewport.top + scrollbar.up_arrow.y,
        scrollbar.up_arrow.width, scrollbar.up_arrow.height);
    draw_skin(host, down_pressed ? 160 : 159,
        scrollbar_viewport.left + scrollbar.down_arrow.x,
        scrollbar_viewport.top + scrollbar.down_arrow.y,
        scrollbar.down_arrow.width, scrollbar.down_arrow.height);
}

void draw_report_violation_transcript(ClientHost& host,
    const tibia::client::ConsolePanelLayout& layout, int console_x, int console_y,
    const tibia::client::ReportViolationBufferSnapshot& snapshot) {
    draw_console_transcript(host, layout, console_x, console_y, snapshot.messages,
        snapshot.open,
        [](const auto& message) -> const tibia::client::MessageLine& {
            return message.line;
        },
        [](std::uint8_t mode) -> std::optional<std::uint32_t> {
            return report_violation_message_rgb(mode);
        }, true);
}

void draw_primary_console_transcript(ClientHost& host,
    const tibia::client::ConsolePanelLayout& layout, int console_x, int console_y,
    const tibia::client::GameWorldSnapshot& snapshot, bool visible) {
    draw_console_transcript(host, layout, console_x, console_y,
        snapshot.primary_console_messages, visible,
        [](const tibia::client::MessageLine& message)
            -> const tibia::client::MessageLine& { return message; },
        console_message_rgb, true);
}

void draw_game_status_message(ClientHost& host,
    const tibia::client::GameWorldSnapshot& snapshot,
    const tibia::client::MapWindowLayout& map) {
    if (!snapshot.status_visible || snapshot.status_text.empty()) return;
    constexpr std::size_t font_index = 2;
    const int line_spacing = std::max(1, host.ui_font_heights[font_index]);
    const int line_height = line_spacing;
    const auto text = decode_ui_string(snapshot.status_text);
    const auto lines = wrap_bitmap_lines(host, text, map.width, font_index, false);
    if (lines.empty()) return;
    const int visible_lines = std::min(static_cast<int>(lines.size()),
        std::max(1, map.height / line_height));
    const int text_height = visible_lines * line_height;
    const int top = map.y + map.height - text_height;
    for (int index = 0; index < visible_lines; ++index)
        draw_bitmap_text(host, lines[static_cast<std::size_t>(index)], map.x,
            top + index * line_height, map.width, true, font_index, 255, false);
}

tibia::resources::TibiaSprImage crop_picture(const std::vector<std::uint8_t>& bytes,
    const tibia::resources::TibiaPicFile& pic, std::uint16_t picture_id,
    int x, int y, int width, int height) {
    if (width <= 0 || height <= 0 || width > 32 || height > 32)
        throw std::invalid_argument("UI image crop must fit in one 32x32 renderer tile");
    const auto& picture = pic.pictures.at(static_cast<std::size_t>(picture_id - 1));
    tibia::resources::TibiaSprImage output;
    output.color_key = picture.color_key;
    for (std::size_t pixel = 0; pixel < 32U * 32U; ++pixel) {
        const auto offset = pixel * 4;
        output.rgba[offset] = picture.color_key.red;
        output.rgba[offset + 1] = picture.color_key.green;
        output.rgba[offset + 2] = picture.color_key.blue;
        output.rgba[offset + 3] = 0;
    }
    std::array<std::array<std::optional<tibia::resources::TibiaSprImage>, 2>, 2> tile_cache{};
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            const int source_x = x + dx;
            const int source_y = y + dy;
            if (source_x < 0 || source_y < 0 || source_x >= picture.tiles_x * 32 ||
                source_y >= picture.tiles_y * 32) continue;
            const auto tile_x = source_x / 32;
            const auto tile_y = source_y / 32;
            auto& cached = tile_cache[static_cast<std::size_t>(tile_y - y / 32)]
                [static_cast<std::size_t>(tile_x - x / 32)];
            if (!cached) cached = tibia::resources::decode_tibia_pic_tile(bytes, pic, picture_id,
                static_cast<std::uint16_t>(tile_x), static_cast<std::uint16_t>(tile_y));
            const auto source = (static_cast<std::size_t>(source_y % 32) * 32U +
                static_cast<std::size_t>(source_x % 32)) * 4U;
            const auto destination = (static_cast<std::size_t>(dy) * 32U +
                static_cast<std::size_t>(dx)) * 4U;
            std::copy_n(cached->rgba.begin() + static_cast<std::ptrdiff_t>(source), 4,
                output.rgba.begin() + static_cast<std::ptrdiff_t>(destination));
        }
    }
    return output;
}

tibia::resources::TibiaSprImage prepare_ui_crop(
    const tibia::resources::TibiaSprImage& source, int source_width, int source_height) {
    auto image = source;
    image.content_width = static_cast<std::uint8_t>(std::clamp(source_width, 1, 32));
    image.content_height = static_cast<std::uint8_t>(std::clamp(source_height, 1, 32));
    return image;
}

void upload_bitmap_fonts(ClientHost& host, const std::vector<std::uint8_t>& bytes,
    const tibia::resources::TibiaPicFile& pic) {
    constexpr int characters_per_row = 32;
    struct FontSource { std::uint16_t picture; int cell_width; int cell_height; };
    constexpr std::array<FontSource, 4> sources{{{3, 8, 16}, {5, 16, 16}, {6, 8, 8}, {8, 8, 16}}};
    for (std::size_t font_index = 0; font_index < sources.size(); ++font_index) {
        host.ui_font_images[font_index].resize(224);
        const auto source = sources[font_index];
        host.ui_font_cell_widths[font_index] = source.cell_width;
        host.ui_font_cell_heights[font_index] = source.cell_height;
        for (std::size_t index = 0; index < host.ui_fonts[font_index].size(); ++index) {
            const int source_x = static_cast<int>(index % characters_per_row) * source.cell_width;
            const int source_y = static_cast<int>(index / characters_per_row) * source.cell_height;
            auto glyph_image = crop_picture(bytes, pic, source.picture, source_x, source_y,
                source.cell_width, source.cell_height);
            int rightmost = -1;
            int bottommost = -1;
            for (int gy = 0; gy < source.cell_height; ++gy) {
                for (int gx = 0; gx < source.cell_width; ++gx) {
                    const auto alpha = glyph_image.rgba[(static_cast<std::size_t>(gy) * 32U +
                        static_cast<std::size_t>(gx)) * 4U + 3U];
                    if (alpha) {
                        rightmost = std::max(rightmost, gx);
                        bottommost = std::max(bottommost, gy);
                    }
                }
            }
            auto& glyph = host.ui_fonts[font_index][index];
            glyph.texture = ui_glyph_texture_base + static_cast<std::uint32_t>(font_index * 0x1000U + index);
            glyph.width = std::max(1, rightmost + 1);
            glyph.height = std::max(1, bottommost + 1);
            host.ui_font_heights[font_index] = std::max(
                host.ui_font_heights[font_index], glyph.height);
            glyph.advance = rightmost < 0 ? source.cell_width : glyph.width;
            if (font_index == 1)
                host.talkbox_font_glyphs[index] = {glyph.width, glyph.height, glyph.advance};
            auto glyph_crop = prepare_ui_crop(glyph_image, glyph.width, glyph.height);
            host.renderer->upload(glyph.texture, glyph_crop);
            host.ui_font_images[font_index][index] = glyph_crop;
            {
                for (std::size_t tint = 0; tint < ui_font_text_shades.size(); ++tint) {
                    auto colored = glyph_crop;
                    const auto shade = ui_font_text_shades[tint];
                    for (std::size_t pixel = 0; pixel < 32U * 32U; ++pixel) {
                        const auto offset = pixel * 4U;
                        if (colored.rgba[offset + 3] == 0) continue;
                        for (std::size_t channel = 0; channel < 3; ++channel)
                            colored.rgba[offset + channel] = static_cast<std::uint8_t>(
                                static_cast<unsigned>(colored.rgba[offset + channel]) * shade / 255U);
                    }
                    auto& tinted_glyph = host.ui_font_tints[font_index][tint][index];
                    tinted_glyph = glyph;
                    tinted_glyph.texture = ui_tinted_glyph_texture_base +
                        static_cast<std::uint32_t>(font_index * 0x10000U + tint * 0x1000U + index);
                    host.renderer->upload(tinted_glyph.texture, colored);
                }
            }
        }
    }
}

void upload_ui_skins(ClientHost& host, const std::vector<std::uint8_t>& bytes,
    const tibia::resources::TibiaPicFile& pic) {
    for (std::size_t index = 0; index < tibia::client::ui_skins.size(); ++index) {
        const auto skin_id = static_cast<std::uint16_t>(index);
        const auto& skin = tibia::client::ui_skins[skin_id];
        if (skin.width <= 0 || skin.height <= 0) continue;
        const int columns = (skin.width + 31) / 32;
        const int rows = (skin.height + 31) / 32;
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                const int source_x = skin.x + column * 32;
                const int source_y = skin.y + row * 32;
                const int width = std::min(32, skin.width - column * 32);
                const int height = std::min(32, skin.height - row * 32);
                const auto texture = ui_skin_texture_base +
                    static_cast<std::uint32_t>(skin_id) * 16U +
                    static_cast<std::uint32_t>(row * 4 + column);
                const auto crop = prepare_ui_crop(
                    crop_picture(bytes, pic, 4, source_x, source_y, width, height), width, height);
                host.renderer->upload(texture, crop);
                host.skins[skin_id].push_back({texture, column * 32, row * 32, width, height});
            }
        }
    }
}

void upload_login_field_bevel(ClientHost& host, const std::vector<std::uint8_t>& bytes,
    const tibia::resources::TibiaPicFile& pic) {
    using Pixel = std::array<std::uint8_t, 4>;
    const auto read_line = [&](int x, int y, int count, bool horizontal, auto& output) {
        for (int offset = 0; offset < count;) {
            const int length = std::min(32, count - offset);
            const int source_x = x + (horizontal ? offset : 0);
            const int source_y = y + (horizontal ? 0 : offset);
            const auto image = crop_picture(bytes, pic, 4, source_x, source_y,
                horizontal ? length : 1, horizontal ? 1 : length);
            for (int i = 0; i < length; ++i) {
                const auto source_offset = static_cast<std::size_t>(horizontal ? i : i * 32) * 4U;
                output[static_cast<std::size_t>(offset + i)] = {
                    image.rgba[source_offset], image.rgba[source_offset + 1],
                    image.rgba[source_offset + 2], image.rgba[source_offset + 3]};
            }
            offset += length;
        }
    };
    read_line(0, 96, 86, true, host.login_bevel_top);
    read_line(2, 197, 86, true, host.login_bevel_bottom);
    read_line(0, 98, 16, false, host.login_bevel_left);
    read_line(276, 0, 16, false, host.login_bevel_right);

    host.login_bevel_palette.clear();
    const auto add_colors = [&](const auto& line) {
        for (const auto& pixel : line) {
            if (pixel[3] == 0) continue;
            if (std::find(host.login_bevel_palette.begin(), host.login_bevel_palette.end(), pixel) ==
                host.login_bevel_palette.end()) host.login_bevel_palette.push_back(pixel);
        }
    };
    add_colors(host.login_bevel_top);
    add_colors(host.login_bevel_bottom);
    add_colors(host.login_bevel_left);
    add_colors(host.login_bevel_right);

    for (std::size_t i = 0; i < host.login_bevel_palette.size(); ++i) {
        const auto& pixel = host.login_bevel_palette[i];
        tibia::resources::TibiaSprImage image{};
        image.color_key = {pixel[0], pixel[1], pixel[2]};
        for (std::size_t offset = 0; offset < image.rgba.size(); offset += 4) {
            std::copy(pixel.begin(), pixel.end(), image.rgba.begin() +
                static_cast<std::ptrdiff_t>(offset));
        }
        host.renderer->upload(login_bevel_texture_base + static_cast<std::uint32_t>(i), image);
    }
}

void draw_login_field_bevel(ClientHost& host, const RECT& field) {
    if (!host.renderer) return;
    const float scale = ui_scale(host.window);
    const auto position = [scale](int value) {
        return static_cast<int>(std::lround(static_cast<float>(value) * scale));
    };
    const auto draw_pixel = [&](const std::array<std::uint8_t, 4>& pixel,
        int x, int y, int width, int height) {
        if (pixel[3] == 0 || width <= 0 || height <= 0) return;
        const auto it = std::find(host.login_bevel_palette.begin(),
            host.login_bevel_palette.end(), pixel);
        if (it == host.login_bevel_palette.end()) return;
        const auto texture = login_bevel_texture_base + static_cast<std::uint32_t>(
            std::distance(host.login_bevel_palette.begin(), it));
        host.renderer->draw(texture, x, y, width, height);
    };
    const int width = field.right - field.left;
    const int height = field.bottom - field.top;
    for (int i = 0; i < static_cast<int>(host.login_bevel_top.size()); ++i) {
        const int x = position(i);
        const int next_x = position(i + 1);
        draw_pixel(host.login_bevel_top[static_cast<std::size_t>(i)],
            field.left + x, field.top, next_x - x, position(1));
        const int bottom_y = position(15);
        draw_pixel(host.login_bevel_bottom[static_cast<std::size_t>(i)],
            field.left + x, field.top + bottom_y, next_x - x, height - bottom_y);
    }
    for (int i = 0; i < static_cast<int>(host.login_bevel_left.size()); ++i) {
        const int y = position(i);
        const int next_y = position(i + 1);
        const int right_x = position(85);
        draw_pixel(host.login_bevel_left[static_cast<std::size_t>(i)],
            field.left, field.top + y, position(1), next_y - y);
        draw_pixel(host.login_bevel_right[static_cast<std::size_t>(i)],
            field.left + right_x, field.top + y, width - right_x, next_y - y);
    }
}

void draw_skin(ClientHost& host, std::uint16_t skin_id, int x, int y, int width, int height) {
    if (!host.renderer || skin_id >= host.skins.size() || width <= 0 || height <= 0) return;
    const auto& skin = tibia::client::ui_skins[skin_id];
    if (skin.width <= 0 || skin.height <= 0) return;
    for (const auto& chunk : host.skins[skin_id]) {
        const int left = chunk.source_x * width / skin.width;
        const int top = chunk.source_y * height / skin.height;
        const int right = (chunk.source_x + chunk.width) * width / skin.width;
        const int bottom = (chunk.source_y + chunk.height) * height / skin.height;
        host.renderer->draw(chunk.texture, x + left, y + top,
            std::max(1, right - left), std::max(1, bottom - top));
    }
}

void draw_skin_clipped(ClientHost& host, std::uint16_t skin_id, int x, int y,
    int width, int height, const RECT& clip) {
    if (!host.renderer || skin_id >= host.skins.size() || width <= 0 || height <= 0) return;
    const auto& skin = tibia::client::ui_skins[skin_id];
    if (skin.width <= 0 || skin.height <= 0) return;
    for (const auto& chunk : host.skins[skin_id]) {
        const int left = chunk.source_x * width / skin.width;
        const int top = chunk.source_y * height / skin.height;
        const int right = (chunk.source_x + chunk.width) * width / skin.width;
        const int bottom = (chunk.source_y + chunk.height) * height / skin.height;
        host.renderer->draw_clipped(chunk.texture, x + left, y + top,
            std::max(1, right - left), std::max(1, bottom - top), clip.left, clip.top,
            clip.right - clip.left, clip.bottom - clip.top);
    }
}

void draw_skin_tiled(ClientHost& host, std::uint16_t skin_id, int x, int y,
    int width, int height) {
    if (skin_id >= tibia::client::ui_skins.size()) return;
    const auto& skin = tibia::client::ui_skins[skin_id];
    if (skin.width <= 0 || skin.height <= 0) return;
    for (int tile_y = 0; tile_y < height; tile_y += skin.height) {
        for (int tile_x = 0; tile_x < width; tile_x += skin.width) {
            const int tile_width = std::min(skin.width, width - tile_x);
            const int tile_height = std::min(skin.height, height - tile_y);
            const RECT clip{x + tile_x, y + tile_y,
                x + tile_x + tile_width, y + tile_y + tile_height};
            draw_skin_clipped(host, skin_id, x + tile_x, y + tile_y,
                skin.width, skin.height, clip);
        }
    }
}

void draw_skin_tiled_with_source_offset(ClientHost& host, std::uint16_t skin_id,
    int x, int y, int width, int height, int source_x, int source_y) {
    if (skin_id >= tibia::client::ui_skins.size()) return;
    const auto& skin = tibia::client::ui_skins[skin_id];
    if (skin.width <= 0 || skin.height <= 0 || width <= 0 || height <= 0) return;
    source_x = ((source_x % skin.width) + skin.width) % skin.width;
    source_y = ((source_y % skin.height) + skin.height) % skin.height;
    const int bounds_left = x;
    const int bounds_top = y;
    const int bounds_right = x + width;
    const int bounds_bottom = y + height;
    for (int tile_y = -source_y; tile_y < height; tile_y += skin.height) {
        for (int tile_x = -source_x; tile_x < width; tile_x += skin.width) {
            const int left = std::max(bounds_left, x + tile_x);
            const int top = std::max(bounds_top, y + tile_y);
            const int right = std::min(bounds_right, x + tile_x + skin.width);
            const int bottom = std::min(bounds_bottom, y + tile_y + skin.height);
            if (left >= right || top >= bottom) continue;
            draw_skin_clipped(host, skin_id, x + tile_x, y + tile_y,
                skin.width, skin.height, RECT{left, top, right, bottom});
        }
    }
}

void draw_console_panel_frame(ClientHost& host, int x, int y, int width, int height) {
    constexpr std::uint16_t top_left_skin = 177;
    constexpr std::uint16_t top_right_skin = 178;
    constexpr std::uint16_t bottom_left_skin = 179;
    constexpr std::uint16_t bottom_right_skin = 180;
    constexpr std::uint16_t left_edge_skin = 181;
    constexpr std::uint16_t right_edge_skin = 182;
    constexpr std::uint16_t top_edge_skin = 183;
    constexpr std::uint16_t bottom_edge_skin = 184;
    constexpr std::uint16_t body_skin = 185;
    if (width <= 0 || height <= 0) return;
    const auto& top_left = tibia::client::ui_skins[top_left_skin];
    const auto& top_right = tibia::client::ui_skins[top_right_skin];
    const auto& bottom_left = tibia::client::ui_skins[bottom_left_skin];
    const auto& bottom_right = tibia::client::ui_skins[bottom_right_skin];
    const auto& left_edge = tibia::client::ui_skins[left_edge_skin];
    const auto& right_edge = tibia::client::ui_skins[right_edge_skin];
    const auto& top_edge = tibia::client::ui_skins[top_edge_skin];
    const auto& bottom_edge = tibia::client::ui_skins[bottom_edge_skin];
    const int left = left_edge.width;
    const int right = right_edge.width;
    const int top = top_edge.height;
    const int bottom = bottom_edge.height;
    const int middle_width = std::max(0, width - left - right);
    const int middle_height = std::max(0, height - top - bottom);
    draw_skin_tiled_with_source_offset(host, body_skin, x + left, y + top,
        middle_width, middle_height, 0, y %
            tibia::client::ui_skins[body_skin].height);
    draw_skin_tiled(host, left_edge_skin, x, y + top,
        left, middle_height);
    draw_skin_tiled(host, right_edge_skin, x + width - right, y + top,
        right, middle_height);
    draw_skin_tiled(host, top_edge_skin, x + left, y,
        middle_width, top);
    draw_skin_tiled(host, bottom_edge_skin, x + left, y + height - bottom,
        middle_width, bottom);
    draw_skin(host, top_left_skin, x, y, top_left.width, top_left.height);
    draw_skin(host, top_right_skin, x + width - top_right.width, y,
        top_right.width, top_right.height);
    draw_skin(host, bottom_left_skin, x, y + height - bottom_left.height,
        bottom_left.width, bottom_left.height);
    draw_skin(host, bottom_right_skin, x + width - bottom_right.width,
        y + height - bottom_right.height, bottom_right.width,
        bottom_right.height);
}

void draw_ui_scrollbar_parts(ClientHost& host, const RECT& list,
    const tibia::client::UiScrollbarGeometry& geometry,
    bool pressed_up, bool pressed_down) {
    const auto up = options_list_scrollbar_part_rect(list, geometry.up_arrow);
    const auto down = options_list_scrollbar_part_rect(list, geometry.down_arrow);
    const auto track = options_list_scrollbar_part_rect(list, geometry.track);
    const auto s = [host_window = host.window](int value) {
        return ui_units(host_window, value);
    };

    if (geometry.thumb_visible) {
        draw_skin_tiled(host, 153, track.left, track.top,
            track.right - track.left, track.bottom - track.top);
    }
    if (geometry.thumb_visible) {
        const auto thumb = options_list_scrollbar_part_rect(list, geometry.thumb);
        const int thumb_height = static_cast<int>(thumb.bottom - thumb.top);
        const int cap = std::min(s(6), thumb_height / 2);
        draw_skin(host, 154, thumb.left, thumb.top,
            thumb.right - thumb.left, cap);
        draw_skin_tiled(host, 155, thumb.left, thumb.top + cap,
            thumb.right - thumb.left, thumb.bottom - thumb.top - 2 * cap);
        draw_skin(host, 156, thumb.left, thumb.bottom - cap,
            thumb.right - thumb.left, cap);
    }

    draw_skin(host, pressed_up ? 158 : 157, up.left, up.top,
        up.right - up.left, up.bottom - up.top);
    draw_skin(host, pressed_down ? 160 : 159, down.left, down.top,
        down.right - down.left, down.bottom - down.top);
}

void draw_options_list_scrollbar(ClientHost& host, const RECT& list,
    ClientHost::ListScrollbarTarget target, std::size_t item_count,
    int visible_items, int first_visible) {
    const auto geometry = options_list_scrollbar_geometry(host.window, list,
        item_count, visible_items, first_visible);
    draw_ui_scrollbar_parts(host, list, geometry,
        host.list_scrollbar_target == target && host.pressed_list_scrollbar_part == 0,
        host.list_scrollbar_target == target && host.pressed_list_scrollbar_part == 1);
}

void draw_console_text_input(ClientHost& host,
    const tibia::client::ConsoleTextInputVisual& input) {
    if (!host.renderer || !host.window) return;
    const auto background = input.focused
        ? console_input_focused_texture : login_field_background_texture;
    host.renderer->draw(background, input.rect.x, input.rect.y,
        input.rect.width, input.rect.height);
    draw_skin_tiled(host, 9, input.rect.x, input.rect.y,
        input.rect.width, ui_units(host.window, 1));
    draw_skin_tiled(host, 8, input.rect.x,
        input.rect.y + input.rect.height - ui_units(host.window, 1),
        input.rect.width, ui_units(host.window, 1));
    draw_skin_tiled(host, 11, input.rect.x, input.rect.y + ui_units(host.window, 1),
        ui_units(host.window, 1), input.rect.height - 2 * ui_units(host.window, 1));
    draw_skin_tiled(host, 10,
        input.rect.x + input.rect.width - ui_units(host.window, 1),
        input.rect.y + ui_units(host.window, 1), ui_units(host.window, 1),
        input.rect.height - 2 * ui_units(host.window, 1));
    const int text_x = input.rect.x + input.inset;
    const int text_y = input.rect.y + ui_units(host.window, input.text_y_offset);
    const auto text = std::wstring_view(host.console_input_text).substr(0,
        input.max_length);
    const auto caret = std::min(host.console_input_caret, text.size());
    const auto anchor = std::min(host.console_input_anchor, text.size());
    const auto selection_start = std::min(caret, anchor);
    const auto selection_end = std::max(caret, anchor);
    if (input.focused && selection_start != selection_end) {
        const int selection_x = text_x + bitmap_text_width(host,
            text.substr(0, selection_start), input.font_index, host.window);
        const int selection_width = bitmap_text_width(host,
            text.substr(selection_start, selection_end - selection_start),
            input.font_index, host.window);
        host.renderer->draw(console_input_selection_texture, selection_x,
            text_y, std::max(0, std::min(selection_width,
                input.rect.x + input.rect.width - input.inset - selection_x)),
            ui_units(host.window, host.ui_font_heights[input.font_index]));
    }
    draw_bitmap_text(host, text, text_x, text_y,
        std::max(0, input.rect.width - input.inset * 2), false,
        input.font_index, input.text_tint);
    if (!input.focused || ((GetTickCount64() / 500U) & 1U) != 0) return;
    const int caret_x = std::min(input.rect.x + input.rect.width - input.inset,
        text_x + bitmap_text_width(host, text.substr(0, caret), input.font_index,
            host.window));
    host.renderer->draw(login_caret_texture, caret_x, text_y, 1,
        ui_units(host.window, host.ui_font_heights[input.font_index]));
}

void draw_combat_icon_controls(ClientHost& host,
    const auto& icons, int origin_x, int origin_y) {
    for (std::size_t index = 0;
        index < icons.size(); ++index) {
        const auto& control = icons[index];
        const bool selected = index < 3
            ? host.combat_attack_mode == static_cast<std::int32_t>(index + 1)
            : index == 3 ? host.combat_chase_mode == 0
            : index == 4 ? host.combat_chase_mode == 1
            : !host.combat_secure_mode;
        const bool pressed = host.pressed_combat_control == static_cast<int>(index) || selected;
        draw_skin(host, static_cast<std::uint16_t>(pressed
                ? control.pressed_skin : control.normal_skin),
            origin_x + control.x, origin_y + control.y,
            tibia::client::combat_icon_size, tibia::client::combat_icon_size);
    }
}

void draw_combat_text_controls(ClientHost& host,
    const auto& actions, int origin_x, int origin_y) {
    for (std::size_t index = 0; index < actions.size(); ++index) {
        const auto& action = actions[index];
        const bool pressed = host.pressed_combat_action == static_cast<int>(index);
        const int x = origin_x + action.x;
        const int y = origin_y + action.y;
        draw_skin(host, static_cast<std::uint16_t>(pressed
            ? action.pressed_skin : action.normal_skin), x, y, 43, 20);
        draw_bitmap_text(host, action.label, x, y + 6, 43, true, 2);
    }
}

void draw_combat_control_panel(ClientHost& host, int sidebar_x, bool compact) {
    if (compact) {
        const int x = sidebar_x + tibia::client::compact_combat_controls_x;
        const int y = tibia::client::compact_combat_controls_y;
        draw_combat_icon_controls(host, tibia::client::compact_combat_icons, x, y);
        draw_combat_text_controls(host, tibia::client::compact_combat_actions, x, y);
        for (std::size_t index = 0;
            index < tibia::client::compact_combat_utility_icons.size(); ++index) {
            const auto& control = tibia::client::compact_combat_utility_icons[index];
            const bool pressed = host.pressed_combat_action ==
                static_cast<int>(index + tibia::client::compact_combat_actions.size());
            draw_skin(host, static_cast<std::uint16_t>(pressed
                ? control.pressed_skin : control.normal_skin),
                x + control.x, y + control.y, tibia::client::combat_icon_size,
                tibia::client::combat_icon_size);
        }
        return;
    }

    const int x = sidebar_x + tibia::client::expanded_combat_controls_x;
    const int y = tibia::client::expanded_combat_controls_y;
    draw_combat_icon_controls(host, tibia::client::expanded_combat_icons, x, y);
    draw_combat_text_controls(host, tibia::client::expanded_combat_actions, x, y);
}

void draw_automap_panel(ClientHost& host, int sidebar_x, int panel_y,
    const tibia::map::AutomapView* automap_view) {
    const auto layout = tibia::client::layout_automap_panel(sidebar_x, panel_y);
    draw_skin_tiled(host, 51, layout.panel.x, layout.panel.y,
        layout.panel.width, layout.panel.height);
    host.renderer->draw(automap_empty_viewport_texture, layout.viewport.x,
        layout.viewport.y, layout.viewport.width, layout.viewport.height);
    if (automap_view != nullptr) {
        constexpr std::uint32_t texture_base = 0xf5100000U;
        constexpr int tile_size = static_cast<int>(tibia::resources::TibiaSprImage::width);
        for (int tile_y = 0; tile_y < 4; ++tile_y) {
            for (int tile_x = 0; tile_x < 4; ++tile_x) {
                tibia::resources::TibiaSprImage image{};
                for (int pixel_y = 0; pixel_y < tile_size; ++pixel_y) {
                    for (int pixel_x = 0; pixel_x < tile_size; ++pixel_x) {
                        const auto view_x = tile_x * tile_size + pixel_x;
                        const auto view_y = tile_y * tile_size + pixel_y;
                        if (view_x >= static_cast<int>(tibia::map::automap_view_size) ||
                            view_y >= static_cast<int>(tibia::map::automap_view_size))
                            continue;
                        const auto& color = automap_view->pixels[
                            static_cast<std::size_t>(view_y) * tibia::map::automap_view_size +
                            static_cast<std::size_t>(view_x)];
                        bool player_marker = false;
                        if (automap_view->player_visible) {
                            const auto dx = view_x - automap_view->player_pixel_x;
                            const auto dy = view_y - automap_view->player_pixel_y;
                            if (automap_view->zoom < 3)
                                player_marker = (dx == 0 && std::abs(dy) <= 1) ||
                                    (dy == 0 && std::abs(dx) <= 1);
                            else
                                player_marker = (dy == 0 || dy == -1) && dx >= -3 && dx < 3;
                            if (automap_view->zoom >= 3)
                                player_marker = player_marker ||
                                    ((dx == 0 || dx == -1) && dy >= -3 && dy < 3);
                        }
                        const auto offset = (static_cast<std::size_t>(pixel_y) * tile_size +
                            static_cast<std::size_t>(pixel_x)) * 4U;
                        image.rgba[offset] = player_marker ? 255 : color.red;
                        image.rgba[offset + 1] = player_marker ? 255 : color.green;
                        image.rgba[offset + 2] = player_marker ? 255 : color.blue;
                        image.rgba[offset + 3] = 255;
                    }
                }
                const auto texture = texture_base +
                    static_cast<std::uint32_t>(tile_y * 4 + tile_x);
                host.renderer->upload(texture, image);
                const auto x = layout.viewport.x + tile_x * tile_size;
                const auto y = layout.viewport.y + tile_y * tile_size;
                host.renderer->draw_clipped(texture, x, y, tile_size, tile_size,
                    layout.viewport.x, layout.viewport.y,
                    layout.viewport.width, layout.viewport.height);
            }
        }
    }
    for (const auto& edge : tibia::client::layout_map_window_frame(layout.viewport).edges)
        draw_skin(host, edge.skin, edge.rect.x, edge.rect.y,
            edge.rect.width, edge.rect.height);
    for (const auto& edge : tibia::client::layout_map_window_frame(layout.panel).edges)
        draw_skin(host, edge.skin, edge.rect.x, edge.rect.y,
            edge.rect.width, edge.rect.height);
    draw_skin(host, 65, layout.compass.x, layout.compass.y,
        layout.compass.width, layout.compass.height);
    for (std::size_t index = 0; index < layout.controls.size(); ++index) {
        const auto& control = layout.controls[index];
        const auto skin = host.pressed_automap_control == static_cast<int>(index) &&
            (host.mouse_buttons & 1U) != 0
            ? control.pressed_skin : control.normal_skin;
        draw_skin(host, skin, control.bounds.x, control.bounds.y,
            control.bounds.width, control.bounds.height);
    }
    const auto centre_skin = host.pressed_automap_control == 4 &&
        (host.mouse_buttons & 1U) != 0
        ? layout.centre.pressed_skin : layout.centre.normal_skin;
    draw_skin(host, centre_skin, layout.centre.bounds.x,
        layout.centre.bounds.y, layout.centre.bounds.width,
        layout.centre.bounds.height);
    draw_bitmap_text(host, L"Centre", layout.centre.bounds.x,
        layout.centre.bounds.y + 6, layout.centre.bounds.width, true, 2);
}

void draw_player_status_bars(ClientHost& host,
    const tibia::client::GamePlayerStatistics& statistics,
    int sidebar_x, int status_window_y) {
    const auto layouts = tibia::client::layout_player_status_bars(
        sidebar_x, status_window_y);
    const std::array<std::uint32_t, 2> values{
        statistics.hit_points, statistics.mana};
    const std::array<std::uint32_t, 2> maxima{
        statistics.maximum_hit_points, statistics.maximum_mana};
    for (std::size_t index = 0; index < layouts.size(); ++index) {
        const auto& bar = layouts[index];
        draw_skin(host, bar.icon_skin, bar.icon_x, bar.icon_y, 12, 11);
        draw_skin(host, bar.track_skin, bar.bar_x, bar.bar_y,
            tibia::client::player_status_bar_width,
            tibia::client::player_status_bar_height);
        const int fill_width = tibia::client::player_status_bar_fill_width(
            values[index], maxima[index]);
        if (fill_width > 0) {
            const RECT clip{bar.bar_x, bar.bar_y, bar.bar_x + fill_width,
                bar.bar_y + tibia::client::player_status_bar_height};
            draw_skin_clipped(host, bar.fill_skin, bar.bar_x, bar.bar_y,
                tibia::client::player_status_bar_width,
                tibia::client::player_status_bar_height, clip);
        }
        const auto value = std::to_wstring(values[index]);
        draw_bitmap_text(host, value, bar.value_x, bar.value_y, 0, false,
            4, 176, false);
    }
}

void draw_game_screen_control_buttons(ClientHost& host, int sidebar_x, int row_y) {
    constexpr std::array<std::wstring_view, 4> labels{
        L"Skills", L"Battle", L"VIP", L"Logout"};
    const auto buttons = tibia::client::layout_game_screen_control_buttons(
        sidebar_x, row_y);
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        const auto& button = buttons[index];
        const bool pressed = host.pressed_game_screen_control ==
            static_cast<int>(index);
        draw_skin(host, pressed ? button.pressed_skin : button.normal_skin,
            button.bounds.x, button.bounds.y, button.bounds.width,
            button.bounds.height);
        draw_bitmap_text(host, labels[index], button.bounds.x, button.bounds.y + 6,
            button.bounds.width, true, 2);
    }
}

RECT combat_control_rect(int sidebar_x, bool compact, std::size_t index) {
    const auto& control = compact
        ? tibia::client::compact_combat_icons[index]
        : tibia::client::expanded_combat_icons[index];
    const int left = sidebar_x + (compact
        ? tibia::client::compact_combat_controls_x
        : tibia::client::expanded_combat_controls_x) + control.x;
    const int top = (compact
        ? tibia::client::compact_combat_controls_y
        : tibia::client::expanded_combat_controls_y) + control.y;
    return {left, top, left + tibia::client::combat_icon_size,
        top + tibia::client::combat_icon_size};
}

RECT combat_action_rect(int sidebar_x, bool compact, std::size_t index) {
    if (compact && index > 0) {
        const auto& control = tibia::client::compact_combat_utility_icons[index - 1];
        const int left = sidebar_x + tibia::client::compact_combat_controls_x + control.x;
        const int top = tibia::client::compact_combat_controls_y + control.y;
        return {left, top, left + tibia::client::combat_icon_size,
            top + tibia::client::combat_icon_size};
    }
    const int control_x = compact
        ? tibia::client::compact_combat_controls_x
        : tibia::client::expanded_combat_controls_x;
    const int control_y = compact
        ? tibia::client::compact_combat_controls_y
        : tibia::client::expanded_combat_controls_y;
    const auto& control = compact
        ? tibia::client::compact_combat_actions[0]
        : tibia::client::expanded_combat_actions[index];
    const int left = sidebar_x + control_x + control.x;
    const int top = control_y + control.y;
    return {left, top, left + 43, top + 20};
}

void draw_menu_frame(ClientHost& host, const RECT& rect) {
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const auto& tl = tibia::client::ui_skins[52];
    const auto& tr = tibia::client::ui_skins[53];
    const auto& bl = tibia::client::ui_skins[54];
    const auto& br = tibia::client::ui_skins[55];
    const int left = tl.width;
    const int right = tr.width;
    const int top = tl.height;
    const int bottom = bl.height;
    draw_skin_tiled(host, 60, rect.left + left, rect.top + top,
        width - left - right, height - top - bottom);
    draw_skin(host, 52, rect.left, rect.top, left, top);
    draw_skin_tiled(host, 58, rect.left + left, rect.top, width - left - right, top);
    draw_skin(host, 53, rect.right - right, rect.top, right, top);
    draw_skin_tiled(host, 56, rect.left, rect.top + top, left,
        height - top - bottom);
    draw_skin_tiled(host, 57, rect.right - right, rect.top + top, right,
        height - top - bottom);
    draw_skin(host, 54, rect.left, rect.bottom - bottom, left, bottom);
    draw_skin_tiled(host, 59, rect.left + left, rect.bottom - bottom,
        width - left - right, bottom);
    draw_skin(host, 55, rect.right - right, rect.bottom - bottom, right, bottom);
}

void draw_dialog_frame(ClientHost& host, const RECT& rect) {
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    // Use the verified nine-piece window frame from Tibia.pic. The previous
    // 177-185 mapping assigned 2x18 and 96x2 strips to the wrong edges and
    // tiled the menu's skin 60 in the body, leaving these dialogs without the
    // original opaque stone-textured background.
    constexpr int left = 4;
    constexpr int right = 4;
    constexpr int title = 17;
    constexpr int bottom = 4;
    draw_skin_tiled(host, 51, rect.left + left, rect.top + title,
        width - left - right, height - title - bottom);
    draw_skin_tiled(host, 47, rect.left, rect.top + title, left,
        height - title - bottom);
    draw_skin_tiled(host, 48, rect.right - right, rect.top + title, right,
        height - title - bottom);
    draw_skin_tiled(host, 49, rect.left + left, rect.top, width - left - right, title);
    draw_skin(host, 43, rect.left, rect.top, left, title);
    draw_skin(host, 44, rect.right - right, rect.top, right, title);
    draw_skin_tiled(host, 50, rect.left + left, rect.bottom - bottom,
        width - left - right, bottom);
    draw_skin(host, 45, rect.left, rect.bottom - bottom, left, bottom);
    draw_skin(host, 46, rect.right - right, rect.bottom - bottom, right, bottom);
}

void draw_original_dialog_frame(ClientHost& host, const RECT& rect) {
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    constexpr int left = 4;
    constexpr int right = 4;
    constexpr int title = 17;
    constexpr int bottom = 4;
    draw_skin_tiled(host, 51, rect.left + left, rect.top + title,
        width - left - right, height - title - bottom);
    draw_skin_tiled(host, 47, rect.left, rect.top + title, left,
        height - title - bottom);
    draw_skin_tiled(host, 48, rect.right - right, rect.top + title, right,
        height - title - bottom);
    draw_skin_tiled(host, 49, rect.left + left, rect.top, width - left - right, title);
    draw_skin(host, 43, rect.left, rect.top, left, title);
    draw_skin(host, 44, rect.right - right, rect.top, right, title);
    draw_skin_tiled(host, 50, rect.left + left, rect.bottom - bottom,
        width - left - right, bottom);
    draw_skin(host, 45, rect.left, rect.bottom - bottom, left, bottom);
    draw_skin(host, 46, rect.right - right, rect.bottom - bottom, right, bottom);
}

void draw_checkbox_row_background(ClientHost& host, const RECT& rect) {
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    draw_skin_tiled(host, 51, rect.left, rect.top, width, height);
    host.renderer->draw(ui_separator_shadow_texture, rect.left, rect.top, width, 1);
    host.renderer->draw(ui_separator_shadow_texture, rect.left, rect.top, 1, height);
    host.renderer->draw(ui_separator_highlight_texture, rect.right - 1, rect.top, 1, height);
    host.renderer->draw(ui_separator_highlight_texture, rect.left, rect.bottom - 1, width, 1);
}

void draw_skills_content_window(ClientHost& host,
    const tibia::client::SkillsPanelWindowPlan& window_plan) {
    const auto& window = window_plan.window;
    if (window.width <= 0 || window.height <
        tibia::client::content_window_frame_header_height) return;
    const auto& frame = window_plan.frame;
    for (const auto& part : frame.parts) {
        if (part.tiled)
            draw_skin_tiled(host, part.skin, part.rect.x, part.rect.y,
                part.rect.width, part.rect.height);
        else
            draw_skin(host, part.skin, part.rect.x, part.rect.y,
                part.rect.width, part.rect.height);
    }

    const auto header = tibia::client::layout_skills_panel_header(window);
    draw_bitmap_text(host, L"Skills", header.title.x, header.title.y,
        header.title.width, false, 3, 144, false);
    draw_skin(host, 142, header.icon.x, header.icon.y,
        header.icon.width, header.icon.height);
    draw_skin(host, host.pressed_skills_header_button == 1 ? 139 : 138,
        header.minimize_button.x, header.minimize_button.y,
        header.minimize_button.width, header.minimize_button.height);
    draw_skin(host, host.pressed_skills_header_button == 0 ? 137 : 136,
        header.close_button.x, header.close_button.y,
        header.close_button.width, header.close_button.height);
    if (frame.body.height <= 0) return;
    draw_skin_tiled(host, 7, frame.body.x, frame.body.y,
        frame.body.width, frame.body.height);

    const auto& plan = window_plan.skills;
    const auto& panel = plan.panel;

    const RECT panel_clip{panel.x, panel.y, panel.x + panel.width,
        panel.y + panel.height};
    for (std::size_t index = 0; index < plan.rows.size(); ++index) {
        const auto& row = plan.rows[index];
        const auto& layout = plan.row_layouts[index];
        if (row.kind == tibia::client::SkillsPanelRowKind::separator) {
            const auto color = static_cast<std::uint8_t>(
                layout.separator_color.red);
            if (color == 176)
                host.renderer->draw(skills_separator_texture,
                    panel.x + tibia::client::skills_panel_padding_x,
                    layout.separator_y,
                    std::max(0, panel.width - tibia::client::skills_panel_padding_x * 2 -
                        tibia::client::skills_panel_scrollbar_width), 1);
            continue;
        }

        const RECT row_clip{layout.clip.x, layout.clip.y,
            layout.clip.x + layout.clip.width,
            layout.clip.y + layout.clip.height};
        draw_bitmap_text_clipped(host, row.label, layout.label_x, layout.label_y,
            row_clip, tibia::client::skills_panel_font_index, layout.text_color.red);
        const auto value_width = bitmap_text_width(host, row.value,
            tibia::client::skills_panel_font_index, host.window);
        draw_bitmap_text_clipped(host, row.value,
            layout.value_right + 1 - value_width, layout.value_y,
            row_clip, tibia::client::skills_panel_font_index, layout.text_color.red);

        if (!row.progress) continue;
        host.renderer->draw_clipped(skills_bar_track_texture,
            layout.progress_track.x, layout.progress_track.y,
            layout.progress_track.width, layout.progress_track.height,
            panel_clip.left, panel_clip.top,
            panel_clip.right - panel_clip.left,
            panel_clip.bottom - panel_clip.top);
        if (layout.progress_fill.width <= 0 || layout.progress_fill.height <= 0)
            continue;
        const auto& color = layout.progress_color;
        const auto texture = color.red == 192 && color.green == 192
            ? skills_bar_yellow_texture
            : color.red == 192 && color.green == 96
                ? skills_bar_orange_texture
                : color.green == 192 ? skills_bar_green_texture
                                     : skills_bar_red_texture;
        host.renderer->draw_clipped(texture, layout.progress_fill.x,
            layout.progress_fill.y, layout.progress_fill.width,
            layout.progress_fill.height, panel_clip.left, panel_clip.top,
            panel_clip.right - panel_clip.left,
            panel_clip.bottom - panel_clip.top);
    }

    const auto& scrollbar = plan.scrollbar;
    draw_skin_tiled(host, 153, scrollbar.track.x, scrollbar.track.y,
        scrollbar.track.width, scrollbar.track.height);
    if (scrollbar.thumb_visible) {
        const auto cap = std::min(tibia::client::skills_panel_scroll_thumb_cap_height,
            scrollbar.thumb.height / 2);
        draw_skin(host, 154, scrollbar.thumb.x, scrollbar.thumb.y,
            scrollbar.thumb.width, cap);
        draw_skin_tiled(host, 155, scrollbar.thumb.x, scrollbar.thumb.y + cap,
            scrollbar.thumb.width, scrollbar.thumb.height - 2 * cap);
        draw_skin(host, 156, scrollbar.thumb.x,
            scrollbar.thumb.y + scrollbar.thumb.height - cap,
            scrollbar.thumb.width, cap);
    }
    draw_skin(host, host.pressed_skills_scrollbar_part == 0 ? 158 : 157,
        scrollbar.up_arrow.x, scrollbar.up_arrow.y,
        scrollbar.up_arrow.width, scrollbar.up_arrow.height);
    draw_skin(host, host.pressed_skills_scrollbar_part == 1 ? 160 : 159,
        scrollbar.down_arrow.x, scrollbar.down_arrow.y,
        scrollbar.down_arrow.width, scrollbar.down_arrow.height);
    if (window.height > tibia::client::content_window_frame_header_height)
        draw_skin(host, 42, window_plan.resize_grip.x,
            window_plan.resize_grip.y, window_plan.resize_grip.width,
            window_plan.resize_grip.height);
}

std::optional<tibia::client::SkillsPanelWindowPlan> skills_panel_window_plan(
    ClientHost& host, HWND window);
std::optional<tibia::client::BattlePanelWindowPlan> battle_panel_window_plan(
    ClientHost& host, HWND window);
std::optional<tibia::client::VipPanelWindowPlan> vip_panel_window_plan(
    ClientHost& host, HWND window);
int content_window_origin_y(int automap_y);

std::vector<tibia::client::ContainerPanelLayout> container_panel_layouts(
    ClientHost& host, HWND window,
    const tibia::client::GameWorldSnapshot& snapshot,
    const tibia::client::GameWindowLayout& game_layout) {
    host.container_panel_states.erase(std::remove_if(
        host.container_panel_states.begin(), host.container_panel_states.end(),
        [&snapshot](const auto& state) {
            return std::none_of(snapshot.containers.begin(), snapshot.containers.end(),
                [&state](const auto& container) { return container.id == state.id; });
        }), host.container_panel_states.end());
    for (const auto& container : snapshot.containers) {
        if (std::none_of(host.container_panel_states.begin(),
                host.container_panel_states.end(), [&container](const auto& state) {
                    return state.id == container.id;
                }))
            host.container_panel_states.push_back({container.id, 0, 0, 0, -1,
                ++host.next_container_z_order, false});
    }
    const int automap_y = host.inventory_minimized
        ? tibia::client::automap_panel_minimized_inventory_y
        : tibia::client::automap_panel_full_inventory_y;
    auto origin_y = content_window_origin_y(automap_y);
    if (const auto plan = skills_panel_window_plan(host, window))
        origin_y = std::max(origin_y, plan->window.y + plan->window.height);
    if (const auto plan = battle_panel_window_plan(host, window))
        origin_y = std::max(origin_y, plan->window.y + plan->window.height);
    if (const auto plan = vip_panel_window_plan(host, window))
        origin_y = std::max(origin_y, plan->window.y + plan->window.height);
    const tibia::client::ContentWindowRect sidebar{
        game_layout.sidebar.x, game_layout.sidebar.y,
        game_layout.sidebar.width, game_layout.sidebar.height};
    auto panels = tibia::client::layout_container_panels(sidebar, origin_y,
        snapshot.containers, host.container_panel_states);
    for (const auto& panel : panels) {
        const auto state = std::find_if(host.container_panel_states.begin(),
            host.container_panel_states.end(), [&panel](const auto& current) {
                return current.id == panel.id;
            });
        if (state != host.container_panel_states.end()) {
            if (state->position_y < 0) state->position_y = panel.window.y;
            state->scroll_value = panel.scroll_value;
        }
    }
    std::sort(panels.begin(), panels.end(), [&host](const auto& left, const auto& right) {
        const auto left_state = std::find_if(host.container_panel_states.begin(),
            host.container_panel_states.end(), [&left](const auto& state) {
                return state.id == left.id;
            });
        const auto right_state = std::find_if(host.container_panel_states.begin(),
            host.container_panel_states.end(), [&right](const auto& state) {
                return state.id == right.id;
            });
        if (left_state == host.container_panel_states.end() ||
            right_state == host.container_panel_states.end())
            return left.id < right.id;
        return left_state->z_order < right_state->z_order;
    });
    return panels;
}

void draw_container_panels(ClientHost& host,
    HWND window,
    const tibia::client::GameWorldSnapshot& snapshot,
    const tibia::client::GameWindowLayout& game_layout) {
    const auto panels = container_panel_layouts(host, window, snapshot, game_layout);
    for (const auto& panel : panels) {
        for (const auto& part : panel.frame.parts) {
            if (part.tiled)
                draw_skin_tiled(host, part.skin, part.rect.x, part.rect.y,
                    part.rect.width, part.rect.height);
            else
                draw_skin(host, part.skin, part.rect.x, part.rect.y,
                    part.rect.width, part.rect.height);
        }
        if (panel.frame.body.height > 0)
            draw_skin_tiled(host, 7, panel.frame.body.x, panel.frame.body.y,
                panel.frame.body.width, panel.frame.body.height);
        const auto container = std::find_if(snapshot.containers.begin(),
            snapshot.containers.end(), [&panel](const auto& current) {
                return current.id == panel.id;
            });
        if (container == snapshot.containers.end()) continue;
        for (const auto& cell : panel.item_cells)
            draw_skin_tiled(host, 102, cell.bounds.x, cell.bounds.y, 34, 34);
        if (panel.scrollable) {
            draw_skin_tiled(host, 153, panel.scroll_track.x, panel.scroll_track.y,
                panel.scroll_track.width, panel.scroll_track.height);
            const auto cap = std::min(6, panel.scroll_thumb.height / 2);
            draw_skin(host, 154, panel.scroll_thumb.x, panel.scroll_thumb.y,
                panel.scroll_thumb.width, cap);
            draw_skin_tiled(host, 155, panel.scroll_thumb.x,
                panel.scroll_thumb.y + cap, panel.scroll_thumb.width,
                panel.scroll_thumb.height - 2 * cap);
            draw_skin(host, 156, panel.scroll_thumb.x,
                panel.scroll_thumb.y + panel.scroll_thumb.height - cap,
                panel.scroll_thumb.width, cap);
        }
        if (!panel.minimized) {
            draw_skin(host, 157, panel.scroll_buttons[0].x,
                panel.scroll_buttons[0].y, panel.scroll_buttons[0].width,
                panel.scroll_buttons[0].height);
            draw_skin(host, 159, panel.scroll_buttons[1].x,
                panel.scroll_buttons[1].y, panel.scroll_buttons[1].width,
                panel.scroll_buttons[1].height);
        }
        draw_skin(host, 126, panel.higher_button.x, panel.higher_button.y,
            panel.higher_button.width, panel.higher_button.height);
        draw_skin(host, panel.minimized ? 139 : 138,
            panel.minimize_button.x, panel.minimize_button.y,
            panel.minimize_button.width, panel.minimize_button.height);
        draw_skin(host, 136, panel.close_button.x, panel.close_button.y,
            panel.close_button.width, panel.close_button.height);
        const auto header = tibia::client::layout_skills_panel_header(panel.window);
        draw_bitmap_text(host, decode_ui_string(container->name), header.title.x,
            header.title.y, header.title.width, false, 3, 144, false);
    }
    host.container_panel_renderer.draw(*host.renderer, snapshot, *host.dat,
        *host.tibia_spr_bytes, *host.tibia_spr, panels, host.animation_clock);
    for (const auto& panel : panels) {
        const auto container = std::find_if(snapshot.containers.begin(),
            snapshot.containers.end(), [&panel](const auto& current) {
                return current.id == panel.id;
            });
        if (container == snapshot.containers.end()) continue;
        for (const auto& cell : panel.item_cells) {
            const auto& item = container->items[cell.item_index];
            if (!item.has_extra) continue;
            const auto* object = host.dat->find_item(item.type);
            if (!object) continue;
            const auto quantity = tibia::client::inventory_item_quantity_label(
                *object, item.extra);
            if (quantity) {
                const auto label = std::to_wstring(*quantity);
                const auto width = bitmap_text_width(host, label, 1, host.window);
                const auto position = tibia::client::inventory_quantity_position(
                    cell.bounds.x, cell.bounds.y, width);
                draw_bitmap_text(host, label, position.x, position.y, 0, false, 1, 192);
            }
        }
    }
    for (const auto& panel : panels) {
        if (!panel.minimized)
            draw_skin(host, 42, panel.resize_grip.x, panel.resize_grip.y,
                panel.resize_grip.width, panel.resize_grip.height);
    }
}

int content_window_origin_y(int automap_y) {
    const auto controls = tibia::client::layout_game_screen_control_buttons(
        0, automap_y + tibia::client::automap_control_row_offset);
    return controls.front().bounds.y + controls.front().bounds.height;
}

std::optional<tibia::client::BattlePanelWindowPlan> battle_panel_window_plan(
    ClientHost& host, HWND window) {
    if (host.screen != UiScreen::game_world || !host.battle_window_open)
        return std::nullopt;
    const auto snapshot = host.game_world_mailbox->read();
    if (!snapshot) return std::nullopt;
    const auto rows = host.battle_panel_model.update(*snapshot);
    if (rows) host.battle_panel_rows = *rows;
    else host.battle_panel_rows.clear();

    RECT client{};
    if (!GetClientRect(window, &client)) return std::nullopt;
    auto restored_windows = host.restored_content_windows;
    for (auto& restored : restored_windows) {
        if (restored.type != tibia::client::ContentWindowType::battle) continue;
        if (host.battle_window_minimized)
            restored.saved_height = tibia::client::content_window_frame_header_height;
        else if (host.battle_window_resized_height > 0)
            restored.saved_height = host.battle_window_resized_height;
        else if (host.battle_window_restore_height > 0)
            restored.saved_height = host.battle_window_restore_height;
        else if (host.battle_window_natural_height > 0)
            restored.saved_height = host.battle_window_natural_height;
    }
    const auto automap_y = host.inventory_minimized
        ? tibia::client::automap_panel_minimized_inventory_y
        : tibia::client::automap_panel_full_inventory_y;
    auto plan = tibia::client::make_battle_panel_window_plan(
        client.right - client.left, client.bottom - client.top,
        content_window_origin_y(automap_y), restored_windows, host.battle_panel_rows,
        host.battle_scroll_value);
    if (plan && host.battle_window_natural_height == 0 &&
        !host.battle_window_minimized && host.battle_window_restore_height == 0) {
        const auto restored = std::find_if(host.restored_content_windows.begin(),
            host.restored_content_windows.end(), [](const auto& item) {
                return item.type == tibia::client::ContentWindowType::battle;
            });
        if (restored != host.restored_content_windows.end() &&
            restored->saved_height == 0)
            host.battle_window_natural_height = plan->window.height;
    }
    return plan;
}

std::uint32_t battle_panel_creature_at(
    ClientHost& host, HWND window, POINT point) {
    const auto plan = battle_panel_window_plan(host, window);
    if (!plan) return 0;
    return tibia::client::battle_panel_creature_at(
        plan->panel, host.battle_panel_rows, point.x, point.y);
}

std::optional<tibia::client::VipPanelWindowPlan> vip_panel_window_plan(
    ClientHost& host, HWND window) {
    if (host.screen != UiScreen::game_world || !host.vip_window_open)
        return std::nullopt;
    const auto snapshot = host.game_world_mailbox->read();
    if (!snapshot) return std::nullopt;
    RECT client{};
    if (!GetClientRect(window, &client)) return std::nullopt;
    auto restored_windows = host.restored_content_windows;
    for (auto& restored : restored_windows) {
        if (restored.type != tibia::client::ContentWindowType::vip) continue;
        if (host.vip_window_minimized)
            restored.saved_height = tibia::client::content_window_frame_header_height;
        else if (host.vip_window_resized_height > 0)
            restored.saved_height = host.vip_window_resized_height;
        else if (host.vip_window_restore_height > 0)
            restored.saved_height = host.vip_window_restore_height;
        else if (host.vip_window_natural_height > 0)
            restored.saved_height = host.vip_window_natural_height;
    }
    const auto automap_y = host.inventory_minimized
        ? tibia::client::automap_panel_minimized_inventory_y
        : tibia::client::automap_panel_full_inventory_y;
    auto plan = tibia::client::make_vip_panel_window_plan(
        client.right - client.left, client.bottom - client.top,
        content_window_origin_y(automap_y),
        restored_windows, snapshot->vip_contacts, host.vip_scroll_value);
    if (plan && host.vip_window_natural_height == 0 &&
        !host.vip_window_minimized && host.vip_window_restore_height == 0) {
        const auto restored = std::find_if(host.restored_content_windows.begin(),
            host.restored_content_windows.end(), [](const auto& item) {
                return item.type == tibia::client::ContentWindowType::vip;
            });
        if (restored != host.restored_content_windows.end() &&
            restored->saved_height == 0)
            host.vip_window_natural_height = plan->window.height;
    }
    return plan;
}

void remember_content_window_heights(ClientHost& host, HWND window) {
    for (const auto& type : tibia::client::restorable_content_window_types) {
        std::optional<int> height;
        switch (type) {
        case tibia::client::ContentWindowType::skills:
            if (const auto plan = skills_panel_window_plan(host, window))
                height = plan->window.height;
            break;
        case tibia::client::ContentWindowType::battle:
            if (const auto plan = battle_panel_window_plan(host, window))
                height = plan->window.height;
            break;
        case tibia::client::ContentWindowType::vip:
            if (const auto plan = vip_panel_window_plan(host, window))
                height = plan->window.height;
            break;
        case tibia::client::ContentWindowType::trade:
            break;
        }
        if (!height) continue;
        const auto state = std::find_if(host.restored_content_windows.begin(),
            host.restored_content_windows.end(), [type](const auto& item) {
                return item.type == type;
            });
        if (state != host.restored_content_windows.end() && state->saved_height == 0)
            state->saved_height = *height;
    }
}

void remember_content_window_height(ClientHost& host, HWND window,
    tibia::client::ContentWindowType type) {
    std::optional<int> height;
    switch (type) {
    case tibia::client::ContentWindowType::skills:
        if (const auto plan = skills_panel_window_plan(host, window))
            height = plan->window.height;
        break;
    case tibia::client::ContentWindowType::battle:
        if (const auto plan = battle_panel_window_plan(host, window))
            height = plan->window.height;
        break;
    case tibia::client::ContentWindowType::vip:
        if (const auto plan = vip_panel_window_plan(host, window))
            height = plan->window.height;
        break;
    case tibia::client::ContentWindowType::trade:
        break;
    }
    if (!height) return;
    const auto state = std::find_if(host.restored_content_windows.begin(),
        host.restored_content_windows.end(), [type](const auto& item) {
            return item.type == type;
        });
    if (state != host.restored_content_windows.end() && state->saved_height == 0)
        state->saved_height = *height;
}

std::uint32_t battle_health_color_texture(std::uint8_t health) {
    if (health < 4) return battle_health_color_texture_base;
    if (health < 10) return battle_health_color_texture_base + 1;
    if (health < 30) return battle_health_color_texture_base + 2;
    if (health < 60) return battle_health_color_texture_base + 3;
    if (health < 95) return battle_health_color_texture_base + 4;
    return battle_health_color_texture_base + 5;
}

std::uint32_t battle_target_frame_texture(
    const tibia::client::BattleTargetState& targets,
    std::uint32_t creature_id) {
    if (targets.attack_target == creature_id)
        return targets.hovered_creature == creature_id
            ? battle_attack_hover_frame_texture : battle_attack_frame_texture;
    if (targets.follow_target == creature_id)
        return targets.hovered_creature == creature_id
            ? battle_follow_hover_frame_texture : battle_follow_frame_texture;
    if (targets.hovered_creature == creature_id)
        return battle_hover_frame_texture;
    return 0;
}

void draw_battle_target_frame(ClientHost& host,
    const tibia::client::BattleCreatureRow& row,
    const tibia::client::BattlePanelRowLayout& layout) {
    const auto texture = battle_target_frame_texture(
        host.battle_targets, row.creature.creature_id);
    if (texture == 0) return;
    const auto x = layout.creature_cell.x - 1;
    const auto y = layout.creature_cell.y - 1;
    constexpr int extent = 20;
    const RECT clip{layout.clip.x, layout.clip.y,
        layout.clip.x + layout.clip.width, layout.clip.y + layout.clip.height};
    host.renderer->draw_clipped(texture, x, y, extent, 1,
        clip.left, clip.top, clip.right - clip.left, clip.bottom - clip.top);
    host.renderer->draw_clipped(texture, x, y + extent - 1, extent, 1,
        clip.left, clip.top, clip.right - clip.left, clip.bottom - clip.top);
    host.renderer->draw_clipped(texture, x, y + 1, 1, extent - 2,
        clip.left, clip.top, clip.right - clip.left, clip.bottom - clip.top);
    host.renderer->draw_clipped(texture, x + extent - 1, y + 1, 1, extent - 2,
        clip.left, clip.top, clip.right - clip.left, clip.bottom - clip.top);
}

void draw_battle_content_window(ClientHost& host,
    const tibia::client::BattlePanelWindowPlan& window_plan) {
    const auto& window = window_plan.window;
    if (window.width <= 0 || window.height <
        tibia::client::content_window_frame_header_height) return;
    for (const auto& part : window_plan.frame.parts) {
        if (part.tiled)
            draw_skin_tiled(host, part.skin, part.rect.x, part.rect.y,
                part.rect.width, part.rect.height);
        else
            draw_skin(host, part.skin, part.rect.x, part.rect.y,
                part.rect.width, part.rect.height);
    }

    const auto header = tibia::client::layout_skills_panel_header(window);
    draw_bitmap_text(host, L"Battle", header.title.x, header.title.y,
        header.title.width, false, 3, 144, false);
    draw_skin(host, 144, header.icon.x, header.icon.y,
        header.icon.width, header.icon.height);
    draw_skin(host, host.pressed_battle_header_button == 1 ? 139 : 138,
        header.minimize_button.x, header.minimize_button.y,
        header.minimize_button.width, header.minimize_button.height);
    draw_skin(host, host.pressed_battle_header_button == 0 ? 137 : 136,
        header.close_button.x, header.close_button.y,
        header.close_button.width, header.close_button.height);
    if (window.height > tibia::client::content_window_frame_header_height)
        draw_skin(host, 42, window.x + tibia::client::skills_panel_resize_grip_left_offset,
            window.y + window.height -
                tibia::client::skills_panel_resize_grip_bottom_offset,
            tibia::client::skills_panel_resize_grip_size,
            tibia::client::skills_panel_resize_grip_size);

    const auto& body = window_plan.frame.body;
    if (body.height <= 0) return;
    draw_skin_tiled(host, 7, body.x, body.y, body.width, body.height);
    const auto& viewport = window_plan.panel.viewport;
    const RECT panel_clip{viewport.x, viewport.y,
        viewport.x + viewport.width, viewport.y + viewport.height};
    const auto snapshot = host.game_world_mailbox->read();
    if (!snapshot || !snapshot->map || !host.dat || !host.tibia_spr ||
        !host.tibia_spr_bytes)
        return;
    for (const auto& row_layout : window_plan.panel.rows) {
        if (row_layout.index >= host.battle_panel_rows.size()) continue;
        const auto& row = host.battle_panel_rows[row_layout.index];
        const RECT row_clip{row_layout.clip.x, row_layout.clip.y,
            row_layout.clip.x + row_layout.clip.width,
            row_layout.clip.y + row_layout.clip.height};
        const auto icon = tibia::client::compose_battle_creature_icon(
            *host.tibia_spr_bytes, *host.tibia_spr, row.creature, *host.dat,
            snapshot->map->local_player_id,
            snapshot->local_player_movement,
            host.animation_clock,
            static_cast<std::uint32_t>(timeGetTime() - animation_epoch_tick));
        if (icon) {
            const auto texture = battle_creature_texture_base +
                static_cast<std::uint32_t>(row_layout.index);
            host.renderer->upload(texture, *icon);
            host.renderer->draw_clipped(texture, row_layout.creature_cell.x,
                row_layout.creature_cell.y, row_layout.creature_cell.width,
                row_layout.creature_cell.height, row_clip.left, row_clip.top,
                row_clip.right - row_clip.left, row_clip.bottom - row_clip.top);
        }
        draw_battle_target_frame(host, row, row_layout);

        const RECT name_clip{
            std::max(static_cast<int>(row_clip.left), row_layout.name.x),
            std::max(static_cast<int>(row_clip.top), row_layout.name.y),
            std::min(static_cast<int>(row_clip.right),
                row_layout.name.x + row_layout.name.width),
            std::min(static_cast<int>(row_clip.bottom),
                row_layout.name.y + row_layout.name.height)};
        draw_bitmap_text_clipped(host, error_to_wide(row.name.c_str()),
            row_layout.name.x, row_layout.name.y, name_clip, 3, 176);

        const auto draw_badge = [&](std::optional<std::uint16_t> skin,
            const tibia::client::BattlePanelRect& position) {
            if (!skin || *skin >= tibia::client::ui_skins.size()) return;
            const auto& image = tibia::client::ui_skins[*skin];
            draw_skin_clipped(host, *skin, position.x, position.y,
                image.width, image.height, row_clip);
        };
        draw_badge(row_layout.party_shield_skin, row_layout.party_shield);
        draw_badge(row_layout.skull_skin, row_layout.skull);

        host.renderer->draw_clipped(battle_health_track_texture,
            row_layout.health_track.x, row_layout.health_track.y,
            row_layout.health_track.width, row_layout.health_track.height,
            panel_clip.left, panel_clip.top,
            panel_clip.right - panel_clip.left,
            panel_clip.bottom - panel_clip.top);
        if (row_layout.health_fill.width > 0) {
            host.renderer->draw_clipped(
                battle_health_color_texture(row.health_percent),
                row_layout.health_fill.x, row_layout.health_fill.y,
                row_layout.health_fill.width, row_layout.health_fill.height,
                panel_clip.left, panel_clip.top,
                panel_clip.right - panel_clip.left,
                panel_clip.bottom - panel_clip.top);
        }
    }

    const auto& scrollbar = window_plan.scrollbar;
    if (scrollbar.thumb_visible) {
        draw_skin_tiled(host, 153, scrollbar.track.x, scrollbar.track.y,
            scrollbar.track.width, scrollbar.track.height);
        const auto cap = std::min(6, scrollbar.thumb.height / 2);
        draw_skin(host, 154, scrollbar.thumb.x, scrollbar.thumb.y,
            scrollbar.thumb.width, cap);
        draw_skin_tiled(host, 155, scrollbar.thumb.x,
            scrollbar.thumb.y + cap, scrollbar.thumb.width,
            scrollbar.thumb.height - 2 * cap);
        draw_skin(host, 156, scrollbar.thumb.x,
            scrollbar.thumb.y + scrollbar.thumb.height - cap,
            scrollbar.thumb.width, cap);
    }
    draw_skin(host, host.pressed_battle_scrollbar_part == 0 ? 158 : 157,
        scrollbar.up_arrow.x, scrollbar.up_arrow.y,
        scrollbar.up_arrow.width, scrollbar.up_arrow.height);
    draw_skin(host, host.pressed_battle_scrollbar_part == 1 ? 160 : 159,
        scrollbar.down_arrow.x, scrollbar.down_arrow.y,
        scrollbar.down_arrow.width, scrollbar.down_arrow.height);
}

void draw_vip_content_window(ClientHost& host,
    const tibia::client::VipPanelWindowPlan& window_plan,
    const std::vector<tibia::network::State6BuddyEntry>& contacts) {
    const auto& window = window_plan.window;
    for (const auto& part : window_plan.frame.parts) {
        if (part.tiled)
            draw_skin_tiled(host, part.skin, part.rect.x, part.rect.y,
                part.rect.width, part.rect.height);
        else
            draw_skin(host, part.skin, part.rect.x, part.rect.y,
                part.rect.width, part.rect.height);
    }

    const auto header = tibia::client::layout_skills_panel_header(window);
    draw_bitmap_text(host, L"VIP", header.title.x, header.title.y,
        header.title.width, false, 3, 144, false);
    draw_skin(host, 143, header.icon.x, header.icon.y,
        header.icon.width, header.icon.height);
    draw_skin(host, host.pressed_vip_header_button == 1 ? 139 : 138,
        header.minimize_button.x, header.minimize_button.y,
        header.minimize_button.width, header.minimize_button.height);
    draw_skin(host, host.pressed_vip_header_button == 0 ? 137 : 136,
        header.close_button.x, header.close_button.y,
        header.close_button.width, header.close_button.height);
    if (window.height > tibia::client::content_window_frame_header_height)
        draw_skin(host, 42, window.x + tibia::client::skills_panel_resize_grip_left_offset,
            window.y + window.height -
                tibia::client::skills_panel_resize_grip_bottom_offset,
            tibia::client::skills_panel_resize_grip_size,
            tibia::client::skills_panel_resize_grip_size);

    const auto& body = window_plan.frame.body;
    if (body.height > 0)
        draw_skin_tiled(host, 7, body.x, body.y, body.width, body.height);
    for (const auto& row : window_plan.rows) {
        if (row.index >= contacts.size() || row.clip.height <= 0) continue;
        const auto name = error_to_wide(contacts[row.index].name.c_str());
        const RECT clip{row.clip.x, row.clip.y,
            row.clip.x + row.clip.width, row.clip.y + row.clip.height};
        const auto color = contacts[row.index].online
            ? 0x0060f860U : 0x00f86060U;
        constexpr std::size_t font_index = 3;
        const float scale = host.window ? ui_scale(host.window) : 1.0F;
        const auto scaled = [scale](int value) {
            return std::max(1, static_cast<int>(std::lround(
                static_cast<float>(value) * scale)));
        };
        int x = row.bounds.x;
        const int y = row.bounds.y;
        const int text_right = row.bounds.x + row.bounds.width;
        for (const wchar_t character : name) {
            if (character == L' ') {
                x += scaled(host.ui_fonts[font_index][12].advance) +
                    static_cast<int>(std::lround(
                        host.ui_font_spacing[font_index] * scale));
                continue;
            }
            const wchar_t code = character < 32 || character > 255
                ? L'?' : character;
            const auto glyph_index = static_cast<std::size_t>(code - 32);
            const auto& glyph = host.ui_fonts[font_index][glyph_index];
            const int glyph_width = scaled(glyph.width);
            const int glyph_height = scaled(glyph.height);
            if (x + glyph_width > text_right) break;
            if (glyph.width > 0 && glyph.height > 0) {
                host.renderer->draw_clipped(
                    colored_ui_font_glyph_texture(host, font_index, color,
                        glyph_index), x, y, glyph_width, glyph_height,
                    clip.left, clip.top,
                    clip.right - clip.left, clip.bottom - clip.top);
            }
            x += scaled(glyph.advance) + static_cast<int>(std::lround(
                host.ui_font_spacing[font_index] * scale));
        }
    }

    const auto& scrollbar = window_plan.scrollbar;
    if (scrollbar.thumb_visible) {
        draw_skin_tiled(host, 153, scrollbar.track.x, scrollbar.track.y,
            scrollbar.track.width, scrollbar.track.height);
        const auto cap = std::min(tibia::client::skills_panel_scroll_thumb_cap_height,
            scrollbar.thumb.height / 2);
        draw_skin(host, 154, scrollbar.thumb.x, scrollbar.thumb.y,
            scrollbar.thumb.width, cap);
        draw_skin_tiled(host, 155, scrollbar.thumb.x,
            scrollbar.thumb.y + cap, scrollbar.thumb.width,
            scrollbar.thumb.height - 2 * cap);
        draw_skin(host, 156, scrollbar.thumb.x,
            scrollbar.thumb.y + scrollbar.thumb.height - cap,
            scrollbar.thumb.width, cap);
    }
    draw_skin(host, host.pressed_vip_scrollbar_part == 0 ? 158 : 157,
        scrollbar.up_arrow.x, scrollbar.up_arrow.y,
        scrollbar.up_arrow.width, scrollbar.up_arrow.height);
    draw_skin(host, host.pressed_vip_scrollbar_part == 1 ? 160 : 159,
        scrollbar.down_arrow.x, scrollbar.down_arrow.y,
        scrollbar.down_arrow.width, scrollbar.down_arrow.height);
}

void draw_context_menu_frame(ClientHost& host,
    const tibia::client::ContentWindowRect& rect) {
    constexpr int edge = tibia::client::console_context_menu_border;
    draw_skin_tiled(host, 211, rect.x + edge, rect.y + edge,
        rect.width - edge * 2, rect.height - edge * 2);
    draw_skin(host, 212, rect.x, rect.y, edge, edge);
    draw_skin_tiled(host, 218, rect.x + edge, rect.y,
        rect.width - edge * 2, edge);
    draw_skin(host, 213, rect.x + rect.width - edge, rect.y, edge, edge);
    draw_skin_tiled(host, 216, rect.x, rect.y + edge, edge,
        rect.height - edge * 2);
    draw_skin_tiled(host, 217, rect.x + rect.width - edge,
        rect.y + edge, edge, rect.height - edge * 2);
    draw_skin(host, 214, rect.x, rect.y + rect.height - edge, edge, edge);
    draw_skin_tiled(host, 219, rect.x + edge, rect.y + rect.height - edge,
        rect.width - edge * 2, edge);
    draw_skin(host, 215, rect.x + rect.width - edge,
        rect.y + rect.height - edge, edge, edge);
}

void draw_context_menu_separator(ClientHost& host,
    const tibia::client::ContentWindowRect& rect) {
    const int line_y = rect.y + (rect.height - 2) / 2;
    draw_skin_tiled(host, 9, rect.x, line_y, rect.width, 1);
    draw_skin_tiled(host, 8, rect.x, line_y + 1, rect.width, 1);
}

void draw_skills_context_menu(ClientHost& host) {
    if (!host.skills_context_menu) return;
    const auto& menu = *host.skills_context_menu;
    const auto& rect = menu.window;
    if (rect.width <= 0 || rect.height <= 0) return;
    draw_context_menu_frame(host, rect);
    for (const auto& entry : menu.entries) {
        if (entry.separator) {
            draw_context_menu_separator(host, entry.rect);
            continue;
        }
        if (contains_point(RECT{entry.rect.x, entry.rect.y,
                entry.rect.x + entry.rect.width,
                entry.rect.y + entry.rect.height}, host.mouse))
            host.renderer->draw(skills_menu_selection_texture, entry.rect.x,
                entry.rect.y, entry.rect.width, entry.rect.height);
        draw_bitmap_text(host, entry.label, entry.rect.x + 2,
            entry.rect.y + 2, entry.rect.width - 4, false, 1, 224, false);
    }
}

std::optional<tibia::client::SkillsPanelWindowPlan> skills_panel_window_plan(
    ClientHost& host, HWND window) {
    if (host.screen != UiScreen::game_world || !host.skills_window_open)
        return std::nullopt;
    const auto snapshot = host.game_world_mailbox->read();
    if (!snapshot) return std::nullopt;
    RECT client{};
    if (!GetClientRect(window, &client)) return std::nullopt;
    auto restored_windows = host.restored_content_windows;
    for (auto& restored : restored_windows) {
        if (restored.type != tibia::client::ContentWindowType::skills) continue;
        if (host.skills_window_minimized)
            restored.saved_height = 19;
        else if (host.skills_window_resized_height > 0)
            restored.saved_height = host.skills_window_resized_height;
        else if (host.skills_window_restore_height > 0)
            restored.saved_height = host.skills_window_restore_height;
    }
    const auto automap_y = host.inventory_minimized
        ? tibia::client::automap_panel_minimized_inventory_y
        : tibia::client::automap_panel_full_inventory_y;
    return tibia::client::make_skills_panel_window_plan(
        client.right - client.left, client.bottom - client.top,
        content_window_origin_y(automap_y), restored_windows,
        snapshot->player_statistics, host.skills_bar_visibility,
        std::max(1, host.ui_font_heights[
            tibia::client::skills_panel_font_index]), host.skills_scroll_value);
}

RECT skills_rect(const tibia::client::ContentWindowRect& rect) {
    return {rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
}

void open_skills_context_menu(ClientHost& host, HWND window, POINT anchor) {
    RECT client{};
    if (!GetClientRect(window, &client)) return;
    const auto labels = tibia::client::skills_context_menu_labels(
        host.skills_bar_visibility);
    std::array<int, 12> widths{};
    for (std::size_t index = 0; index < labels.size(); ++index)
        widths[index] = bitmap_text_width(host, labels[index], 1, window);
    host.skills_context_menu = tibia::client::make_skills_context_menu_plan(
        host.skills_bar_visibility, widths,
        std::max(1, host.ui_font_heights[1]), anchor.x, anchor.y,
        client.right - client.left, client.bottom - client.top);
    host.pressed_skills_context_menu_action = -1;
    InvalidateRect(window, nullptr, FALSE);
}

void open_vip_context_menu(ClientHost& host, HWND window,
    const tibia::client::VipPanelWindowPlan& panel, POINT anchor) {
    const auto snapshot = host.game_world_mailbox->read();
    if (!snapshot) return;
    const tibia::network::State6BuddyEntry* contact = nullptr;
    for (const auto& row : panel.rows) {
        if (row.index < snapshot->vip_contacts.size() &&
            contains_point(skills_rect(row.clip), anchor)) {
            contact = &snapshot->vip_contacts[row.index];
            break;
        }
    }

    std::vector<tibia::client::VipContextMenuOption> options;
    if (contact) {
        const auto name = error_to_wide(contact->name.c_str());
        options.push_back({tibia::client::VipContextAction::edit,
            L"Edit " + name, false});
        options.push_back({tibia::client::VipContextAction::remove,
            L"Remove " + name, false});
        const bool is_local_player = snapshot->map &&
            snapshot->map->local_player_id == contact->id;
        if (!is_local_player && contact->online)
            options.push_back({tibia::client::VipContextAction::message,
                L"Message to " + name, false});
        if (host.communication_session_mailbox &&
            host.communication_session_mailbox->private_channel_filter_available()) {
            const auto channel = host.communication_session_mailbox->channel_view();
            if (channel.selected_identifier && *channel.selected_identifier >= 9)
                options.push_back({tibia::client::VipContextAction::invite_private,
                    L"Invite to private chat", false});
        }
        options.push_back({{}, {}, true});
        host.vip_context_player_name = name;
    } else {
        host.vip_context_player_name.clear();
    }
    options.push_back({tibia::client::VipContextAction::add_new,
        L"Add new VIP", false});
    options.push_back({tibia::client::VipContextAction::sort_name,
        L"Sort by name", false});
    options.push_back({tibia::client::VipContextAction::sort_type,
        L"Sort by type", false});
    options.push_back({tibia::client::VipContextAction::sort_status,
        L"Sort by status", false});
    options.push_back({tibia::client::VipContextAction::toggle_offline,
        L"Hide offline VIPs", false});
    if (contact) {
        options.push_back({{}, {}, true});
        options.push_back({tibia::client::VipContextAction::copy_name,
            L"Copy Name", false});
        if (snapshot->server_right_values_received) {
            options.push_back({{}, {}, true});
            options.push_back({tibia::client::VipContextAction::rule_violation,
                L"Rule Violation", false});
        }
    }
    std::vector<int> widths;
    widths.reserve(options.size());
    for (const auto& option : options)
        if (!option.separator)
            widths.push_back(bitmap_text_width(host, option.label, 1, window));
    RECT client{};
    if (!GetClientRect(window, &client)) return;
    host.vip_context_menu = tibia::client::make_vip_context_menu_plan(
        std::move(options), widths, std::max(1, host.ui_font_heights[1]),
        anchor.x, anchor.y, client.right - client.left,
        client.bottom - client.top);
    host.pressed_vip_context_menu_action = -1;
    InvalidateRect(window, nullptr, FALSE);
}

void apply_skills_context_menu_action(ClientHost& host, std::uint16_t action) {
    auto visibility = host.skills_bar_visibility;
    if (!tibia::client::apply_skills_context_menu_action(visibility, action)) return;
    host.skills_bar_visibility = visibility;
    try {
        auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        preferences.skills_bar_visibility = visibility;
        tibia::client::write_communication_preferences(
            host.communication_config_path, preferences);
    } catch (const std::exception& error) {
        host.notice_text = error_to_wide(error.what());
    }
}

void save_skills_window_height(ClientHost& host, int height) {
    try {
        auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        tibia::client::save_content_window_height(preferences.content_windows,
            tibia::client::ContentWindowType::skills, height);
        tibia::client::write_communication_preferences(
            host.communication_config_path, preferences);
        host.restored_content_windows =
            tibia::client::restore_content_windows(preferences.content_windows);
    } catch (const std::exception& error) {
        host.notice_text = error_to_wide(error.what());
    }
}

void save_battle_window_height(ClientHost& host, int height) {
    try {
        auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        tibia::client::save_content_window_height(preferences.content_windows,
            tibia::client::ContentWindowType::battle, height);
        tibia::client::write_communication_preferences(
            host.communication_config_path, preferences);
        host.restored_content_windows =
            tibia::client::restore_content_windows(preferences.content_windows);
    } catch (const std::exception& error) {
        host.notice_text = error_to_wide(error.what());
    }
}

void save_vip_window_height(ClientHost& host, int height) {
    try {
        auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        tibia::client::save_content_window_height(preferences.content_windows,
            tibia::client::ContentWindowType::vip, height);
        tibia::client::write_communication_preferences(
            host.communication_config_path, preferences);
        host.restored_content_windows =
            tibia::client::restore_content_windows(preferences.content_windows);
    } catch (const std::exception& error) {
        host.notice_text = error_to_wide(error.what());
    }
}

void save_console_height(ClientHost& host) {
    try {
        auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        preferences.console_height = host.console_height;
        tibia::client::write_communication_preferences(
            host.communication_config_path, preferences);
    } catch (const std::exception& error) {
        host.notice_text = error_to_wide(error.what());
    }
}

const tibia::client::SkillsContextMenuEntry* skills_menu_entry_at(
    const ClientHost& host, POINT point) {
    if (!host.skills_context_menu) return nullptr;
    for (const auto& entry : host.skills_context_menu->entries) {
        if (!entry.separator && contains_point(skills_rect(entry.rect), point))
            return &entry;
    }
    return nullptr;
}

const tibia::client::BattleContextMenuEntry* battle_menu_entry_at(
    const ClientHost& host, POINT point) {
    if (!host.battle_context_menu) return nullptr;
    for (const auto& entry : host.battle_context_menu->entries) {
        if (!entry.separator && contains_point(skills_rect(entry.rect), point))
            return &entry;
    }
    return nullptr;
}

void draw_battle_context_menu(ClientHost& host) {
    if (!host.battle_context_menu) return;
    const auto& menu = *host.battle_context_menu;
    const auto& rect = menu.window;
    if (rect.width <= 0 || rect.height <= 0) return;
    draw_context_menu_frame(host, rect);
    for (const auto& entry : menu.entries) {
        if (entry.separator) {
            draw_context_menu_separator(host, entry.rect);
            continue;
        }
        if (contains_point(skills_rect(entry.rect), host.mouse))
            host.renderer->draw(skills_menu_selection_texture, entry.rect.x,
                entry.rect.y, entry.rect.width, entry.rect.height);
        draw_bitmap_text(host, entry.label, entry.rect.x + 2,
            entry.rect.y + 2, entry.rect.width - 4, false, 1, 224, false);
    }
}

const tibia::client::ConsoleContextMenuItem* console_context_menu_item_at(
    const ClientHost& host, POINT point) {
    if (!host.console_context_menu) return nullptr;
    for (const auto& item : host.console_context_menu->items) {
        if (contains_point(RECT{item.rect.x, item.rect.y,
                item.rect.x + item.rect.width,
                item.rect.y + item.rect.height}, point))
            return &item;
    }
    return nullptr;
}

const tibia::client::VipContextMenuEntry* vip_context_menu_entry_at(
    const ClientHost& host, POINT point) {
    if (!host.vip_context_menu) return nullptr;
    for (const auto& entry : host.vip_context_menu->entries) {
        if (!entry.separator && contains_point(skills_rect(entry.rect), point))
            return &entry;
    }
    return nullptr;
}

void apply_vip_context_menu_action(ClientHost& host, HWND window,
    tibia::client::VipContextAction action) {
    if (action == tibia::client::VipContextAction::message ||
        action == tibia::client::VipContextAction::invite_private) {
        if (!host.vip_context_player_name.empty() && host.send_game_payload) {
            tibia::network::PacketWriter writer;
            writer.write_u8(action == tibia::client::VipContextAction::message
                ? 0x9a : 0xab);
            writer.write_string(encode_ui_string(host.vip_context_player_name));
            const auto payload = std::move(writer).take();
            try {
                host.send_game_payload(payload);
            } catch (const std::exception& error) {
                host.notice_text = error_to_wide(error.what());
            }
        }
    } else if (action == tibia::client::VipContextAction::copy_name &&
        !host.vip_context_player_name.empty()) {
        (void)copy_text_to_clipboard(window, host.vip_context_player_name);
    }
    host.vip_context_menu.reset();
    host.pressed_vip_context_menu_action = -1;
    InvalidateRect(window, nullptr, FALSE);
}

void draw_console_context_menu(ClientHost& host) {
    if (!host.console_context_menu) return;
    const auto& menu = *host.console_context_menu;
    if (menu.window.width <= 0 || menu.window.height <= 0) return;
    draw_context_menu_frame(host, menu.window);
    for (const auto& item : menu.items) {
        const RECT rect{item.rect.x, item.rect.y,
            item.rect.x + item.rect.width,
            item.rect.y + item.rect.height};
        if (contains_point(rect, host.mouse))
            host.renderer->draw(skills_menu_selection_texture, item.rect.x,
                item.rect.y, item.rect.width, item.rect.height);
        draw_bitmap_text(host, item.label, item.rect.x + 2,
            item.rect.y + 2, item.rect.width - 4, false, 1, 224, false);
    }
}

void draw_vip_context_menu(ClientHost& host) {
    if (!host.vip_context_menu) return;
    const auto& menu = *host.vip_context_menu;
    if (menu.window.width <= 0 || menu.window.height <= 0) return;
    draw_context_menu_frame(host, menu.window);
    for (const auto& entry : menu.entries) {
        if (entry.separator) {
            draw_context_menu_separator(host, entry.rect);
            continue;
        }
        if (contains_point(skills_rect(entry.rect), host.mouse))
            host.renderer->draw(skills_menu_selection_texture, entry.rect.x,
                entry.rect.y, entry.rect.width, entry.rect.height);
        draw_bitmap_text(host, entry.label, entry.rect.x + 2,
            entry.rect.y + 2, entry.rect.width - 4, false, 1, 224, false);
    }
}

void open_console_context_menu(ClientHost& host, HWND window, POINT anchor,
    int row_index) {
    if (row_index < 0 || row_index >= static_cast<int>(
            host.console_transcript_rows.size()))
        return;
    const auto& row = host.console_transcript_rows[
        static_cast<std::size_t>(row_index)];
    std::vector<std::pair<tibia::client::ConsoleContextMenuAction,
        std::wstring>> items;
    std::wstring local_player_name;
    if (host.selected_character >= 0 &&
        static_cast<std::size_t>(host.selected_character) < host.characters.size())
        local_player_name = host.characters[
            static_cast<std::size_t>(host.selected_character)].name;
    const bool report_buffer_active = host.report_violation_tab_selected;
    const bool speaker_is_player = !local_player_name.empty() &&
        !row.speaker_name.empty() &&
        row.speaker_name != local_player_name;
    if (!report_buffer_active && speaker_is_player) {
        items.emplace_back(
            tibia::client::ConsoleContextMenuAction::message_to,
            L"Message to " + row.speaker_name);
        if (host.communication_session_mailbox &&
            host.communication_session_mailbox->private_channel_filter_available()) {
            const auto channel = host.communication_session_mailbox->channel_view();
            const bool active_private_channel = channel.selected_identifier &&
                *channel.selected_identifier >= 9;
            items.emplace_back(active_private_channel
                    ? tibia::client::ConsoleContextMenuAction::exclude_from_private_chat
                    : tibia::client::ConsoleContextMenuAction::invite_to_private_chat,
                active_private_channel ? L"Exclude from private chat"
                                       : L"Invite to private chat");
        }
        const auto snapshot = host.game_world_mailbox->read();
        const auto encoded_speaker_name = encode_ui_string(row.speaker_name);
        const auto same_name = [&encoded_speaker_name](std::string_view name) {
            if (name.size() != encoded_speaker_name.size()) return false;
            return std::equal(name.begin(), name.end(), encoded_speaker_name.begin(),
                [](unsigned char left, unsigned char right) {
                    return std::tolower(left) == std::tolower(right);
                });
        };
        if (snapshot && std::none_of(snapshot->vip_contacts.begin(),
                snapshot->vip_contacts.end(), [&same_name](const auto& contact) {
                    return same_name(contact.name);
                }))
            items.emplace_back(
                tibia::client::ConsoleContextMenuAction::add_to_vip,
                L"Add to VIP list");
        const auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        const bool ignored = std::any_of(preferences.ignore_list.begin(),
            preferences.ignore_list.end(), [&same_name](const auto& name) {
                return same_name(name);
            });
        items.emplace_back(
            tibia::client::ConsoleContextMenuAction::toggle_ignore,
            std::wstring(ignored ? L"Unignore " : L"Ignore ") +
                row.speaker_name);
    }
    if (host.console_transcript_selection_active)
        items.emplace_back(
            tibia::client::ConsoleContextMenuAction::copy_selection, L"Copy");
    if (!row.speaker_name.empty())
        items.emplace_back(
            tibia::client::ConsoleContextMenuAction::copy_name, L"Copy Name");
    items.emplace_back(
        tibia::client::ConsoleContextMenuAction::copy_message, L"Copy Message");
    items.emplace_back(
        tibia::client::ConsoleContextMenuAction::select_all, L"Select all");
    const auto snapshot = host.game_world_mailbox->read();
    if (!report_buffer_active && !row.speaker_name.empty() && snapshot &&
        snapshot->server_right_values_received &&
        std::any_of(snapshot->server_right_values.begin(),
            snapshot->server_right_values.end(), [](std::uint8_t value) {
                return value != 0;
            }))
        items.emplace_back(
            tibia::client::ConsoleContextMenuAction::rule_violation,
            L"Rule Violation");
    std::vector<int> widths;
    widths.reserve(items.size());
    for (const auto& item : items)
        widths.push_back(bitmap_text_width(host, item.second, 1, window));
    RECT client{};
    if (!GetClientRect(window, &client)) return;
    host.console_context_message = row.message;
    host.console_context_speaker_name = row.speaker_name;
    host.console_context_statement_id = row.report_statement_id;
    host.console_context_statement = row.statement_text;
    host.console_context_menu = tibia::client::make_console_context_menu_plan(
        std::move(items), widths, std::max(1, host.ui_font_heights[1]),
        anchor.x, anchor.y, client.right - client.left,
        client.bottom - client.top);
    host.pressed_console_context_menu_action = -1;
    InvalidateRect(window, nullptr, FALSE);
}

void apply_console_context_menu_action(ClientHost& host, HWND window,
    tibia::client::ConsoleContextMenuAction action) {
    switch (action) {
    case tibia::client::ConsoleContextMenuAction::message_to:
    case tibia::client::ConsoleContextMenuAction::invite_to_private_chat:
    case tibia::client::ConsoleContextMenuAction::exclude_from_private_chat:
    case tibia::client::ConsoleContextMenuAction::add_to_vip: {
        if (host.console_context_speaker_name.empty() || !host.send_game_payload)
            break;
        std::uint8_t opcode = 0;
        switch (action) {
        case tibia::client::ConsoleContextMenuAction::message_to:
            opcode = 0x9a;
            break;
        case tibia::client::ConsoleContextMenuAction::invite_to_private_chat:
            opcode = 0xab;
            break;
        case tibia::client::ConsoleContextMenuAction::exclude_from_private_chat:
            opcode = 0xac;
            break;
        case tibia::client::ConsoleContextMenuAction::add_to_vip:
            opcode = 0xdc;
            break;
        default:
            break;
        }
        tibia::network::PacketWriter writer;
        writer.write_u8(opcode);
        writer.write_string(encode_ui_string(host.console_context_speaker_name));
        const auto payload = std::move(writer).take();
        try {
            host.send_game_payload(payload);
        } catch (const std::exception& error) {
            host.notice_text = error_to_wide(error.what());
        }
        break;
    }
    case tibia::client::ConsoleContextMenuAction::toggle_ignore: {
        if (host.console_context_speaker_name.empty()) break;
        try {
            const auto target_name = encode_ui_string(
                host.console_context_speaker_name);
            auto preferences = tibia::client::read_communication_preferences(
                host.communication_config_path);
            const auto same_name = [&target_name](const auto& name) {
                if (name.size() != target_name.size()) return false;
                return std::equal(name.begin(), name.end(), target_name.begin(),
                    [](unsigned char left, unsigned char right) {
                        return std::tolower(left) == std::tolower(right);
                    });
            };
            const auto ignored = std::find_if(preferences.ignore_list.begin(),
                preferences.ignore_list.end(), same_name);
            std::string message;
            if (ignored == preferences.ignore_list.end()) {
                preferences.ignore_list.push_back(target_name);
                message = "You are now ignoring player " +
                    target_name + ".";
            } else {
                preferences.ignore_list.erase(ignored);
                message = "You are no longer ignoring player " +
                    target_name + ".";
            }
            tibia::client::write_communication_preferences(
                host.communication_config_path, preferences);
            host.communication_session_mailbox->publish_ignore_list(
                preferences.ignore_list);
            host.communication_session_mailbox->enqueue_local_message(0x15,
                std::move(message));
        } catch (const std::exception& error) {
            host.notice_text = error_to_wide(error.what());
        }
        break;
    }
    case tibia::client::ConsoleContextMenuAction::copy_selection:
        (void)copy_console_transcript_selection(host, window);
        break;
    case tibia::client::ConsoleContextMenuAction::copy_name:
        (void)copy_text_to_clipboard(window,
            host.console_context_speaker_name);
        break;
    case tibia::client::ConsoleContextMenuAction::copy_message:
        (void)copy_text_to_clipboard(window, host.console_context_message);
        break;
    case tibia::client::ConsoleContextMenuAction::select_all:
        select_all_console_transcript(host);
        break;
    case tibia::client::ConsoleContextMenuAction::rule_violation: {
        const auto snapshot = host.game_world_mailbox->read();
        if (!host.console_context_speaker_name.empty() && snapshot &&
            snapshot->server_right_values_received) {
            host.rule_violation_request_form.show_for_message(
                snapshot->server_right_values,
                host.console_context_speaker_name,
                host.console_context_statement,
                host.console_context_statement_id);
            host.rule_violation_request_reason_scroll = 0;
            host.rule_violation_request_action_scroll = 0;
            host.rule_violation_request_focus_field = -1;
            host.rule_violation_request_dialog_offset = {};
            host.pressed_rule_violation_request_button = -1;
        }
        break;
    }
    }
    host.console_context_menu.reset();
    host.pressed_console_context_menu_action = -1;
    InvalidateRect(window, nullptr, FALSE);
}

void open_battle_context_menu(ClientHost& host, HWND window,
    std::uint32_t creature_id, POINT anchor) {
    if (creature_id == 0) return;
    RECT client{};
    if (!GetClientRect(window, &client)) return;
    const auto creature = std::find_if(host.battle_panel_rows.begin(),
        host.battle_panel_rows.end(), [creature_id](const auto& row) {
            return row.creature.creature_id == creature_id;
        });
    std::vector<tibia::client::BattleContextMenuOption> player_options;
    if (creature_id >= 0x40000000U && creature != host.battle_panel_rows.end()) {
        const auto preferences = tibia::client::read_communication_preferences(
            host.communication_config_path);
        const auto same_name = [](std::string_view left, std::string_view right) {
            if (left.size() != right.size()) return false;
            return std::equal(left.begin(), left.end(), right.begin(),
                [](unsigned char lhs, unsigned char rhs) {
                    return std::tolower(lhs) == std::tolower(rhs);
                });
        };
        player_options.push_back({tibia::client::BattleTargetAction::message_to,
            L"Message to " + error_to_wide(creature->name.c_str())});
        const auto snapshot = host.game_world_mailbox->read();
        const auto is_vip = snapshot && std::any_of(snapshot->vip_contacts.begin(),
            snapshot->vip_contacts.end(), [&creature, &same_name](const auto& contact) {
                return same_name(contact.name, creature->name);
            });
        if (snapshot && !is_vip)
            player_options.push_back({tibia::client::BattleTargetAction::add_vip,
                L"Add to VIP list"});
        const auto is_ignored = std::any_of(preferences.ignore_list.begin(),
            preferences.ignore_list.end(), [&creature, &same_name](const auto& name) {
                return same_name(name, creature->name);
            });
        const auto player_name = error_to_wide(creature->name.c_str());
        player_options.push_back({tibia::client::BattleTargetAction::toggle_ignore,
            std::wstring(is_ignored ? L"Unignore " : L"Ignore ") + player_name});
    }
    const auto attack_label = host.battle_targets.attack_target == creature_id
        ? std::wstring_view(L"Stop Attack") : std::wstring_view(L"Attack");
    const auto follow_label = host.battle_targets.follow_target == creature_id
        ? std::wstring_view(L"Stop Follow") : std::wstring_view(L"Follow");
    std::vector<int> widths{
        bitmap_text_width(host, attack_label, 1, window),
        bitmap_text_width(host, follow_label, 1, window),
        bitmap_text_width(host, L"Copy Name", 1, window)};
    for (const auto& option : player_options)
        widths.push_back(bitmap_text_width(host, option.label, 1, window));
    host.battle_context_menu = tibia::client::make_battle_context_menu_plan(
        host.battle_targets, creature_id, std::move(player_options), widths,
        std::max(1, host.ui_font_heights[1]), anchor.x, anchor.y,
        client.right - client.left, client.bottom - client.top);
    host.pressed_battle_context_menu_action = -1;
    InvalidateRect(window, nullptr, FALSE);
}

void copy_battle_creature_name(ClientHost& host, std::uint32_t creature_id) {
    const auto creature = std::find_if(host.battle_panel_rows.begin(),
        host.battle_panel_rows.end(), [creature_id](const auto& row) {
            return row.creature.creature_id == creature_id;
        });
    if (creature == host.battle_panel_rows.end() || !OpenClipboard(nullptr)) return;
    if (EmptyClipboard() == 0) {
        CloseClipboard();
        return;
    }
    const auto allocation_size = creature->name.size() + 1U;
    const auto memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, allocation_size);
    if (!memory) {
        CloseClipboard();
        return;
    }
    auto* text = static_cast<char*>(GlobalLock(memory));
    if (!text) {
        GlobalFree(memory);
        CloseClipboard();
        return;
    }
    std::copy(creature->name.begin(), creature->name.end(), text);
    text[creature->name.size()] = '\0';
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_TEXT, memory)) GlobalFree(memory);
    CloseClipboard();
}

void apply_battle_target_command(ClientHost& host,
    tibia::client::BattleTargetAction action, std::uint32_t creature_id) {
    if (action == tibia::client::BattleTargetAction::copy_name) {
        copy_battle_creature_name(host, creature_id);
        return;
    }
    if (action == tibia::client::BattleTargetAction::message_to ||
        action == tibia::client::BattleTargetAction::add_vip) {
        const auto creature = std::find_if(host.battle_panel_rows.begin(),
            host.battle_panel_rows.end(), [creature_id](const auto& row) {
                return row.creature.creature_id == creature_id;
            });
        if (creature == host.battle_panel_rows.end() || !host.send_game_payload)
            return;
        tibia::network::PacketWriter writer;
        writer.write_u8(action == tibia::client::BattleTargetAction::message_to
            ? 0x9a : 0xdc);
        writer.write_string(creature->name);
        const auto payload = std::move(writer).take();
        try {
            host.send_game_payload(payload);
        } catch (const std::exception& error) {
            host.notice_text = error_to_wide(error.what());
        }
        return;
    }
    if (action == tibia::client::BattleTargetAction::toggle_ignore) {
        const auto creature = std::find_if(host.battle_panel_rows.begin(),
            host.battle_panel_rows.end(), [creature_id](const auto& row) {
                return row.creature.creature_id == creature_id;
            });
        if (creature == host.battle_panel_rows.end()) return;
        try {
            auto preferences = tibia::client::read_communication_preferences(
                host.communication_config_path);
            const auto same_name = [&creature](const auto& name) {
                if (name.size() != creature->name.size()) return false;
                return std::equal(name.begin(), name.end(), creature->name.begin(),
                    [](unsigned char left, unsigned char right) {
                        return std::tolower(left) == std::tolower(right);
                    });
            };
            const auto ignored = std::find_if(preferences.ignore_list.begin(),
                preferences.ignore_list.end(), same_name);
            std::string confirmation;
            if (ignored == preferences.ignore_list.end()) {
                preferences.ignore_list.push_back(creature->name);
                confirmation = "You are now ignoring player " + creature->name + ".";
            } else {
                preferences.ignore_list.erase(ignored);
                confirmation = "You are no longer ignoring player " + creature->name + ".";
            }
            tibia::client::write_communication_preferences(
                host.communication_config_path, preferences);
            host.communication_session_mailbox->publish_ignore_list(preferences.ignore_list);
            host.communication_session_mailbox->enqueue_local_message(0x15,
                std::move(confirmation));
        } catch (const std::exception& error) {
            host.notice_text = error_to_wide(error.what());
        }
        return;
    }
    if (!tibia::client::apply_battle_target_action(
            host.battle_targets, action, creature_id))
        return;
    if (!host.send_game_payload) return;
    const auto payload = tibia::client::battle_target_payload(action, creature_id);
    try {
        host.send_game_payload(payload);
    } catch (const std::exception& error) {
        host.notice_text = error_to_wide(error.what());
    }
}

std::wstring read_control_text(HWND control) {
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring text(static_cast<std::size_t>(length) + 1U, L'\0');
    const int copied = GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(std::max(0, copied)));
    return text;
}

void draw_login_screen(ClientHost& host) {
    const auto dialog = login_dialog_rect(host.window, host.login_dialog_offset);
    const auto panel = login_rect(host.window, host.login_dialog_offset);
    const int x = panel.left;
    const int y = panel.top;
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Enter Game", dialog.left + ui_units(host.window, 5),
        dialog.top + ui_units(host.window, 5), ui_units(host.window, 226), true, 3, 144);

    draw_bitmap_text(host, L"Account number:", x, y + ui_units(host.window, 2),
        ui_units(host.window, 107), false, 3, 192);
    draw_bitmap_text(host, L"Password:", x, y + ui_units(host.window, 31),
        ui_units(host.window, 107), false, 3, 192);

    const auto draw_field = [&](const RECT& field, std::wstring_view value, bool focused) {
        const auto scale = ui_scale(host.window);
        const int inset = std::max(1, static_cast<int>(std::lround(3.0F * scale)));
        const int field_width = field.right - field.left;
        const int field_height = field.bottom - field.top;
        // The edit fill is a solid #363636 rectangle. The original's 1px bevel
        // comes from picture 4: skin 9 (top), 8 (bottom), 11 (left), 10 (right).
        host.renderer->draw(login_field_background_texture, field.left, field.top,
            field_width, field_height);
        // Draw the source texels at native UI coordinates; draw_skin would
        // compress the 96px strips to this 86px control and shift their shades.
        draw_login_field_bevel(host, field);
        std::wstring display(value);
        const int text_x = field.left + inset;
        const int text_y = field.top + inset;
        draw_bitmap_text(host, display, text_x, text_y,
            std::max(0, field_width - inset * 2), false, 3, 224);
        const int phase = static_cast<int>((GetTickCount64() / 500U) & 1U);
        if (focused && phase == 0) {
            const int caret_x = std::min(static_cast<int>(field.right) - inset,
                text_x + bitmap_text_width(host, value, 3, host.window));
            const int caret_height = std::max(1, field_height - inset * 2);
            host.renderer->draw(login_caret_texture, caret_x, text_y,
                std::max(1, static_cast<int>(std::lround(scale))), caret_height);
        }
    };
    const std::wstring hidden_account(host.login_account_text.size(), L'*');
    draw_field(login_account_field_rect(host.window, host.login_dialog_offset), hidden_account,
        host.login_focus_field == 0);
    const std::wstring hidden_password(host.login_password_text.size(), L'*');
    draw_field(login_password_field_rect(host.window, host.login_dialog_offset), hidden_password,
        host.login_focus_field == 1);

    // The original hyperlink gives the prompt a 114x28 region, so it wraps to
    // two lines beside the vertically centered 86x20 Create Account button.
    draw_wrapped_bitmap_text(host, L"If you don't have an account yet:", x,
        y + ui_units(host.window, 60), ui_units(host.window, 114),
        ui_units(host.window, 28), 14, 3, 192);
    const int link_x = x + ui_units(host.window, 114);
    const int link_y = y + ui_units(host.window, 58);
    const int link_width = ui_units(host.window, 86);
    const int link_height = ui_units(host.window, 20);
    draw_skin(host, host.pressed_login_button == 2 ? 19 : 18,
        link_x, link_y, link_width, link_height);
    draw_bitmap_text(host, L"Create Account", link_x,
        link_y + ui_units(host.window, 6), link_width, true, 2, 255);

    const auto draw_dialog_button = [&](const RECT& button, std::wstring_view label, int id) {
        draw_skin(host, host.pressed_login_button == id ? 13 : 12, button.left, button.top,
            button.right - button.left, button.bottom - button.top);
        draw_bitmap_text(host, label, button.left, button.top + ui_units(host.window, 6),
            button.right - button.left, true, 2, 255);
    };
    draw_dialog_button(login_ok_rect(host.window, host.login_dialog_offset), L"Ok", 0);
    draw_dialog_button(login_cancel_rect(host.window, host.login_dialog_offset), L"Cancel", 1);

}

RECT character_selection_dialog_rect(HWND window, POINT offset = {}) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = ui_units(window, 254);
    const int height = ui_units(window, 312);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int left = std::max(0, (client_width - width) / 2);
    const int top = std::max(0, (client_height - height) / 2);
    return clamp_dialog_rect(window, {left, top, left + width, top + height}, offset);
}

RECT character_panel_frame_rect(HWND window, POINT offset = {}) {
    const auto dialog = character_selection_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 18), dialog.top + ui_units(window, 32),
        dialog.left + ui_units(window, 236), dialog.top + ui_units(window, 252)};
}

RECT character_content_rect(HWND window, POINT offset = {}) {
    const auto frame = character_panel_frame_rect(window, offset);
    return {frame.left + ui_units(window, 9), frame.top + ui_units(window, 9),
        frame.right - ui_units(window, 9), frame.bottom - ui_units(window, 9)};
}

RECT character_list_rect(HWND window, POINT offset = {}) {
    const auto content = character_content_rect(window, offset);
    return {content.left, content.top + ui_units(window, 15), content.right,
        content.top + ui_units(window, 161)};
}

RECT character_ok_rect(HWND window, POINT offset = {}) {
    const auto dialog = character_selection_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 137), dialog.top + ui_units(window, 282),
        dialog.left + ui_units(window, 180), dialog.top + ui_units(window, 302)};
}

RECT character_cancel_rect(HWND window, POINT offset = {}) {
    const auto dialog = character_selection_dialog_rect(window, offset);
    return {dialog.left + ui_units(window, 190), dialog.top + ui_units(window, 282),
        dialog.left + ui_units(window, 233), dialog.top + ui_units(window, 302)};
}

void draw_character_screen(ClientHost& host) {
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    const auto dialog = character_selection_dialog_rect(host.window,
        host.character_selection_dialog_offset);
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Select Character", dialog.left + s(5), dialog.top + s(5),
        s(244), true, 3, 144);

    const auto frame = character_panel_frame_rect(host.window,
        host.character_selection_dialog_offset);
    const auto content = character_content_rect(host.window,
        host.character_selection_dialog_offset);
    draw_skin_tiled(host, 7, frame.left + s(1), frame.top + s(1),
        frame.right - frame.left - s(2), frame.bottom - frame.top - s(2));
    draw_skin_tiled(host, 9, frame.left, frame.top, frame.right - frame.left, s(1));
    draw_skin_tiled(host, 8, frame.left, frame.bottom - s(1), frame.right - frame.left, s(1));
    draw_skin_tiled(host, 11, frame.left, frame.top + s(1), s(1),
        frame.bottom - frame.top - s(2));
    draw_skin_tiled(host, 10, frame.right - s(1), frame.top + s(1), s(1),
        frame.bottom - frame.top - s(2));
    draw_bitmap_text(host, L"Select character:", content.left, content.top + s(1),
        content.right - content.left, false, 3, 192);

    const auto list = character_list_rect(host.window, host.character_selection_dialog_offset);
    const int margin = s(3);
    const int row_height = s(std::max(1, host.ui_font_heights[3]));
    const int visible_rows = std::max(1,
        (static_cast<int>(list.bottom - list.top) - 2 * margin) / row_height);
    const auto scrollbar = options_list_scrollbar_geometry(host.window, list,
        host.characters.size(), visible_rows, host.character_list_scroll);
    const int scrollbar_width = scrollbar.up_arrow.right - scrollbar.up_arrow.left;
    const int text_width = std::max(0,
        static_cast<int>(list.right - list.left) - 2 * margin - scrollbar_width);
    host.renderer->draw(graphics_list_background_texture, list.left, list.top,
        list.right - list.left, list.bottom - list.top);
    draw_skin_tiled(host, 9, list.left, list.top, list.right - list.left, margin);
    draw_skin_tiled(host, 8, list.left, list.bottom - margin, list.right - list.left, margin);
    draw_skin_tiled(host, 11, list.left, list.top + margin, margin,
        list.bottom - list.top - 2 * margin);
    draw_skin_tiled(host, 10, list.right - margin, list.top + margin, margin,
        list.bottom - list.top - 2 * margin);
    RECT text_clip{list.left + margin, list.top + margin,
        list.left + margin + text_width, list.bottom - margin};
    for (int visible = 0; visible < visible_rows; ++visible) {
        const int index = host.character_list_scroll + visible;
        if (index < 0 || static_cast<std::size_t>(index) >= host.characters.size()) break;
        const int row_y = list.top + margin + visible * row_height;
        const int row_bottom = std::min(row_y + row_height,
            static_cast<int>(list.bottom) - margin);
        if (index == host.selected_character)
            host.renderer->draw(graphics_list_selection_texture,
                list.left + margin - s(1), row_y - s(1), text_width + s(2),
                row_bottom - row_y + s(2));
        const auto& character = host.characters[static_cast<std::size_t>(index)];
        const auto label = character.name + L" (" + character.world + L")";
        draw_bitmap_text_clipped(host, label, list.left + margin, row_y, text_clip, 3,
            index == host.selected_character ? 255 : 176);
    }
    draw_ui_scrollbar_parts(host, list, scrollbar,
        host.pressed_character_scroll_button == 0,
        host.pressed_character_scroll_button == 1);

    const int status_y = content.top + s(174);
    draw_bitmap_text(host, L"Account Status:", content.left, status_y,
        content.right - content.left, false, 3, 176);
    if (host.account_status == 0) {
        draw_bitmap_text(host, L"Free Account", content.left, status_y + s(16),
            content.right - content.left, false, 3, 176);
    } else if (host.account_status == 0xffff) {
        draw_bitmap_text(host, L"Gratis Premium Account", content.left, status_y + s(16),
            content.right - content.left, false, 3, 176);
    } else {
        const auto days = std::to_wstring(host.account_status);
        const std::wstring_view prefix = L"Premium Account (";
        const std::wstring_view suffix = host.account_status == 1 ? L" day left)" : L" days left)";
        draw_bitmap_text(host, prefix, content.left, status_y + s(16), 0, false, 3, 176);
        const int days_x = content.left + bitmap_text_width(host, prefix, 3, host.window);
        draw_bitmap_text(host, days, days_x, status_y + s(16), 0, false, 3,
            host.account_status < 14 ? 96 : 176);
        const int suffix_x = days_x + bitmap_text_width(host, days, 3, host.window);
        draw_bitmap_text(host, suffix, suffix_x, status_y + s(16), 0, false, 3, 176);
    }

    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12),
        dialog.top + s(272), s(230), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12),
        dialog.top + s(273), s(230), s(1));
    const auto draw_button = [&](const RECT& button, std::wstring_view label, int id) {
        draw_skin(host, host.pressed_login_button == id ? 13 : 12,
            button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, label, button.left, button.top + s(6),
            button.right - button.left, true, 2, 255);
    };
    draw_button(character_ok_rect(host.window, host.character_selection_dialog_offset), L"Ok", 0);
    draw_button(character_cancel_rect(host.window, host.character_selection_dialog_offset), L"Cancel", 1);
}

RECT notice_rect(const ClientHost& host, POINT offset = {}) {
    return message_dialog_rect(host, host.notice_text, offset);
}

RECT new_channel_native_rect(const tibia::client::ConsolePanelRect& rect) {
    return {rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
}

tibia::client::NewChannelDialogLayout new_channel_layout(
    const ClientHost& host, POINT offset = {}) {
    RECT client{};
    GetClientRect(host.window, &client);
    return tibia::client::resolve_new_channel_dialog_layout(
        client.right - client.left, client.bottom - client.top,
        ui_units(host.window, 1), offset.x, offset.y);
}

RECT new_channel_dialog_rect(const ClientHost& host, POINT offset = {}) {
    return new_channel_native_rect(new_channel_layout(host, offset).dialog);
}

RECT new_channel_list_rect(const ClientHost& host) {
    return new_channel_native_rect(
        new_channel_layout(host, host.new_channel_dialog_offset).channel_list);
}

RECT new_channel_private_field_rect(const ClientHost& host) {
    return new_channel_native_rect(
        new_channel_layout(host, host.new_channel_dialog_offset).private_field);
}

RECT new_channel_button_rect(const ClientHost& host, int index) {
    const auto layout = new_channel_layout(host, host.new_channel_dialog_offset);
    return new_channel_native_rect(index == 0 ? layout.ok_button : layout.cancel_button);
}

void draw_new_channel_dialog(ClientHost& host) {
    const auto layout = new_channel_layout(host, host.new_channel_dialog_offset);
    const auto dialog = new_channel_native_rect(layout.dialog);
    const int unit = ui_units(host.window, 1);
    draw_original_dialog_frame(host, dialog);
    const auto title = new_channel_native_rect(layout.title);
    draw_bitmap_text(host, L"New Channel", title.left, title.top,
        title.right - title.left,
        true, 3, 144);

    const auto channel_label = new_channel_native_rect(layout.channel_label);
    draw_bitmap_text(host, L"Select a channel:", channel_label.left,
        channel_label.top, channel_label.right - channel_label.left,
        false, 3, 192);

    const auto list = new_channel_list_rect(host);
    const int list_width = static_cast<int>(list.right - list.left);
    const int list_height = static_cast<int>(list.bottom - list.top);
    const int margin = 3 * unit;
    const int row_height = ui_units(host.window,
        std::max(1, host.ui_font_heights[3]));
    const int visible_rows = std::max(1,
        (list_height - 2 * margin) / row_height);
    const auto scrollbar = options_list_scrollbar_geometry(host.window, list,
        host.available_channels.size(), visible_rows, host.new_channel_scroll);
    const int scrollbar_width = scrollbar.up_arrow.right - scrollbar.up_arrow.left;
    const int text_width = std::max(0, list_width - 2 * margin - scrollbar_width);
    host.renderer->draw(graphics_list_background_texture, list.left, list.top,
        list_width, list_height);
    draw_skin_tiled(host, 9, list.left, list.top, list_width, margin);
    draw_skin_tiled(host, 8, list.left, list.bottom - margin,
        list_width, margin);
    draw_skin_tiled(host, 11, list.left, list.top + margin, margin,
        list_height - 2 * margin);
    draw_skin_tiled(host, 10, list.right - margin, list.top + margin, margin,
        list_height - 2 * margin);
    const RECT clip{list.left + margin, list.top + margin,
        list.left + margin + text_width, list.bottom - margin};
    for (int row = 0; row < visible_rows; ++row) {
        const int index = host.new_channel_scroll + row;
        if (index < 0 || static_cast<std::size_t>(index) >= host.available_channels.size())
            break;
        const int y = list.top + margin + row * row_height;
        const int bottom = std::min(y + row_height,
            static_cast<int>(list.bottom) - margin);
        if (index == host.new_channel_selected)
            host.renderer->draw(graphics_list_selection_texture, list.left + margin - unit,
                y - unit, text_width + 2 * unit, bottom - y + 2 * unit);
        const auto name = decode_ui_string(
            host.available_channels[static_cast<std::size_t>(index)].name);
        draw_bitmap_text_clipped(host, name, list.left + margin, y, clip, 3,
            index == host.new_channel_selected ? 255 : 176);
    }
    draw_ui_scrollbar_parts(host, list, scrollbar,
        host.pressed_new_channel_scroll_button == 0,
        host.pressed_new_channel_scroll_button == 1);

    const auto private_label = new_channel_native_rect(layout.private_label);
    draw_bitmap_text(host, L"Open a private message channel:", private_label.left,
        private_label.top, private_label.right - private_label.left,
        false, 3, 192);
    const auto field = new_channel_private_field_rect(host);
    host.renderer->draw(host.private_channel_focused
        ? console_input_focused_texture : login_field_background_texture,
        field.left, field.top, field.right - field.left, field.bottom - field.top);
    const int text_x = field.left + 3 * unit;
    const int text_y = field.top + 2 * unit;
    draw_bitmap_text(host, host.private_channel_name, text_x, text_y,
        field.right - field.left - 6 * unit, false, 3, 255);
    if (host.private_channel_focused && ((GetTickCount64() / 500U) & 1U) == 0) {
        const int caret_x = std::min(static_cast<int>(field.right) - 2 * unit,
            text_x + bitmap_text_width(host, host.private_channel_name, 3, host.window));
        host.renderer->draw(login_caret_texture, caret_x, field.top + 3 * unit,
            unit, field.bottom - field.top - 6 * unit);
    }

    const auto separator = new_channel_native_rect(layout.separator);
    host.renderer->draw(ui_separator_shadow_texture, separator.left, separator.top,
        separator.right - separator.left, unit);
    host.renderer->draw(ui_separator_highlight_texture, separator.left,
        separator.top + unit, separator.right - separator.left, unit);
    for (int index = 0; index < 2; ++index) {
        const auto button = new_channel_button_rect(host, index);
        draw_skin(host, host.pressed_new_channel_button == index ? 13 : 12,
            button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, index == 0 ? L"Ok" : L"Cancel",
            button.left, button.top + 6 * unit, button.right - button.left,
            true, 2, 255);
    }
}

void draw_rule_violation_request_dialog(ClientHost& host) {
    const auto dialog = rule_violation_request_dialog_rect(host.window,
        host.rule_violation_request_dialog_offset);
    const auto s = [window = host.window](int value) {
        return ui_units(window, value);
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Rule Violation", dialog.left + s(5),
        dialog.top + s(5), dialog.right - dialog.left - s(10), true, 3, 144);

    const auto content = rule_violation_request_content_rect(host.window,
        host.rule_violation_request_dialog_offset);
    constexpr std::array<std::wstring_view, 5> labels{
        L"Name:", L"Statement:", L"Reason:", L"Action:", L"Comment:"};
    constexpr std::array<int, 5> label_y{0, 59, 92, 199, 315};
    for (std::size_t index = 0; index < labels.size(); ++index)
        draw_bitmap_text(host, labels[index], content.left,
            content.top + s(label_y[index]), content.right - content.left,
            false, 3, 176);

    const std::array<std::wstring_view, 3> fields{
        host.rule_violation_request_form.player_name,
        host.rule_violation_request_form.statement,
        host.rule_violation_request_form.comment};
    constexpr std::array<int, 3> field_ids{0, 1, 3};
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto rect = rule_violation_request_edit_rect(host.window,
            host.rule_violation_request_dialog_offset, field_ids[index]);
        const bool enabled = field_ids[index] != 1 ||
            host.rule_violation_request_form.statement_enabled;
        host.renderer->draw(enabled && host.rule_violation_request_focus_field ==
                field_ids[index]
                ? console_input_focused_texture : login_field_background_texture,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
        draw_skin_tiled(host, 9, rect.left, rect.top,
            rect.right - rect.left, s(1));
        draw_skin_tiled(host, 8, rect.left, rect.bottom - s(1),
            rect.right - rect.left, s(1));
        draw_skin_tiled(host, 11, rect.left, rect.top + s(1), s(1),
            rect.bottom - rect.top - 2 * s(1));
        draw_skin_tiled(host, 10, rect.right - s(1), rect.top + s(1), s(1),
            rect.bottom - rect.top - 2 * s(1));
        const auto text_x = rect.left + s(3);
        const auto text_y = rect.top + s(1);
        const auto tint = enabled ? 224 : 128;
        draw_bitmap_text(host, fields[index], text_x, text_y,
            rect.right - rect.left - s(6), false, 3, tint);
        if (enabled && host.rule_violation_request_focus_field == field_ids[index] &&
            ((GetTickCount64() / 500U) & 1U) == 0) {
            const int caret_x = std::min(rect.right - s(2), text_x +
                bitmap_text_width(host, fields[index], 3, host.window));
            host.renderer->draw(login_caret_texture, caret_x, rect.top + s(3),
                s(1), rect.bottom - rect.top - s(6));
        }
    }

    const auto draw_list = [&host, &s](bool actions) {
        const auto& form = host.rule_violation_request_form;
        const auto list = rule_violation_request_list_rect(host.window,
            host.rule_violation_request_dialog_offset, actions);
        const auto& entries = actions ? form.available_actions :
            form.available_reasons;
        const int first = actions ? host.rule_violation_request_action_scroll :
            host.rule_violation_request_reason_scroll;
        const auto target = actions
            ? ClientHost::ListScrollbarTarget::rule_violation_actions
            : ClientHost::ListScrollbarTarget::rule_violation_reasons;
        const auto visible = options_list_visible_items(host, target);
        const int margin = s(3);
        const int row_height = s(std::max(1, host.ui_font_line_advances[3]));
        const auto scrollbar = options_list_scrollbar_geometry(host.window,
            list, entries.size(), visible, first);
        const int text_width = std::max(0, static_cast<int>(list.right -
            list.left - 2 * margin -
            (scrollbar.up_arrow.right - scrollbar.up_arrow.left)));
        host.renderer->draw(graphics_list_background_texture, list.left,
            list.top, list.right - list.left, list.bottom - list.top);
        draw_skin_tiled(host, 9, list.left, list.top,
            list.right - list.left, margin);
        draw_skin_tiled(host, 8, list.left, list.bottom - margin,
            list.right - list.left, margin);
        draw_skin_tiled(host, 11, list.left, list.top + margin, margin,
            list.bottom - list.top - 2 * margin);
        draw_skin_tiled(host, 10, list.right - margin, list.top + margin,
            margin, list.bottom - list.top - 2 * margin);
        const RECT clip{list.left + margin, list.top + margin,
            list.left + margin + text_width, list.bottom - margin};
        const int selected = actions ? form.selected_action : form.selected_reason;
        for (int row = 0; row < visible; ++row) {
            const int index = first + row;
            if (index < 0 || static_cast<std::size_t>(index) >= entries.size())
                break;
            const int y = list.top + margin + row * row_height;
            const auto id = entries[static_cast<std::size_t>(index)];
            const auto label = actions
                ? tibia::client::rule_violation_action_label(id)
                : tibia::client::rule_violation_reason_label(id);
            if (id == selected)
                host.renderer->draw(graphics_list_selection_texture,
                    list.left + margin - s(1), y - s(1),
                    text_width + 2 * s(1), row_height + 2 * s(1));
            draw_bitmap_text_clipped(host, label, list.left + margin, y,
                clip, 3, id == selected ? 255 : 176);
        }
        draw_options_list_scrollbar(host, list, target, entries.size(),
            visible, first);
    };
    draw_list(false);
    draw_list(true);

    const auto checkbox = rule_violation_request_checkbox_rect(host.window,
        host.rule_violation_request_dialog_offset);
    draw_checkbox_row_background(host, checkbox);
    draw_skin(host, host.rule_violation_request_form.ip_banishment ? 150 : 151,
        checkbox.left + s(6), checkbox.top + s(5), s(12), s(12));
    draw_bitmap_text(host, L"IP Address Banishment",
        checkbox.left + s(24), centered_bitmap_text_y(host, checkbox.top + s(5),
            s(12), 3), checkbox.right - checkbox.left - s(30), false, 3,
        host.rule_violation_request_form.ip_banishment_enabled ? 176 : 128);

    const int footer_y = dialog.bottom - s(40);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12),
        footer_y, dialog.right - dialog.left - s(24), s(1));
    host.renderer->draw(ui_separator_highlight_texture,
        dialog.left + s(12), footer_y + s(1),
        dialog.right - dialog.left - s(24), s(1));
    for (int index = 0; index < 2; ++index) {
        const auto button = rule_violation_request_button_rect(host.window,
            host.rule_violation_request_dialog_offset, index);
        draw_skin(host, host.pressed_rule_violation_request_button == index
                ? 13 : 12,
            button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, index == 0 ? L"Ok" : L"Cancel",
            button.left, button.top + s(6), button.right - button.left,
            true, 2, 255);
    }
}

void submit_rule_violation_request(ClientHost& host) {
    auto& form = host.rule_violation_request_form;
    const auto validation = form.validate(form.statement_reportable);
    if (validation != tibia::client::RuleViolationValidationError::none) {
        show_notice(host, L"Rule Violation",
            tibia::client::rule_violation_validation_message(validation));
        return;
    }
    if (!host.send_game_payload) return;
    try {
        const auto payload = tibia::network::encode_state6_rule_violation_request(
            encode_ui_string(form.player_name),
            static_cast<std::uint8_t>(form.selected_reason),
            static_cast<std::uint8_t>(form.selected_action),
            encode_ui_string(form.comment),
            form.statement_reportable ? form.statement_id : 0,
            form.ip_banishment ? 1 : 0);
        host.send_game_payload(payload);
        form.close();
        KillTimer(host.window, login_caret_timer_id);
        host.rule_violation_request_focus_field = -1;
        host.pressed_rule_violation_request_button = -1;
        host.rule_violation_request_dialog_offset = {};
    } catch (const std::exception& error) {
        show_notice(host, L"Rule Violation", error_to_wide(error.what()));
    }
    InvalidateRect(host.window, nullptr, FALSE);
}

void submit_new_channel_dialog(ClientHost& host) {
    if (!host.send_game_payload) return;
    try {
        if (!host.private_channel_name.empty()) {
            const auto name = encode_ui_string(host.private_channel_name);
            const auto payload = tibia::network::encode_state6_private_channel_request(name);
            if (!payload.empty()) host.send_game_payload(payload);
        } else if (host.new_channel_selected >= 0 &&
            static_cast<std::size_t>(host.new_channel_selected) < host.available_channels.size()) {
            const auto id = host.available_channels[
                static_cast<std::size_t>(host.new_channel_selected)].id;
            const auto payload = tibia::network::encode_state6_join_channel_request(id);
            host.send_game_payload(payload);
        } else {
            return;
        }
    } catch (const std::exception& error) {
        host.notice_title = L"Tibia";
        host.notice_text = error_to_wide(error.what());
        host.notice_dialog_offset = {};
        return;
    }
    host.new_channel_dialog_open = false;
    host.private_channel_name.clear();
    host.private_channel_focused = false;
    host.pressed_new_channel_button = -1;
    host.pressed_new_channel_scroll_button = -1;
    host.new_channel_scrollbar_dragging = false;
    InvalidateRect(host.window, nullptr, FALSE);
}

bool close_selected_console_channel(ClientHost& host) {
    if (host.screen != UiScreen::game_world || active_dialog(host) != ActiveDialog::none ||
        !host.send_game_payload) return false;
    const auto report_buffer = host.report_violation_buffer
        ? host.report_violation_buffer->snapshot()
        : tibia::client::ReportViolationBufferSnapshot{};
    if (report_buffer.open && host.report_violation_tab_selected) {
        const auto payload =
            tibia::network::encode_state6_close_rule_violation_request();
        try {
            host.send_game_payload(payload);
        } catch (const std::exception& error) {
            show_notice(host, L"Tibia", error_to_wide(error.what()));
            return true;
        }
        host.report_violation_buffer->close();
        host.report_violation_tab_selected = false;
        host.report_violation_buffer_was_open = false;
        host.console_input_focused = false;
        InvalidateRect(host.window, nullptr, FALSE);
        return true;
    }

    const auto view = host.communication_session_mailbox
        ? host.communication_session_mailbox->channel_view()
        : tibia::client::CommunicationChannelView{};
    if (!view.selected_identifier) return false;
    const auto channel = std::find_if(view.channels.begin(), view.channels.end(),
        [&view](const auto& entry) { return entry.id == *view.selected_identifier; });
    if (channel == view.channels.end()) return false;

    try {
        const auto payload = channel->id >= 9
            ? tibia::network::encode_state6_close_private_channel_request(channel->name)
            : tibia::network::encode_state6_close_channel_request(channel->id);
        if (payload.empty()) return false;
        host.send_game_payload(payload);
    } catch (const std::exception& error) {
        show_notice(host, L"Tibia", error_to_wide(error.what()));
        return true;
    }

    host.communication_session_mailbox->request_channel_close(channel->id);
    host.console_input_focused = false;
    return true;
}

RECT notice_button_rect(const ClientHost& host) {
    const auto dialog = notice_rect(host, host.notice_dialog_offset);
    const int left = dialog.right - ui_units(host.window, 56);
    const int top = dialog.bottom - ui_units(host.window, 30);
    return {left, top, left + ui_units(host.window, 43), top + ui_units(host.window, 20)};
}

void draw_message_dialog(ClientHost& host, std::wstring_view title,
    std::wstring_view text, int pressed_button, const RECT& dialog);

void draw_notice(ClientHost& host) {
    if (!host.notice_text.empty())
        draw_message_dialog(host, host.notice_title, host.notice_text,
            host.pressed_notice_button, notice_rect(host, host.notice_dialog_offset));
}

void draw_connection_status(ClientHost& host) {
    if (host.connection_status_text.empty()) return;
    RECT client{};
    GetClientRect(host.window, &client);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    const int line_height = ui_units(host.window,
        std::max(1, host.ui_font_heights[3]));
    const int maximum_width = std::max(1, client_width - ui_units(host.window, 24));
    int content_width = 0;
    std::size_t line_start = 0;
    for (std::size_t index = 0; index <= host.connection_status_text.size(); ++index) {
        if (index != host.connection_status_text.size() &&
            host.connection_status_text[index] != L'\n') continue;
        content_width = std::max(content_width, bitmap_text_width(host,
            std::wstring_view(host.connection_status_text).substr(line_start,
                index - line_start), 3, host.window));
        line_start = index + 1;
    }
    content_width = std::min(maximum_width - ui_units(host.window, 36), content_width);
    const int width = std::min(maximum_width,
        std::max(ui_units(host.window, 236), content_width + ui_units(host.window, 36)));
    const int text_width = std::max(1, width - ui_units(host.window, 36));
    const auto lines = wrap_bitmap_lines(host, host.connection_status_text, text_width, 3);
    const int line_count = std::max(1, static_cast<int>(lines.size()));
    const int height = std::min(std::max(1, client_height - ui_units(host.window, 24)),
        line_count * line_height + ui_units(host.window, 42));
    const RECT dialog{(client_width - width) / 2, (client_height - height) / 2,
        (client_width + width) / 2, (client_height + height) / 2};
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Please wait", dialog.left + ui_units(host.window, 5),
        dialog.top + ui_units(host.window, 5), width - ui_units(host.window, 10),
        true, 3, 144);
    draw_wrapped_bitmap_text(host, host.connection_status_text,
        dialog.left + ui_units(host.window, 18), dialog.top + ui_units(host.window, 32),
        text_width, std::max(1, height - ui_units(host.window, 42)),
        std::max(1, host.ui_font_heights[3]), 3, 192, true);
}

void draw_info_dialog(ClientHost& host) {
    const auto dialog = info_dialog_rect(host.window, host.info_dialog_offset);
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Info", dialog.left + s(5), dialog.top + s(5),
        s(234), true, 3, 144);

    const RECT panel_frame{dialog.left + s(18), dialog.top + s(32),
        dialog.left + s(226), dialog.top + s(161)};
    const RECT panel{panel_frame.left + s(9), panel_frame.top + s(9),
        panel_frame.right - s(9), panel_frame.bottom - s(9)};
    // The nine-skin panel wrapper adds 9 units around its 190x111 child.
    // Picture 4 provides its tiled center and the four source edge strips.
    draw_skin_tiled(host, 7, panel_frame.left + s(1), panel_frame.top + s(1),
        panel_frame.right - panel_frame.left - s(2),
        panel_frame.bottom - panel_frame.top - s(2));
    draw_skin_tiled(host, 9, panel_frame.left, panel_frame.top,
        panel_frame.right - panel_frame.left, s(1));
    draw_skin_tiled(host, 8, panel_frame.left, panel_frame.bottom - s(1),
        panel_frame.right - panel_frame.left, s(1));
    draw_skin_tiled(host, 11, panel_frame.left, panel_frame.top + s(1),
        s(1), panel_frame.bottom - panel_frame.top - s(2));
    draw_skin_tiled(host, 10, panel_frame.right - s(1), panel_frame.top + s(1),
        s(1), panel_frame.bottom - panel_frame.top - s(2));

    constexpr std::array<std::wstring_view, 5> version_lines{
        L"Tibia Windows Client", L"Version 7.92 ", L"Copyright (C) 2002-2007",
        L"CipSoft GmbH", L"All rights reserved."};
    // Font 4's original cell is 8x16. The rendered glyph bounds can be shorter,
    // but the original multiline control advances by the cell height.
    constexpr int version_line_height = 16;
    for (std::size_t i = 0; i < version_lines.size(); ++i) {
        draw_bitmap_text(host, version_lines[i], panel.left,
            panel.top + s(static_cast<int>(i) * version_line_height),
            panel.right - panel.left, true, 3, 176);
    }

    // The Info window contains two separate separators: one inside its panel
    // and the standard footer divider above the dialog button row.
    host.renderer->draw(ui_separator_shadow_texture, panel.left, panel.top + s(74),
        panel.right - panel.left, s(1));
    host.renderer->draw(ui_separator_highlight_texture, panel.left, panel.top + s(75),
        panel.right - panel.left, s(1));

    const RECT website_button = info_website_rect(host.window, host.info_dialog_offset);
    draw_skin(host, host.pressed_info_button == 2 ? 19 : 18,
        website_button.left, website_button.top,
        website_button.right - website_button.left,
        website_button.bottom - website_button.top);
    draw_bitmap_text(host, L"Official Website", panel.left,
        panel.top + s(93), s(104), true, 3, 176);
    draw_bitmap_text(host, L"www.tibia.com", website_button.left,
        website_button.top + s(6), website_button.right - website_button.left,
        true, 2, 176);

    const int footer_y = dialog.top + s(181);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        s(220), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        s(220), s(1));

    const auto draw_button = [&](const RECT& button, std::wstring_view label, int button_id) {
        draw_skin(host, host.pressed_info_button == button_id ? 13 : 12, button.left, button.top,
            button.right - button.left, button.bottom - button.top);
        draw_bitmap_text(host, label, button.left, button.top + s(6),
            button.right - button.left, true, 2, 255);
    };
    draw_button(info_ok_rect(host.window, host.info_dialog_offset), L"Ok", 0);
}

void draw_help_dialog(ClientHost& host) {
    const auto dialog = help_dialog_rect(host.window, host.help_dialog_offset);
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Client Help", dialog.left + s(5), dialog.top + s(5),
        dialog.right - dialog.left - s(10), true, 3, 144);

    constexpr std::array<std::wstring_view, 6> labels{
        L"Client Help", L"Hints", L"Tutor Help", L"Rule Violation", L"Manual", L"FAQ"};
    constexpr std::array<std::wstring_view, 6> descriptions{
        L"Click here to get\ninfo on the client", L"Show previous hints",
        L"Ask a tutor for help", L"Report rule violations",
        L"Read the manual on\nthe website", L"Read the FAQ on the website"};
    constexpr std::array<int, 6> row_y{0, 33, 66, 99, 132, 165};
    const int content_x = dialog.left + s(27);
    const int content_y = dialog.top + s(41);
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const int row_top = content_y + s(row_y[i]);
        draw_skin(host, host.pressed_help_button == static_cast<int>(i) ? 19 : 18,
            content_x, row_top, s(86), s(20));
        draw_bitmap_text(host, labels[i], content_x, row_top + s(6), s(86), true, 2);
        std::size_t line_start = 0;
        int line = 0;
        for (std::size_t index = 0; index <= descriptions[i].size(); ++index) {
            if (index != descriptions[i].size() && descriptions[i][index] != L'\n') continue;
            draw_bitmap_text(host, descriptions[i].substr(line_start, index - line_start),
                content_x + s(99), row_top + line * s(14), dialog.right - content_x - s(112),
                false, 3, 192);
            if (index < descriptions[i].size()) {
                ++line;
                line_start = index + 1;
            }
        }
    }

    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12),
        dialog.top + s(255), s(280), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12),
        dialog.top + s(256), s(280), s(1));
    const auto button = help_ok_rect(host.window, host.help_dialog_offset);
    draw_skin(host, host.pressed_help_button == 6 ? 13 : 12, button.left, button.top,
        button.right - button.left, button.bottom - button.top);
    draw_bitmap_text(host, L"Ok", button.left, button.top + s(6),
        button.right - button.left, true, 2);
}

constexpr std::size_t hint_body_font_index = 1;

std::size_t hint_font_index(int font_number) {
    if (font_number < 1 || font_number > 4) return hint_body_font_index;
    return static_cast<std::size_t>(font_number - 1);
}

int hint_scaled_unit(HWND window, int value) {
    return value == 0 ? 0 : ui_units(window, value);
}

std::vector<tibia::client::HintLine> hint_document_lines(
    const ClientHost& host, std::wstring_view markup, int width) {
    const auto measure = [&host](std::wstring_view text, int font_number) {
        return bitmap_text_width(host, text, hint_font_index(font_number), host.window);
    };
    const auto scale = [window = host.window](int value) {
        return hint_scaled_unit(window, value);
    };
    return tibia::client::layout_hint_markup(
        tibia::client::parse_hint_markup(markup), std::max(1, width), measure, scale);
}

int hint_content_visible_lines(const ClientHost& host, const RECT& view) {
    const int line_height = ui_units(host.window,
        std::max(1, host.ui_font_heights[hint_body_font_index]));
    int max_spacing{};
    for (const auto& line : host.hint_content_lines)
        max_spacing = std::max(max_spacing,
            hint_scaled_unit(host.window, line.spacing_before + line.spacing_after));
    return std::max(1, static_cast<int>(view.bottom - view.top) /
        std::max(1, line_height + max_spacing));
}

void draw_hint_text_clipped(ClientHost& host, const tibia::client::HintLineRun& run,
    int x, int y, const RECT& clip) {
    if (!host.renderer) return;
    if (run.skin_id) {
        if (*run.skin_id < 0 || *run.skin_id >=
            static_cast<int>(tibia::client::ui_skins.size())) return;
        const auto& skin = tibia::client::ui_skins[static_cast<std::size_t>(*run.skin_id)];
        draw_skin_clipped(host, static_cast<std::uint16_t>(*run.skin_id), x, y,
            ui_units(host.window, skin.width), ui_units(host.window, skin.height), clip);
        return;
    }
    const auto font_index = hint_font_index(run.style.font_number);
    const float scale = ui_scale(host.window);
    const auto scaled = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(
            static_cast<float>(value) * scale)));
    };
    const auto rgb = (static_cast<std::uint32_t>(std::clamp(run.style.red, 0, 255)) << 16U) |
        (static_cast<std::uint32_t>(std::clamp(run.style.green, 0, 255)) << 8U) |
        static_cast<std::uint32_t>(std::clamp(run.style.blue, 0, 255));
    const int letter_spacing = static_cast<int>(std::lround(
        static_cast<float>(host.ui_font_spacing[font_index]) * scale));
    const int horizontal_space = hint_scaled_unit(host.window, run.style.horizontal_space);
    for (std::size_t index = 0; index < run.text.size(); ++index) {
        wchar_t character = run.text[index];
        if (character == L' ') {
            x += scaled(host.ui_fonts[font_index][12].advance);
        } else {
            if (character < 32 || character > 255) character = L'?';
            const auto glyph_index = static_cast<std::size_t>(character - 32);
            const auto& glyph = host.ui_fonts[font_index][glyph_index];
            if (glyph.width > 0 && glyph.height > 0)
                host.renderer->draw_clipped(
                    hint_text_glyph_texture(host, rgb, font_index, glyph_index),
                    x, y, scaled(glyph.width), scaled(glyph.height), clip.left, clip.top,
                    clip.right - clip.left, clip.bottom - clip.top);
            x += scaled(glyph.advance);
        }
        if (index + 1 < run.text.size()) x += letter_spacing + horizontal_space;
    }
}

void draw_hints_dialog(ClientHost& host) {
    const auto dialog = hints_dialog_rect(host.window, host.hints_dialog_offset);
    const auto s = [window = host.window](int value) { return ui_units(window, value); };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Hints for Tibia", dialog.left + s(5), dialog.top + s(5),
        dialog.right - dialog.left - s(10), true, 3, 144);
    const RECT panel{dialog.left + s(18), dialog.top + s(32),
        dialog.left + s(218), dialog.top + s(151)};
    draw_skin_tiled(host, 7, panel.left + s(1), panel.top + s(1),
        panel.right - panel.left - s(2), panel.bottom - panel.top - s(2));
    draw_skin_tiled(host, 9, panel.left, panel.top,
        panel.right - panel.left, s(1));
    draw_skin_tiled(host, 8, panel.left, panel.bottom - s(1),
        panel.right - panel.left, s(1));
    draw_skin_tiled(host, 11, panel.left, panel.top + s(1),
        s(1), panel.bottom - panel.top - s(2));
    draw_skin_tiled(host, 10, panel.right - s(1), panel.top + s(1),
        s(1), panel.bottom - panel.top - s(2));
    draw_bitmap_text(host, L"Available hints:", dialog.left + s(18), dialog.top + s(32),
        s(200), false, 2, 176);

    const auto list = hints_list_rect(host.window, host.hints_dialog_offset);
    const int line_height = s(std::max(1, host.ui_font_heights[2]));
    const int visible = std::max(1, static_cast<int>(list.bottom - list.top) / line_height);
    host.hints_scroll = std::clamp(host.hints_scroll, 0,
        std::max(0, static_cast<int>(host.hints.size()) - visible));
    const auto geometry = options_list_scrollbar_geometry(host.window, list,
        host.hints.size(), visible, host.hints_scroll);
    const int text_width = list.right - list.left - s(21);
    for (int row = 0; row < visible; ++row) {
        const int index = host.hints_scroll + row;
        if (index >= static_cast<int>(host.hints.size())) break;
        const int y = list.top + row * line_height;
        if (index == host.hints_selected)
            host.renderer->draw(graphics_list_selection_texture, list.left + s(1), y,
                text_width - s(1), line_height);
        const RECT row_clip{list.left + s(1), y, list.left + text_width,
            std::min(static_cast<int>(list.bottom), y + line_height)};
        draw_bitmap_text_clipped(host,
            host.hints[static_cast<std::size_t>(index)].title,
            list.left + s(3), y, row_clip, 2, 176);
    }
    draw_ui_scrollbar_parts(host, list, geometry,
        host.pressed_hints_scroll_part == 0,
        host.pressed_hints_scroll_part == 1);

    const int footer_y = dialog.top + s(171);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        s(212), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        s(212), s(1));
    constexpr std::array<std::wstring_view, 2> labels{L"Ok", L"Cancel"};
    for (int index = 0; index < 2; ++index) {
        const auto button = hints_button_rect(host.window, host.hints_dialog_offset, index);
        draw_skin(host, host.pressed_hints_button == index ? 13 : 12,
            button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, labels[static_cast<std::size_t>(index)], button.left,
            button.top + s(6), button.right - button.left, true, 2, 255);
    }
}

void draw_hint_content_dialog(ClientHost& host) {
    if (!host.active_hint) return;
    const auto dialog = hint_content_dialog_rect(host.window,
        host.hint_content_dialog_offset);
    const auto s = [window = host.window](int value) { return ui_units(window, value); };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, host.active_hint->title, dialog.left + s(5), dialog.top + s(5),
        dialog.right - dialog.left - s(10), true, 3, 144);

    const auto view = hint_content_view_rect(host.window,
        host.hint_content_dialog_offset);
    const int line_height = s(std::max(1, host.ui_font_heights[hint_body_font_index]));
    const int visible = hint_content_visible_lines(host, view);
    const auto geometry = options_list_scrollbar_geometry(host.window, view,
        host.hint_content_lines.size(), visible, host.hint_content_scroll);
    int y = view.top;
    const RECT text_clip{view.left, view.top, view.right - s(18), view.bottom};
    for (int index = host.hint_content_scroll;
        index < static_cast<int>(host.hint_content_lines.size()) &&
            index < host.hint_content_scroll + visible; ++index) {
        const auto& line = host.hint_content_lines[static_cast<std::size_t>(index)];
        y += hint_scaled_unit(host.window, line.spacing_before);
        for (const auto& run : line.runs)
            draw_hint_text_clipped(host, run, view.left + s(3) + run.x_offset,
                y, text_clip);
        y += line_height + hint_scaled_unit(host.window, line.spacing_after);
    }
    draw_ui_scrollbar_parts(host, view, geometry,
        host.pressed_hint_content_scroll_part == 0,
        host.pressed_hint_content_scroll_part == 1);

    const int footer_y = dialog.top + s(392);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        s(412), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        s(412), s(1));
    const auto button = hint_content_ok_rect(host.window,
        host.hint_content_dialog_offset);
    draw_skin(host, host.pressed_hint_content_button == 0 ? 13 : 12,
        button.left, button.top, button.right - button.left,
        button.bottom - button.top);
    draw_bitmap_text(host, L"Ok", button.left, button.top + s(6),
        button.right - button.left, true, 2, 255);
}

void draw_report_violation_dialog(ClientHost& host) {
    const auto dialog = report_violation_dialog_rect(host.window,
        host.report_rule_violation_dialog_offset);
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Report Rule Violation", dialog.left + s(5),
        dialog.top + s(5), dialog.right - dialog.left - s(10), true, 3, 144);
    constexpr std::array<std::wstring_view, 5> lines{
        L"Please state the rule violation in one", L"clear sentence and wait for a reply",
        L"from a gamemaster.", L"Please note that your message will",
        L"disappear if you close this channel."};
    for (std::size_t index = 0; index < lines.size(); ++index)
        draw_bitmap_text(host, lines[index], dialog.left + s(18),
            dialog.top + s(static_cast<int>(index) * 14 + 34), s(250), false, 3, 192);

    const auto field = report_violation_input_rect(host.window,
        host.report_rule_violation_dialog_offset);
    const int inset = s(3);
    host.renderer->draw(login_field_background_texture, field.left, field.top,
        field.right - field.left, field.bottom - field.top);
    draw_login_field_bevel(host, field);
    draw_bitmap_text(host, host.report_violation_dialog.text, field.left + inset,
        field.top + inset, field.right - field.left - inset * 2, false, 3, 224);
    if (host.report_violation_dialog.text_focused &&
        ((GetTickCount64() / 500U) & 1U) == 0) {
        const int caret_x = std::min(static_cast<int>(field.right) - inset,
            static_cast<int>(field.left) + inset +
            bitmap_text_width(host, host.report_violation_dialog.text, 3, host.window));
        host.renderer->draw(login_caret_texture, caret_x,
            static_cast<int>(field.top) + inset,
            std::max(1, s(1)), std::max(1, static_cast<int>(field.bottom) -
                static_cast<int>(field.top) - inset * 2));
    }

    const int footer_y = dialog.top + s(139);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        s(262), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        s(262), s(1));
    constexpr std::array<std::wstring_view, 2> labels{L"Ok", L"Cancel"};
    for (int index = 0; index < 2; ++index) {
        const auto button = report_violation_button_rect(host.window,
            host.report_rule_violation_dialog_offset, index);
        draw_skin(host, host.report_violation_dialog.pressed_button == index ? 13 : 12,
            button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, labels[static_cast<std::size_t>(index)], button.left,
            button.top + s(6), button.right - button.left, true, 2);
    }
}

void draw_quest_list_control(ClientHost& host, const RECT& rect,
    const std::vector<std::wstring>& rows, int selected, int first_visible,
    int pressed_scroll_button) {
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int margin = s(tibia::client::quest_log_content_margin);
    const int scrollbar_width = s(tibia::client::quest_scrollbar_width);
    const int content_width = std::max(0, width - 2 * margin - scrollbar_width);
    host.renderer->draw(graphics_list_background_texture, rect.left, rect.top, width, height);
    draw_skin_tiled(host, 9, rect.left, rect.top, width, margin);
    draw_skin_tiled(host, 8, rect.left, rect.bottom - margin, width, margin);
    draw_skin_tiled(host, 11, rect.left, rect.top + margin, margin, height - 2 * margin);
    draw_skin_tiled(host, 10, rect.right - margin, rect.top + margin, margin,
        height - 2 * margin);
    const int row_height = s(tibia::client::quest_log_row_height);
    const int visible_rows = tibia::client::quest_log_visible_rows(height,
        row_height, margin);
    for (int visible = 0; visible < visible_rows; ++visible) {
        const int index = first_visible + visible;
        if (index < 0 || static_cast<std::size_t>(index) >= rows.size()) break;
        const int row_y = rect.top + margin + visible * row_height;
        if (index == selected)
            host.renderer->draw(graphics_list_selection_texture,
                rect.left + margin - s(1), row_y - s(1), content_width + s(2),
                row_height + s(2));
        draw_bitmap_text(host, rows[static_cast<std::size_t>(index)],
            rect.left + margin, row_y, content_width, false, 3,
            index == selected ? 255 : 176);
    }

    const auto scrollbar = quest_list_scrollbar_geometry(host.window, rect,
        rows.size(), first_visible);
    const auto up = quest_scrollbar_part_rect(host.window, rect, scrollbar.up_arrow);
    const auto down = quest_scrollbar_part_rect(host.window, rect, scrollbar.down_arrow);
    if (scrollbar.visible) {
        const auto track = quest_scrollbar_part_rect(host.window, rect, scrollbar.track);
        const auto thumb = quest_scrollbar_part_rect(host.window, rect, scrollbar.thumb);
        draw_skin_tiled(host, 153, track.left, track.top, track.right - track.left,
            track.bottom - track.top);
        const int cap = s(tibia::client::quest_scrollbar_thumb_cap_height);
        draw_skin_tiled(host, 154, thumb.left, thumb.top, thumb.right - thumb.left, cap);
        draw_skin_tiled(host, 155, thumb.left, thumb.top + cap, thumb.right - thumb.left,
            std::max(0, static_cast<int>(thumb.bottom - thumb.top) - 2 * cap));
        draw_skin_tiled(host, 156, thumb.left, thumb.bottom - cap,
            thumb.right - thumb.left, cap);
    }
    draw_skin(host, pressed_scroll_button == 0 ? 158 : 157, up.left, up.top,
        up.right - up.left, up.bottom - up.top);
    draw_skin(host, pressed_scroll_button == 1 ? 160 : 159, down.left, down.top,
        down.right - down.left, down.bottom - down.top);
}

std::vector<std::wstring> quest_log_row_text(const ClientHost& host,
    const tibia::client::GameWorldSnapshot* snapshot) {
    std::vector<std::wstring> rows;
    if (!snapshot) return rows;
    rows.reserve(snapshot->quest_log_entries.size());
    for (const auto& entry : snapshot->quest_log_entries)
        rows.push_back(decode_ui_string(entry.text));
    return rows;
}

void draw_quest_log_dialog(ClientHost& host) {
    const auto dialog = quest_log_dialog_rect(host.window, host.quest_log_dialog_offset);
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Quest Log", dialog.left + s(5), dialog.top + s(5),
        dialog.right - dialog.left - s(10), true, 3, 144);
    const auto snapshot = host.game_world_mailbox->read();
    const auto rows = quest_log_row_text(host, snapshot.get());
    const int visible_rows = std::max(1, tibia::client::quest_log_visible_rows(
        ui_units(host.window, 268), s(tibia::client::quest_log_row_height),
        s(tibia::client::quest_log_content_margin)));
    host.quest_log_scroll = std::clamp(host.quest_log_scroll, 0,
        std::max(0, static_cast<int>(rows.size()) - visible_rows));
    draw_quest_list_control(host, quest_log_list_rect(host.window,
        host.quest_log_dialog_offset), rows, host.quest_log_selected,
        host.quest_log_scroll, host.quest_line_open ? -1 : host.pressed_quest_scroll_button);
    const int footer_y = dialog.bottom - s(40);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        dialog.right - dialog.left - s(24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        dialog.right - dialog.left - s(24), s(1));
    for (int index = 0; index < 2; ++index) {
        const auto button = quest_log_button_rect(host.window,
            host.quest_log_dialog_offset, index);
        const auto label = index == 0 ? std::wstring_view(L"Show") : std::wstring_view(L"Close");
        draw_skin(host, host.pressed_quest_log_button == index ? 13 : 12,
            button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, label, button.left, button.top + s(6),
            button.right - button.left, true, 2, 255);
    }
}

void draw_quest_line_dialog(ClientHost& host) {
    const auto dialog = quest_line_dialog_rect(host.window, host.quest_line_dialog_offset);
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Quest Line", dialog.left + s(5), dialog.top + s(5),
        dialog.right - dialog.left - s(10), true, 3, 144);
    const auto snapshot = host.game_world_mailbox->read();
    const auto line = snapshot ? std::find_if(snapshot->quest_lines.begin(),
        snapshot->quest_lines.end(), [id = host.quest_line_id](const auto& entry) {
            return entry.id == id;
        }) : std::vector<tibia::network::State6QuestLine>::const_iterator{};
    if (snapshot && line != snapshot->quest_lines.end()) {
        const int content_x = dialog.left + s(18);
        const int content_y = dialog.top + s(32);
        draw_bitmap_text(host, decode_ui_string(line->name), content_x, content_y,
            s(300), false, 3, 192);
        if (host.quest_line_flags_pending &&
            snapshot->quest_flags_revision > host.quest_line_request_revision &&
            snapshot->last_quest_flags_id == host.quest_line_id)
            host.quest_line_flags_pending = false;
        std::vector<std::wstring> flag_rows;
        if (!host.quest_line_flags_pending) {
            flag_rows.reserve(line->flags.size());
            for (const auto& flag : line->flags)
                flag_rows.push_back(decode_ui_string(flag.name));
        }
        const auto flags_rect = quest_line_flags_rect(host.window,
            host.quest_line_dialog_offset);
        host.quest_flag_scroll = std::clamp(host.quest_flag_scroll, 0,
            tibia::client::quest_log_max_scroll(flag_rows.size(),
                ui_units(host.window, 118), ui_units(host.window,
                    tibia::client::quest_log_row_height), ui_units(host.window,
                    tibia::client::quest_log_content_margin)));
        draw_quest_list_control(host, flags_rect, flag_rows,
            host.quest_flag_selected, host.quest_flag_scroll,
            host.quest_line_open ? host.pressed_quest_scroll_button : -1);
        if (!host.quest_line_flags_pending && host.quest_flag_selected >= 0 &&
            static_cast<std::size_t>(host.quest_flag_selected) < line->flags.size()) {
            const auto description = decode_ui_string(
                line->flags[static_cast<std::size_t>(host.quest_flag_selected)].description);
            const auto description_lines = wrap_bitmap_lines(host, description,
                s(300), 3);
            for (std::size_t index = 0; index < description_lines.size(); ++index) {
                const int y = dialog.top + s(172) + static_cast<int>(index) * s(16);
                if (y + s(16) > dialog.top + s(290)) break;
                draw_bitmap_text(host, description_lines[index], content_x, y,
                    s(300), false, 3, 176);
            }
        }
    }
    const int footer_y = dialog.bottom - s(40);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        dialog.right - dialog.left - s(24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        dialog.right - dialog.left - s(24), s(1));
    const auto button = quest_line_close_rect(host.window, host.quest_line_dialog_offset);
    draw_skin(host, host.pressed_quest_line_button == 0 ? 13 : 12,
        button.left, button.top, button.right - button.left,
        button.bottom - button.top);
    draw_bitmap_text(host, L"Close", button.left, button.top + s(6),
        button.right - button.left, true, 2, 255);
}

std::size_t quest_scroll_entry_count(ClientHost& host, bool flags) {
    const auto snapshot = host.game_world_mailbox->read();
    if (!snapshot) return 0;
    if (!flags) return snapshot->quest_log_entries.size();
    if (host.quest_line_flags_pending) return 0;
    const auto line = std::find_if(snapshot->quest_lines.begin(), snapshot->quest_lines.end(),
        [id = host.quest_line_id](const auto& entry) { return entry.id == id; });
    return line == snapshot->quest_lines.end() ? 0 : line->flags.size();
}

RECT quest_scroll_list_rect(ClientHost& host, bool flags) {
    return flags
        ? quest_line_flags_rect(host.window, host.quest_line_dialog_offset)
        : quest_log_list_rect(host.window, host.quest_log_dialog_offset);
}

int& quest_scroll_first_visible(ClientHost& host, bool flags) {
    return flags ? host.quest_flag_scroll : host.quest_log_scroll;
}

int quest_scroll_viewport_height(HWND window, bool flags) {
    return ui_units(window, flags ? 118 : 268);
}

bool begin_quest_scroll_interaction(ClientHost& host, POINT mouse, bool flags) {
    const auto list = quest_scroll_list_rect(host, flags);
    const auto count = quest_scroll_entry_count(host, flags);
    const auto geometry = quest_list_scrollbar_geometry(host.window, list, count,
        quest_scroll_first_visible(host, flags));
    const auto up = quest_scrollbar_part_rect(host.window, list, geometry.up_arrow);
    const auto down = quest_scrollbar_part_rect(host.window, list, geometry.down_arrow);
    host.pressed_quest_scroll_button = -1;
    if (contains_point(up, mouse)) host.pressed_quest_scroll_button = 0;
    else if (contains_point(down, mouse)) host.pressed_quest_scroll_button = 1;
    else if (geometry.visible && contains_point(
            quest_scrollbar_part_rect(host.window, list, geometry.track), mouse)) {
        const auto thumb = quest_scrollbar_part_rect(host.window, list, geometry.thumb);
        host.quest_scroll_drag_on_flags = flags;
        if (contains_point(thumb, mouse)) {
            host.quest_scroll_dragging = true;
            host.quest_scroll_drag_grab_offset = mouse.y - thumb.top;
        } else {
            host.pressed_quest_scroll_button = 2;
        }
    } else {
        return false;
    }
    if (host.pressed_quest_scroll_button >= 0)
        host.quest_scroll_drag_on_flags = flags;
    return true;
}

void update_quest_scroll_drag(ClientHost& host, POINT mouse) {
    if (!host.quest_scroll_dragging) return;
    const bool flags = host.quest_scroll_drag_on_flags;
    const auto list = quest_scroll_list_rect(host, flags);
    const auto count = quest_scroll_entry_count(host, flags);
    const int height = quest_scroll_viewport_height(host.window, flags);
    const int row_height = ui_units(host.window, tibia::client::quest_log_row_height);
    const int margin = ui_units(host.window, tibia::client::quest_log_content_margin);
    const auto geometry = quest_list_scrollbar_geometry(host.window, list, count,
        quest_scroll_first_visible(host, flags));
    quest_scroll_first_visible(host, flags) = tibia::client::quest_log_scroll_after_thumb_drag(
        mouse.y - list.top, host.quest_scroll_drag_grab_offset, geometry,
        quest_scroll_first_visible(host, flags), count, height, row_height, margin);
}

void finish_quest_scroll_interaction(ClientHost& host, POINT mouse) {
    const bool flags = host.quest_scroll_drag_on_flags;
    if (host.pressed_quest_scroll_button >= 0) {
        const auto list = quest_scroll_list_rect(host, flags);
        const auto count = quest_scroll_entry_count(host, flags);
        const auto geometry = quest_list_scrollbar_geometry(host.window, list, count,
            quest_scroll_first_visible(host, flags));
        const int row_height = ui_units(host.window, tibia::client::quest_log_row_height);
        const int margin = ui_units(host.window, tibia::client::quest_log_content_margin);
        const int height = quest_scroll_viewport_height(host.window, flags);
        if (host.pressed_quest_scroll_button == 0 && contains_point(
                quest_scrollbar_part_rect(host.window, list, geometry.up_arrow), mouse)) {
            quest_scroll_first_visible(host, flags) = std::max(0,
                quest_scroll_first_visible(host, flags) - 1);
        } else if (host.pressed_quest_scroll_button == 1 && contains_point(
                quest_scrollbar_part_rect(host.window, list, geometry.down_arrow), mouse)) {
            quest_scroll_first_visible(host, flags) = std::min(
                tibia::client::quest_log_max_scroll(count, height, row_height, margin),
                quest_scroll_first_visible(host, flags) + 1);
        } else if (host.pressed_quest_scroll_button == 2 && contains_point(
                quest_scrollbar_part_rect(host.window, list, geometry.track), mouse)) {
            quest_scroll_first_visible(host, flags) =
                tibia::client::quest_log_scroll_after_track_click(mouse.y - list.top,
                    geometry, quest_scroll_first_visible(host, flags), count, height,
                    row_height, margin);
        }
    }
    host.pressed_quest_scroll_button = -1;
    host.quest_scroll_dragging = false;
}

RECT motd_button_rect(const ClientHost& host, int button_index) {
    if (button_index != 0) return {};
    const auto dialog = motd_dialog_rect(host, host.motd_dialog_offset);
    const int right_offset = 56;
    const int left = dialog.right - ui_units(host.window, right_offset);
    const int top = dialog.bottom - ui_units(host.window, 30);
    return {left, top, left + ui_units(host.window, 43), top + ui_units(host.window, 20)};
}

RECT account_data_warning_button_rect(const ClientHost& host) {
    const auto dialog = account_data_warning_dialog_rect(host,
        host.account_data_warning_dialog_offset);
    const int left = dialog.right - ui_units(host.window, 56);
    const int top = dialog.bottom - ui_units(host.window, 30);
    return {left, top, left + ui_units(host.window, 43), top + ui_units(host.window, 20)};
}

void draw_message_dialog(ClientHost& host, std::wstring_view title,
    std::wstring_view text, int pressed_button, const RECT& dialog) {
    const int width = dialog.right - dialog.left;
    const int line_height = ui_units(host.window, std::max(1, host.ui_font_heights[3]));
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, title, dialog.left + ui_units(host.window, 5),
        dialog.top + ui_units(host.window, 5), width - ui_units(host.window, 10), true, 3, 144);

    std::size_t line_start = 0;
    int line_number = 0;
    for (std::size_t index = 0; index <= text.size(); ++index) {
        if (index != text.size() && text[index] != L'\n') continue;
        draw_bitmap_text(host, text.substr(line_start,
            index - line_start), dialog.left + ui_units(host.window, 18),
            dialog.top + ui_units(host.window, 32) + line_number * line_height,
            0, false, 3, 192);
        if (index < text.size()) {
            ++line_number;
            line_start = index + 1;
        }
    }

    const int separator_y = dialog.bottom - ui_units(host.window, 40);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + ui_units(host.window, 12),
        separator_y, width - ui_units(host.window, 24), ui_units(host.window, 1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + ui_units(host.window, 12),
        separator_y + ui_units(host.window, 1), width - ui_units(host.window, 24),
        ui_units(host.window, 1));

    const int button_left = dialog.right - ui_units(host.window, 56);
    const int button_top = dialog.bottom - ui_units(host.window, 30);
    const RECT button{button_left, button_top,
        button_left + ui_units(host.window, 43), button_top + ui_units(host.window, 20)};
    draw_skin(host, pressed_button == 0 ? 13 : 12,
        button.left, button.top, button.right - button.left,
        button.bottom - button.top);
    draw_bitmap_text(host, L"Ok", button.left, button.top + ui_units(host.window, 6),
        button.right - button.left, true, 2, 255);
}

void draw_motd_dialog(ClientHost& host) {
    draw_message_dialog(host, L"Message of the Day", host.motd_text,
        host.pressed_motd_button, motd_dialog_rect(host, host.motd_dialog_offset));
}

void draw_account_data_warning_dialog(ClientHost& host) {
    draw_message_dialog(host, L"Account Data Warning", account_data_warning_text,
        host.pressed_account_data_warning_button,
        account_data_warning_dialog_rect(host, host.account_data_warning_dialog_offset));
}

void draw_options_controls(ClientHost& host, const RECT& dialog) {
    const int x = dialog.left;
    const int y = dialog.top;
    const int width = dialog.right - dialog.left;
    const float scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_bitmap_text(host, L"Options", x + s(8), y + s(4), width - s(16), true, 3, 144);

    constexpr std::array<std::wstring_view, 6> labels{
        L"General", L"Graphics", L"Console", L"Network", L"Hotkeys", L"Motd"};
    constexpr std::array<std::wstring_view, 6> descriptions{
        L"Change general", L"Change graphics and", L"Customise the console",
        L"Change network settings", L"Edit your hotkey texts", L"Show the most recent"};
    constexpr std::array<std::wstring_view, 3> second_lines{
        L"game options", L"performance settings", L"Message of the Day"};
    constexpr std::array<int, 6> row_y{32, 65, 98, 131, 164, 214};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const int row_top = y + s(row_y[i]);
        const int button_x = x + s(18);
        draw_skin(host, host.pressed_options_button == static_cast<int>(i) ? 19 : 18,
            button_x, row_top, s(86), s(20));
        draw_bitmap_text(host, labels[i], button_x, row_top + s(6), s(86), true, 2);
        const int text_x = x + s(116);
        draw_bitmap_text(host, descriptions[i], text_x, row_top, width - s(128), false, 3, 176);
        if (i == 0 || i == 1 || i == 5) {
            const auto second_line = i == 0 ? second_lines[0] :
                i == 1 ? second_lines[1] : second_lines[2];
            draw_bitmap_text(host, second_line, text_x, row_top + s(14),
                width - s(128), false, 3, 176);
        }
    }

    host.renderer->draw(ui_separator_shadow_texture, x + s(12), y + s(198), s(256), s(1));
    host.renderer->draw(ui_separator_highlight_texture, x + s(12), y + s(199), s(256), s(1));
    host.renderer->draw(ui_separator_shadow_texture, x + s(34), y + s(255), s(234), s(1));
    host.renderer->draw(ui_separator_highlight_texture, x + s(34), y + s(256), s(234), s(1));

    const int ok_x = x + s(230);
    const int ok_y = y + s(264);
    draw_skin(host, host.pressed_options_button == 6 ? 13 : 12,
        ok_x, ok_y, s(43), s(20));
    draw_bitmap_text(host, L"Ok", ok_x, ok_y + s(6), s(43), true, 2);
}

void draw_console_options_page(ClientHost& host) {
    const auto dialog = console_options_dialog_rect(host.window,
        host.console_options_dialog_offset);
    const auto scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Console Options", dialog.left + s(5),
        dialog.top + s(5), s(tibia::client::console_options_dialog_width - 10),
        true, 3, 144);

    const int content_x = dialog.left + s(tibia::client::console_options_content_x);
    const int content_y = dialog.top + s(tibia::client::console_options_content_y);
    for (std::size_t index = 0; index < tibia::client::console_options_labels.size(); ++index) {
        const int row_y = content_y + s(tibia::client::console_options_row_y[index]);
        draw_checkbox_row_background(host, {content_x, row_y,
            content_x + s(tibia::client::console_options_content_width), row_y + s(22)});
        draw_skin(host, host.console_options_values[index] ? 150 : 151,
            content_x + s(6), row_y + s(5), s(12), s(12));
        draw_bitmap_text(host, tibia::client::console_options_labels[index],
            content_x + s(24), centered_bitmap_text_y(host, row_y + s(5), s(12), 3),
            s(tibia::client::console_options_content_width - 34), false, 3, 176);
    }

    const int width = dialog.right - dialog.left;
    const int height = dialog.bottom - dialog.top;
    const int footer_y = dialog.top + height - s(40);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12),
        footer_y, width - s(24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12),
        footer_y + s(1), width - s(24), s(1));

    constexpr std::array<std::wstring_view, 3> button_labels{L"Help", L"Ok", L"Cancel"};
    const int button_y = dialog.top + height - s(30);
    for (std::size_t index = 0; index < button_labels.size(); ++index) {
        const int button_x = dialog.left + width - s(13 + 43 + 106) +
            static_cast<int>(index) * s(53);
        const int button_id = static_cast<int>(index) + 6;
        draw_skin(host, host.pressed_console_options_control == button_id ? 13 : 12,
            button_x, button_y, s(43), s(20));
        draw_bitmap_text(host, button_labels[index], button_x, button_y + s(6),
            s(43), true, 2);
    }
}

void draw_hotkey_options_page(ClientHost& host) {
    const auto dialog = hotkey_options_dialog_rect(host.window,
        host.hotkey_options_dialog_offset);
    const auto scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Hotkey Options", dialog.left + s(5), dialog.top + s(5),
        s(tibia::client::hotkey_options_dialog_width - 10), true, 3, 144);

    const auto label_rect = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, 0, 0, 0, 14);
    draw_bitmap_text(host, L"Available hotkeys:", label_rect.left, label_rect.top,
        label_rect.right - label_rect.left, false, 3, 176);

    const auto list = hotkey_options_list_rect(host.window, host.hotkey_options_dialog_offset);
    host.renderer->draw(graphics_list_background_texture, list.left, list.top,
        list.right - list.left, list.bottom - list.top);
    host.renderer->draw(ui_separator_shadow_texture, list.left, list.top,
        list.right - list.left, s(1));
    host.renderer->draw(ui_separator_shadow_texture, list.left, list.top, s(1),
        list.bottom - list.top);
    host.renderer->draw(ui_separator_highlight_texture, list.right - s(1), list.top,
        s(1), list.bottom - list.top);
    host.renderer->draw(ui_separator_highlight_texture, list.left, list.bottom - s(1),
        list.right - list.left, s(1));
    const int list_content_right = list.right - s(15);
    const std::size_t first = host.hotkey_list_scroll;
    for (std::size_t visible = 0; visible < tibia::client::hotkey_list_visible_rows; ++visible) {
        const auto index = first + visible;
        if (index >= tibia::client::hotkey_count) break;
        const int row_y = list.top + s(2) + static_cast<int>(visible) *
            s(tibia::client::hotkey_list_row_height);
        if (index == host.selected_hotkey)
            host.renderer->draw(graphics_list_selection_texture, list.left + s(1), row_y,
                list_content_right - list.left - s(1),
                s(tibia::client::hotkey_list_row_height));
        const auto line = tibia::client::format_hotkey_entry(index,
            host.hotkey_options_values[index]);
        draw_bitmap_text(host, decode_ui_string(line), list.left + s(3), row_y,
            list_content_right - list.left - s(6), false, 3, 176);
    }
    draw_options_list_scrollbar(host, list,
        ClientHost::ListScrollbarTarget::hotkeys, tibia::client::hotkey_count,
        static_cast<int>(tibia::client::hotkey_list_visible_rows),
        static_cast<int>(host.hotkey_list_scroll));

    const auto text_label = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, 0, 132, 0, 14);
    draw_bitmap_text(host, L"Edit hotkey text:", text_label.left, text_label.top,
        text_label.right - text_label.left, false, 3, 176);
    const auto text_field = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, 0, tibia::client::hotkey_text_y, 0,
        tibia::client::hotkey_text_height);
    host.renderer->draw(login_field_background_texture, text_field.left, text_field.top,
        text_field.right - text_field.left, text_field.bottom - text_field.top);
    draw_login_field_bevel(host, text_field);
    draw_bitmap_text(host, host.hotkey_text, text_field.left + s(3), text_field.top + s(3),
        text_field.right - text_field.left - s(7), false, 3, 255);
    if (host.hotkey_text_focused && host.login_caret_phase == 0) {
        const int caret_x = text_field.left + s(3) + static_cast<int>(std::lround(
            host.hotkey_text.size() * s(5)));
        host.renderer->draw(login_caret_texture, caret_x, text_field.top + s(3), s(1), s(10));
    }

    const auto automatic = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, 0, tibia::client::hotkey_automatic_y, 0,
        tibia::client::hotkey_automatic_height);
    draw_checkbox_row_background(host, automatic);
    draw_skin(host, host.hotkey_send_automatically ? 150 : 151,
        automatic.left + s(6), automatic.top + s(5), s(12), s(12));
    draw_bitmap_text(host, L"Send automatically", automatic.left + s(24),
        centered_bitmap_text_y(host, automatic.top + s(5), s(12), 3),
        automatic.right - automatic.left - s(30), false, 3, 176);

    const auto object_label = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, 0, tibia::client::hotkey_object_label_y, 0,
        tibia::client::hotkey_object_label_height);
    draw_bitmap_text(host, L"Edit hotkey object:", object_label.left, object_label.top,
        object_label.right - object_label.left, false, 3, 176);
    const auto object = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, tibia::client::hotkey_object_x,
        tibia::client::hotkey_object_y, tibia::client::hotkey_object_size,
        tibia::client::hotkey_object_size);
    host.renderer->draw(graphics_list_background_texture, object.left, object.top,
        object.right - object.left, object.bottom - object.top);
    const auto object_id = host.hotkey_options_values[host.selected_hotkey].data[0];
    if (object_id > 0 && object_id <= 65535) {
        upload_hotkey_object_preview(host, object_id);
        if (host.hotkey_object_preview_id == object_id)
            host.renderer->draw(hotkey_object_preview_texture,
                object.left + (object.right - object.left - s(32)) / 2,
                object.top + (object.bottom - object.top - s(32)) / 2,
                s(32), s(32));
    }
    const auto select = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, tibia::client::hotkey_select_x,
        tibia::client::hotkey_select_y, tibia::client::hotkey_button_width,
        tibia::client::hotkey_button_height);
    draw_bitmap_text(host, L"Select Object", select.left, select.top + s(6),
        select.right - select.left, true, 2);
    const auto clear = hotkey_options_control_rect(host.window,
        host.hotkey_options_dialog_offset, tibia::client::hotkey_clear_x,
        tibia::client::hotkey_clear_y, tibia::client::hotkey_button_width,
        tibia::client::hotkey_button_height);
    draw_skin(host, host.pressed_hotkey_options_control == 1 ? 19 : 18,
        clear.left, clear.top, clear.right - clear.left, clear.bottom - clear.top);
    draw_bitmap_text(host, L"Clear Object", clear.left, clear.top + s(6),
        clear.right - clear.left, true, 2);
    for (std::size_t index = 0; index < tibia::client::hotkey_mode_labels.size(); ++index) {
        const auto button = hotkey_options_control_rect(host.window,
            host.hotkey_options_dialog_offset, tibia::client::hotkey_mode_x,
            tibia::client::hotkey_mode_y[index], tibia::client::hotkey_button_width,
            tibia::client::hotkey_button_height);
        const bool selected = host.hotkey_options_values[host.selected_hotkey].data[0] != 0 &&
            host.hotkey_options_values[host.selected_hotkey].data[2] ==
                static_cast<std::int32_t>(index == 0 ? 2 : index == 1 ? 1 : 0);
        draw_skin(host, selected || host.pressed_hotkey_options_control ==
            static_cast<int>(index + 2) ? 19 : 18, button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, decode_ui_string(tibia::client::hotkey_mode_labels[index]),
            button.left, button.top + s(6), button.right - button.left, true, 2,
            selected ? 255 : 176);
    }

    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12),
        dialog.bottom - s(40), dialog.right - dialog.left - s(24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12),
        dialog.bottom - s(39), dialog.right - dialog.left - s(24), s(1));
    constexpr std::array<std::wstring_view, 2> footer_labels{L"Ok", L"Cancel"};
    for (std::size_t index = 0; index < footer_labels.size(); ++index) {
        const auto button = hotkey_options_footer_button_rect(host.window,
            host.hotkey_options_dialog_offset, index);
        draw_skin(host, host.pressed_hotkey_options_control == static_cast<int>(index + 5)
            ? 13 : 12, button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, footer_labels[index], button.left, button.top + s(6),
            button.right - button.left, true, 2);
    }
}

void draw_general_options_page(ClientHost& host) {
    const auto dialog = general_options_dialog_rect(host.window,
        host.general_options_dialog_offset);
    const auto scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"General Options", dialog.left + s(5),
        dialog.top + s(5), s(tibia::client::general_options_dialog_width - 10),
        true, 3, 144);

    const int content_x = dialog.left + s(tibia::client::general_options_content_x);
    const int content_y = dialog.top + s(tibia::client::general_options_content_y);
    for (std::size_t index = 0; index < tibia::client::general_options_labels.size(); ++index) {
        const int row_y = content_y + s(tibia::client::general_options_row_y[index]);
        draw_checkbox_row_background(host, {content_x, row_y,
            content_x + s(tibia::client::general_options_content_width), row_y + s(22)});
        draw_skin(host, host.general_options_values[index] ? 150 : 151,
            content_x + s(6), row_y + s(5), s(12), s(12));
        draw_bitmap_text(host, tibia::client::general_options_labels[index],
            content_x + s(24), centered_bitmap_text_y(host, row_y + s(5), s(12), 3),
            s(tibia::client::general_options_content_width - 34), false, 3, 176);
    }

    const int width = dialog.right - dialog.left;
    const int height = dialog.bottom - dialog.top;
    const int footer_y = dialog.top + height - s(40);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12),
        footer_y, width - s(24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12),
        footer_y + s(1), width - s(24), s(1));

    constexpr std::array<std::wstring_view, 2> labels{L"Ok", L"Cancel"};
    const int button_y = dialog.top + height - s(30);
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const auto button = general_options_button_rect(host.window,
            host.general_options_dialog_offset, index);
        const int button_id = static_cast<int>(index) + 5;
        draw_skin(host, host.pressed_general_options_control == button_id ? 13 : 12,
            button.left, button.top, button.right - button.left, button.bottom - button.top);
        draw_bitmap_text(host, labels[index], button.left, button_y + s(6),
            button.right - button.left, true, 2);
    }
}

void draw_network_options_page(ClientHost& host) {
    const auto dialog = network_options_dialog_rect(host.window,
        host.network_options_dialog_offset);
    const auto scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Network Options", dialog.left + s(5),
        dialog.top + s(5), s(tibia::client::network_options_dialog_width - 10),
        true, 3, 144);

    const int content_x = dialog.left + s(tibia::client::network_options_content_x);
    const int content_y = dialog.top + s(tibia::client::network_options_content_y);
    draw_checkbox_row_background(host, {content_x, content_y,
        content_x + s(tibia::client::network_options_content_width), content_y + s(22)});
    draw_skin(host, host.network_options_value ? 150 : 151,
        content_x + s(6), content_y + s(5), s(12), s(12));
    draw_bitmap_text(host, tibia::client::network_options_labels[0],
        content_x + s(24), centered_bitmap_text_y(host, content_y + s(5), s(12), 3),
        s(tibia::client::network_options_content_width - 34), false, 3, 176);

    const int width = dialog.right - dialog.left;
    const int height = dialog.bottom - dialog.top;
    const int footer_y = dialog.top + height - s(40);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12),
        footer_y, width - s(24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12),
        footer_y + s(1), width - s(24), s(1));

    constexpr std::array<std::wstring_view, 3> labels{L"Help", L"Ok", L"Cancel"};
    const int button_y = dialog.top + height - s(30);
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const auto button = network_options_button_rect(host.window,
            host.network_options_dialog_offset, index);
        const int button_id = static_cast<int>(index) + 1;
        draw_skin(host, host.pressed_network_options_control == button_id ? 13 : 12,
            button.left, button.top, button.right - button.left, button.bottom - button.top);
        draw_bitmap_text(host, labels[index], button.left, button_y + s(6),
            button.right - button.left, true, 2);
    }
}

void draw_graphics_options_page(ClientHost& host) {
    const auto dialog = graphics_options_dialog_rect(host.window,
        host.graphics_options_dialog_offset);
    const auto scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Graphics Options", dialog.left + s(5),
        dialog.top + s(5), s(tibia::client::graphics_options_dialog_width - 10),
        true, 3, 144);

    const int content_x = dialog.left + s(tibia::client::graphics_options_content_x);
    const int content_y = dialog.top + s(tibia::client::graphics_options_content_y);
    draw_checkbox_row_background(host, {content_x, content_y,
        content_x + s(tibia::client::graphics_options_content_width), content_y + s(22)});
    draw_skin(host, host.graphics_fullscreen_value ? 150 : 151,
        content_x + s(6), content_y + s(5), s(12), s(12));
    draw_bitmap_text(host, L"Fullscreen", content_x + s(24),
        centered_bitmap_text_y(host, content_y + s(5), s(12), 3),
        s(tibia::client::graphics_options_content_width - 34), false, 3, 176);

    draw_bitmap_text(host, L"Available resolutions", content_x,
        content_y + s(tibia::client::graphics_options_label_y),
        s(tibia::client::graphics_options_content_width), false, 3, 176);

    const auto list = graphics_options_list_rect(host.window,
        host.graphics_options_dialog_offset);
    const int list_width = list.right - list.left;
    const int list_height = list.bottom - list.top;
    host.renderer->draw(graphics_list_background_texture, list.left, list.top,
        list_width, list_height);
    host.renderer->draw(ui_separator_shadow_texture, list.left, list.top,
        list_width, s(1));
    host.renderer->draw(ui_separator_shadow_texture, list.left, list.top,
        s(1), list_height);
    host.renderer->draw(ui_separator_highlight_texture, list.right - s(1), list.top,
        s(1), list_height);
    host.renderer->draw(ui_separator_highlight_texture, list.left, list.bottom - s(1),
        list_width, s(1));
    const int list_content_width = std::max(0, list_width - s(15));
    const int first = std::max(0, host.graphics_mode_scroll);
    const int row_height = s(tibia::client::graphics_options_row_height);
    for (int visible = 0; visible < tibia::client::graphics_options_visible_rows; ++visible) {
        const int index = first + visible;
        if (static_cast<std::size_t>(index) >= host.graphics_display_modes.size()) break;
        const int row_y = list.top + s(3) + visible * row_height;
        if (index == host.graphics_selected_mode)
            host.renderer->draw(graphics_list_selection_texture, list.left + s(1),
                row_y - s(1), list_content_width - s(1), row_height);
        draw_bitmap_text(host, host.graphics_display_modes[static_cast<std::size_t>(index)].label,
            list.left + s(3), centered_bitmap_text_y(host, row_y - s(1), row_height, 3),
            list_content_width - s(6), false, 3, 176);
    }
    draw_options_list_scrollbar(host, list,
        ClientHost::ListScrollbarTarget::graphics_modes,
        host.graphics_display_modes.size(),
        tibia::client::graphics_options_visible_rows, host.graphics_mode_scroll);

    const auto advanced = graphics_options_advanced_rect(host.window,
        host.graphics_options_dialog_offset);
    draw_skin(host, host.pressed_graphics_options_control == 4 ? 19 : 18,
        advanced.left, advanced.top, advanced.right - advanced.left,
        advanced.bottom - advanced.top);
    draw_bitmap_text(host, L"Advanced", advanced.left, advanced.top + s(6),
        advanced.right - advanced.left, true, 2);
    draw_bitmap_text(host, L"Advanced settings", content_x,
        content_y + s(tibia::client::graphics_options_advanced_y + 4),
        s(110), false, 3, 176);

    const int footer_y = dialog.top + s(tibia::client::graphics_options_footer_y);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        s(tibia::client::graphics_options_dialog_width - 24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        s(tibia::client::graphics_options_dialog_width - 24), s(1));
    constexpr std::array<std::wstring_view, 3> labels{L"Help", L"Ok", L"Cancel"};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const auto button = graphics_options_button_rect(host.window,
            host.graphics_options_dialog_offset, index);
        draw_skin(host, host.pressed_graphics_options_control == static_cast<int>(index + 1)
            ? 13 : 12, button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, labels[index], button.left, button.top + s(6),
            button.right - button.left, true, 2);
    }
}

void draw_advanced_graphics_slider(ClientHost& host, const RECT& slider,
    int value, int minimum, int maximum, int control_id) {
    const auto scale = ui_scale(host.window);
    const auto s = [scale](int item) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(item) * scale)));
    };
    const int button_width = s(12);
    const int track_left = slider.left + button_width;
    const int track_right = slider.right - button_width;
    const int track_width = std::max(0, track_right - track_left);
    draw_skin(host, host.pressed_advanced_graphics_control == control_id + 10 ? 166 : 165,
        slider.left, slider.top, button_width, s(12));
    draw_skin(host, host.pressed_advanced_graphics_control == control_id + 11 ? 168 : 167,
        slider.right - button_width, slider.top, button_width, s(12));
    const int cap_width = s(6);
    if (track_width > 0) {
        draw_skin(host, 162, track_left, slider.top, cap_width, s(12));
        draw_skin_tiled(host, 161, track_left + cap_width, slider.top,
            std::max(0, track_width - cap_width * 2), s(12));
        draw_skin(host, 164, track_right - cap_width, slider.top, cap_width, s(12));
    }
    const int thumb_width = s(32);
    const int usable = std::max(0, track_width - thumb_width);
    const int clamped = std::clamp(value, minimum, maximum);
    const float ratio = maximum == minimum ? 0.0F :
        static_cast<float>(clamped - minimum) / static_cast<float>(maximum - minimum);
    const int thumb_x = track_left + static_cast<int>(std::lround(usable * ratio));
    draw_skin(host, 163, thumb_x, slider.top, thumb_width, s(12));
}

void draw_advanced_graphics_options_page(ClientHost& host) {
    const auto dialog = advanced_graphics_options_dialog_rect(host.window,
        host.advanced_graphics_options_dialog_offset);
    const auto scale = ui_scale(host.window);
    const auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    draw_original_dialog_frame(host, dialog);
    draw_bitmap_text(host, L"Advanced Graphics Options", dialog.left + s(5),
        dialog.top + s(5), s(tibia::client::advanced_graphics_dialog_width - 10),
        true, 3, 144);
    const int x = dialog.left + s(tibia::client::advanced_graphics_content_x);
    const int y = dialog.top + s(tibia::client::advanced_graphics_content_y);
    draw_bitmap_text(host, L"Select graphics engine:", x,
        y + s(tibia::client::advanced_graphics_engine_label_y),
        s(tibia::client::advanced_graphics_content_width), false, 3, 176);

    constexpr std::array<std::wstring_view, 3> engines{L"DirectX5", L"DirectX9", L"OpenGL"};
    constexpr std::array<int, 3> engine_values{0, 2, 1};
    for (std::size_t index = 0; index < engines.size(); ++index) {
        const auto button = advanced_graphics_engine_button_rect(host.window,
            host.advanced_graphics_options_dialog_offset, index);
        const bool selected = host.advanced_graphics_engine == engine_values[index];
        const bool pressed = host.pressed_advanced_graphics_control >= 0
            ? host.pressed_advanced_graphics_control == static_cast<int>(index)
            : selected;
        draw_skin(host, pressed ? 17 : 16, button.left, button.top, button.right - button.left,
            button.bottom - button.top);
        draw_bitmap_text(host, engines[index], button.left, button.top + s(6),
            button.right - button.left, true, 2);
    }

    wchar_t brightness_label[64]{};
    swprintf_s(brightness_label, L"Set ambient light: %d %%",
        host.advanced_graphics_brightness);
    draw_bitmap_text(host, brightness_label, x,
        y + s(tibia::client::advanced_graphics_ambient_label_y),
        s(tibia::client::advanced_graphics_content_width), false, 3, 176);
    draw_advanced_graphics_slider(host, advanced_graphics_slider_rect(host.window,
        host.advanced_graphics_options_dialog_offset,
        tibia::client::advanced_graphics_ambient_slider_y),
        host.advanced_graphics_brightness, 0, 128, 3);

    constexpr std::array<std::wstring_view, 3> checkbox_labels{
        L"Show Light Effects", L"Don't Stretch/Shrink Game Window",
        L"Windows Mouse Pointer"};
    const std::array<bool, 3> checkbox_values{host.advanced_graphics_light_effects,
        host.advanced_graphics_no_stretching,
        host.advanced_graphics_windows_mouse_pointer};
    constexpr std::array<int, 3> checkbox_y{
        tibia::client::advanced_graphics_light_effects_y,
        tibia::client::advanced_graphics_stretch_y,
        tibia::client::advanced_graphics_mouse_pointer_y};
    for (std::size_t index = 0; index < checkbox_labels.size(); ++index) {
        const int row_y = y + s(checkbox_y[index]);
        draw_checkbox_row_background(host, {x, row_y,
            x + s(tibia::client::advanced_graphics_content_width), row_y + s(22)});
        draw_skin(host, checkbox_values[index] ? 150 : 151,
            x + s(6), row_y + s(5), s(12), s(12));
        draw_bitmap_text(host, checkbox_labels[index], x + s(24),
            centered_bitmap_text_y(host, row_y + s(5), s(12), 3),
            s(tibia::client::advanced_graphics_content_width - 30), false, 3, 176);
    }

    if (host.advanced_graphics_framerate >= tibia::client::advanced_graphics_slider_maximum)
        draw_bitmap_text(host, L"Adjust framerate limit: max", x,
            y + s(tibia::client::advanced_graphics_framerate_label_y),
            s(tibia::client::advanced_graphics_content_width), false, 3, 176);
    else {
        const auto label = L"Adjust framerate limit: " +
            std::to_wstring(host.advanced_graphics_framerate);
        draw_bitmap_text(host, label, x,
            y + s(tibia::client::advanced_graphics_framerate_label_y),
            s(tibia::client::advanced_graphics_content_width), false, 3, 176);
    }
    draw_advanced_graphics_slider(host, advanced_graphics_slider_rect(host.window,
        host.advanced_graphics_options_dialog_offset,
        tibia::client::advanced_graphics_framerate_slider_y),
        host.advanced_graphics_framerate,
        tibia::client::advanced_graphics_slider_minimum,
        tibia::client::advanced_graphics_slider_maximum, 7);
    wchar_t framerate_label[64]{};
    swprintf_s(framerate_label, L"Current framerate: %2.2f", host.measured_framerate);
    draw_bitmap_text(host, framerate_label, x,
        y + s(tibia::client::advanced_graphics_current_framerate_y),
        s(tibia::client::advanced_graphics_content_width), false, 3, 176);

    const int footer_y = dialog.top + s(tibia::client::advanced_graphics_footer_y);
    host.renderer->draw(ui_separator_shadow_texture, dialog.left + s(12), footer_y,
        s(tibia::client::advanced_graphics_dialog_width - 24), s(1));
    host.renderer->draw(ui_separator_highlight_texture, dialog.left + s(12), footer_y + s(1),
        s(tibia::client::advanced_graphics_dialog_width - 24), s(1));
    constexpr std::array<std::wstring_view, 3> labels{L"Help", L"Ok", L"Cancel"};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const auto button = advanced_graphics_button_rect(host.window,
            host.advanced_graphics_options_dialog_offset, index);
        draw_skin(host, host.pressed_advanced_graphics_control ==
            static_cast<int>(index) + 8 ? 13 : 12, button.left, button.top,
            button.right - button.left, button.bottom - button.top);
        draw_bitmap_text(host, labels[index], button.left, button.top + s(6),
            button.right - button.left, true, 2);
    }
}

bool contains_point(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x < rect.right &&
        point.y >= rect.top && point.y < rect.bottom;
}

bool contains_console_transcript_point(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x <= rect.right &&
        point.y >= rect.top && point.y <= rect.bottom;
}

void layout_controls(ClientHost& host);

void show_screen(ClientHost& host, UiScreen screen) {
    host.screen = screen;
    if (screen != UiScreen::game_world) {
        host.console_input_focused = false;
        host.console_input_text.clear();
        host.console_input_caret = 0;
        host.console_input_anchor = 0;
    }
    host.help_open = false;
    host.hints_open = false;
    host.hint_content_open = false;
    host.active_hint = nullptr;
    if (screen != UiScreen::game_world) {
        host.quest_log_open = false;
        host.quest_line_open = false;
    }
    if (screen != UiScreen::game_world) host.options_from_game = false;
    host.pressed_login_button = -1;
    host.login_caret_phase = -1;
    if (host.window) {
        KillTimer(host.window, login_caret_timer_id);
        KillTimer(host.window, game_frame_timer_id);
        if (screen == UiScreen::login)
            SetTimer(host.window, login_caret_timer_id, 100, nullptr);
        else if (screen == UiScreen::game_world)
            SetTimer(host.window, game_frame_timer_id,
                host.framerate_limit >= 201 ? 1U :
                    static_cast<UINT>(std::max(1, 1000 / host.framerate_limit)), nullptr);
    }
    if (screen != UiScreen::main_menu) {
        host.options_open = false;
        host.console_options_open = false;
        host.hotkey_options_open = false;
        host.general_options_open = false;
        host.network_options_open = false;
        host.graphics_options_open = false;
        host.advanced_graphics_options_open = false;
    }
    for (const auto control : host.menu_buttons)
        if (control) ShowWindow(control, SW_HIDE);
    host.pressed_menu_index = -1;
    for (const auto control : {host.account_label, host.account_edit, host.password_label,
        host.password_edit, host.login_button})
        if (control) ShowWindow(control, SW_HIDE);
    for (const auto control : {host.character_heading, host.character_list,
        host.account_status_label, host.connect_button, host.back_button})
        if (control) ShowWindow(control, SW_HIDE);
    if (host.header_label) ShowWindow(host.header_label, SW_HIDE);
    if (host.validation_label) ShowWindow(host.validation_label, SW_HIDE);
    for (const auto control : {host.options_heading, host.engine_label, host.ambient_label,
        host.ambient_slider, host.light_effects_check, host.stretch_check, host.pointer_check,
        host.framerate_label, host.framerate_slider, host.current_framerate_label,
        host.options_help_button, host.options_ok_button, host.options_cancel_button})
        if (control) ShowWindow(control, SW_HIDE);
    for (const auto control : host.engine_radios)
        if (control) ShowWindow(control, SW_HIDE);
    layout_controls(host);
    if (host.window) InvalidateRect(host.window, nullptr, FALSE);
}

void layout_controls(ClientHost& host) {
    if (!host.window) return;
    RECT client{};
    GetClientRect(host.window, &client);
    const float scale = ui_scale(host.window);
    auto s = [scale](int value) {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
    };
    auto move = [](HWND control, int left, int top, int width, int height) {
        if (control) MoveWindow(control, left, top, width, height, TRUE);
    };
    if (host.screen == UiScreen::main_menu) {
        const auto menu = menu_rect(host.window);
        for (std::size_t i = 0; i < host.menu_buttons.size(); ++i) {
            move(host.menu_buttons[i], menu.left + s(15),
                menu.top + s(15) + static_cast<int>(i) * s(29), s(85), s(20));
        }
        if (host.options_open) {
            const auto dialog = options_rect(host.window, host.options_dialog_offset);
            const int x = dialog.left;
            const int y = dialog.top;
            const int w = dialog.right - dialog.left;
            move(host.options_heading, x + s(14), y + s(5), w - s(28), s(20));
            move(host.engine_label, x + s(15), y + s(31), w - s(30), s(18));
            const int radio_y = y + s(49);
            const int radio_w = std::max(1, (w - s(30)) / 3);
            for (std::size_t i = 0; i < host.engine_radios.size(); ++i)
                move(host.engine_radios[i], x + s(14) + static_cast<int>(i) * radio_w,
                    radio_y, radio_w, s(22));
            move(host.ambient_label, x + s(15), y + s(80), w - s(30), s(18));
            move(host.ambient_slider, x + s(15), y + s(100), w - s(30), s(24));
            move(host.light_effects_check, x + s(15), y + s(126), w - s(30), s(22));
            move(host.stretch_check, x + s(15), y + s(157), w - s(30), s(22));
            move(host.pointer_check, x + s(15), y + s(188), w - s(30), s(22));
            move(host.framerate_label, x + s(15), y + s(222), w - s(30), s(18));
            move(host.framerate_slider, x + s(15), y + s(240), w - s(30), s(24));
            move(host.current_framerate_label, x + s(15), y + s(266), w - s(30), s(18));
            const int button_y = y + s(298);
            move(host.options_help_button, x + s(92), button_y, s(50), s(22));
            move(host.options_ok_button, x + s(148), button_y, s(48), s(22));
            move(host.options_cancel_button, x + s(202), button_y, s(48), s(22));
        }
        return;
    }
    const auto panel = panel_rect(host.window);
    const int x = panel.left;
    const int y = panel.top;
    const int w = panel.right - panel.left;
    const int margin = s(28);
    move(host.header_label, x + margin, y + s(18), w - margin * 2, s(30));
    if (host.screen == UiScreen::login) {
        move(host.account_label, x + margin, y + s(91), s(145), s(24));
        move(host.account_edit, x + s(185), y + s(86), w - s(213), s(28));
        move(host.password_label, x + margin, y + s(133), s(145), s(24));
        move(host.password_edit, x + s(185), y + s(128), w - s(213), s(28));
        move(host.login_button, x + w - s(190), y + s(202), s(162), s(34));
        move(host.validation_label, x + margin, y + s(275), w - margin * 2, s(46));
    } else {
        move(host.character_heading, x + margin, y + s(68), w - margin * 2, s(24));
        move(host.character_list, x + margin, y + s(96), w - margin * 2, s(145));
        move(host.account_status_label, x + margin, y + s(252), s(245), s(35));
        move(host.connect_button, x + w - s(205), y + s(250), s(105), s(34));
        move(host.back_button, x + w - s(88), y + s(250), s(60), s(34));
        move(host.validation_label, x + margin, y + s(290), w - margin * 2, s(36));
    }
}

std::wstring account_status_text(std::uint16_t status) {
    if (status == 0) return L"Account Status: Free Account";
    if (status == 0xffff) return L"Account Status: Gratis Premium Account";
    return L"Account Status: Premium Account (" + std::to_wstring(status) +
        (status == 1 ? L" day left)" : L" days left)");
}

void refresh_character_choices(ClientHost& host, int preferred_index = -1) {
    SendMessageW(host.character_list, LB_RESETCONTENT, 0, 0);
    for (const auto& character : host.characters) {
        const auto row = character.name + L" (" + character.world + L")";
        SendMessageW(host.character_list, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(row.c_str()));
    }
    if (host.characters.empty()) {
        SendMessageW(host.character_list, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(L"No characters available."));
        EnableWindow(host.character_list, FALSE);
        EnableWindow(host.connect_button, FALSE);
        host.selected_character = -1;
        return;
    }
    EnableWindow(host.character_list, TRUE);
    const auto index = preferred_index >= 0 &&
        static_cast<std::size_t>(preferred_index) < host.characters.size() ? preferred_index : 0;
    const int visible_rows = std::max(1,
        (ui_units(host.window, 146) - ui_units(host.window, 6)) /
            ui_units(host.window, std::max(1, host.ui_font_heights[3])));
    host.character_list_scroll = std::max(0, index - visible_rows + 1);
    SendMessageW(host.character_list, LB_SETCURSEL, index, 0);
    host.selected_character = index;
    EnableWindow(host.connect_button, TRUE);
}

std::wstring decode_ui_string(const std::string& value) {
    if (value.empty()) return {};
    const auto count = MultiByteToWideChar(CP_ACP, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) return L"?";
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}

std::string encode_ui_string(std::wstring_view value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_ACP, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::optional<std::uint32_t> parse_account_number(HWND edit);

void submit_default_console_input(ClientHost& host) {
    std::wstring next_console_input;
    const auto account_number = parse_account_number(host.account_edit);
    const auto account_text = account_number
        ? std::to_string(*account_number) : std::string{};
    const auto password = encode_ui_string(host.login_password_text);
    std::uint8_t default_speech_mode = 1;
    const auto channel_view = host.communication_session_mailbox
        ? host.communication_session_mailbox->channel_view()
        : tibia::client::CommunicationChannelView{};
    if (!host.report_violation_tab_selected &&
        !channel_view.selected_identifier) {
        constexpr std::array<std::uint8_t, 3> speech_modes{1, 2, 3};
        default_speech_mode = speech_modes[
            host.console_volume_state % speech_modes.size()];
    }
    const auto input = tibia::client::parse_default_console_input(
        encode_ui_string(host.console_input_text), account_text, password,
        default_speech_mode);
    if (input.kind == tibia::client::ConsoleChatInputKind::speech) {
        const auto payload = tibia::network::encode_state6_speech_request(
            input.mode, input.text, input.addressee);
        if (payload && host.send_game_payload) {
            host.send_game_payload(*payload);
            if (input.mode == 4 && !input.addressee.empty()) {
                auto addressee = input.addressee.substr(0, 29);
                next_console_input = decode_ui_string(
                    "*" + addressee + "* ");
            }
        }
    } else if (input.kind == tibia::client::ConsoleChatInputKind::filter_command) {
        const auto available = host.communication_session_mailbox &&
            host.communication_session_mailbox->private_channel_filter_available();
        if (available && host.send_game_payload) {
            std::optional<tibia::network::State6ConsoleFilterOperation> operation;
            switch (input.filter_operation) {
            case tibia::client::ConsoleFilterOperation::invite:
                operation = tibia::network::State6ConsoleFilterOperation::invite;
                break;
            case tibia::client::ConsoleFilterOperation::exclude:
                operation = tibia::network::State6ConsoleFilterOperation::exclude;
                break;
            default:
                break;
            }
            if (operation) {
                const auto payload = tibia::network::encode_state6_console_filter_request(
                    *operation, input.text);
                if (payload) host.send_game_payload(*payload);
            }
        }
    } else if (input.kind == tibia::client::ConsoleChatInputKind::account_data_warning) {
        host.account_data_warning_open = true;
        host.pressed_account_data_warning_button = -1;
        host.account_data_warning_dialog_offset = {};
    } else if (input.kind == tibia::client::ConsoleChatInputKind::requires_channel) {
        const auto channel_id = host.communication_session_mailbox
            ? host.communication_session_mailbox->active_channel() : std::nullopt;
        if (channel_id && host.send_game_payload) {
            const auto payload = tibia::network::encode_state6_speech_request(
                input.mode, input.text, {}, channel_id);
            if (payload) host.send_game_payload(*payload);
        } else if (host.communication_session_mailbox) {
            host.communication_session_mailbox->enqueue_status_message(
                "This is not a chat channel.");
        }
    }
    host.console_input_text = std::move(next_console_input);
    host.console_input_caret = host.console_input_text.size();
    host.console_input_anchor = host.console_input_caret;
}

void commit_hotkey_editor(ClientHost& host) {
    auto& preference = host.hotkey_options_values[host.selected_hotkey];
    preference.text = encode_ui_string(host.hotkey_text);
    while (preference.text.size() > 255 && !host.hotkey_text.empty()) {
        host.hotkey_text.pop_back();
        preference.text = encode_ui_string(host.hotkey_text);
    }
    preference.send_automatically = host.hotkey_send_automatically;
}

void select_hotkey(ClientHost& host, std::size_t index) {
    commit_hotkey_editor(host);
    host.selected_hotkey = std::min(index, tibia::client::hotkey_count - 1);
    if (host.selected_hotkey < host.hotkey_list_scroll)
        host.hotkey_list_scroll = host.selected_hotkey;
    else if (host.selected_hotkey >= host.hotkey_list_scroll +
        tibia::client::hotkey_list_visible_rows)
        host.hotkey_list_scroll = host.selected_hotkey -
            tibia::client::hotkey_list_visible_rows + 1;
    const auto& preference = host.hotkey_options_values[host.selected_hotkey];
    host.hotkey_text = decode_ui_string(preference.text);
    host.hotkey_send_automatically = preference.send_automatically;
}

void upload_hotkey_object_preview(ClientHost& host, std::int32_t object_id) {
    if (!host.renderer || !host.dat || !host.tibia_spr || !host.tibia_spr_bytes ||
        object_id <= 0 || object_id > 65535 || host.hotkey_object_preview_id == object_id)
        return;
    const auto* object = host.dat->find_item(static_cast<std::uint16_t>(object_id));
    if (!object) return;
    try {
        const auto surface = tibia::render::compose_tibia_dat_object(
            *host.tibia_spr_bytes, *host.tibia_spr, *object);
        if (surface.width() == 0 || surface.height() == 0) return;
        tibia::resources::TibiaSprImage image;
        const auto& pixels = surface.pixels();
        for (std::uint32_t y = 0; y < tibia::resources::TibiaSprImage::height; ++y) {
            const auto source_y = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(y) * surface.height() /
                tibia::resources::TibiaSprImage::height);
            for (std::uint32_t x = 0; x < tibia::resources::TibiaSprImage::width; ++x) {
                const auto source_x = static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(x) * surface.width() /
                    tibia::resources::TibiaSprImage::width);
                const auto source = (static_cast<std::size_t>(source_y) * surface.width() +
                    source_x) * 4U;
                const auto destination = (static_cast<std::size_t>(y) *
                    tibia::resources::TibiaSprImage::width + x) * 4U;
                std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(source), 4,
                    image.rgba.begin() + static_cast<std::ptrdiff_t>(destination));
            }
        }
        host.renderer->upload(hotkey_object_preview_texture, image);
        host.hotkey_object_preview_id = object_id;
    } catch (...) {
        host.hotkey_object_preview_id = -1;
    }
}

std::optional<std::uint32_t> parse_account_number(HWND edit) {
    const auto length = GetWindowTextLengthW(edit);
    if (length <= 0) return std::nullopt;
    std::wstring digits(static_cast<std::size_t>(length) + 1U, L'\0');
    GetWindowTextW(edit, digits.data(), length + 1);
    std::uint32_t value{};
    for (int i = 0; i < length; ++i) {
        const auto ch = digits[static_cast<std::size_t>(i)];
        if (ch < L'0' || ch > L'9') return std::nullopt;
        const auto digit = static_cast<std::uint32_t>(ch - L'0');
        if (value > (UINT32_MAX - digit) / 10U) return std::nullopt;
        value = value * 10U + digit;
    }
    return value;
}

std::string read_password_ansi(HWND edit) {
    const auto length = GetWindowTextLengthW(edit);
    if (length <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(length) + 1U, L'\0');
    GetWindowTextW(edit, wide.data(), length + 1);
    const auto byte_count = WideCharToMultiByte(CP_ACP, 0, wide.data(), length,
        nullptr, 0, nullptr, nullptr);
    if (byte_count <= 0) throw std::runtime_error("password cannot be converted to the client code page");
    std::string password(static_cast<std::size_t>(byte_count), '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.data(), length, password.data(), byte_count, nullptr, nullptr);
    return password;
}

void fill_system_random(std::span<std::uint8_t> output) {
    const auto status = BCryptGenRandom(nullptr, output.data(), static_cast<ULONG>(output.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0) throw std::runtime_error("BCryptGenRandom failed while preparing login");
}

tibia::network::PreparedLogin prepare_login(ClientHost& host, HWND password_edit, std::uint32_t account) {
    tibia::crypto::XteaKeyBytes key{};
    std::array<std::uint8_t, 128> random_block{};
    fill_system_random(key);
    fill_system_random(random_block);
    const auto password = read_password_ansi(password_edit);
    const tibia::network::LoginServerRequestFields fields{
        host.dat->header.signature,
        host.tibia_spr->signature,
        host.tibia_pic->signature};
    return tibia::network::prepare_login_server_session(
        key, random_block, fields, account, password);
}

tibia::network::PreparedLogin prepare_game_login(ClientHost& host, HWND password_edit,
    std::uint32_t account, const CharacterChoice& character) {
    tibia::crypto::XteaKeyBytes key{};
    std::array<std::uint8_t, 128> random_block{};
    fill_system_random(key);
    fill_system_random(random_block);
    const auto password = read_password_ansi(password_edit);
    return tibia::network::prepare_game_server_session(key, random_block,
        host.test_server_login, account, character.wire_name, password);
}

void apply_character_list_response(ClientHost& host,
    const tibia::network::CharacterListResponse& response) {
    const auto preferred = host.selected_character;
    host.account_status = response.account_status;
    host.characters.clear();
    host.characters.reserve(response.characters.size());
    for (const auto& entry : response.characters) {
        host.characters.push_back({decode_ui_string(entry.name), decode_ui_string(entry.world),
            entry.name, entry.server_ipv4, entry.server_port});
    }
    refresh_character_choices(host, preferred);
    if (host.account_status_label) {
        const auto text = account_status_text(host.account_status);
        SetWindowTextW(host.account_status_label, text.c_str());
    }
    show_screen(host, UiScreen::character_selection);
    if (host.validation_label) {
        const auto message = L"Character list loaded: " + std::to_wstring(host.characters.size()) + L" entries.";
        SetWindowTextW(host.validation_label, message.c_str());
    }
}

void publish_login_event(const std::shared_ptr<LoginMailbox>& mailbox, HWND window,
    LoginUiEvent event) {
    if (event.game_session && !event.finished) return;
    {
        const std::lock_guard lock(mailbox->mutex);
        mailbox->pending = std::move(event);
    }
    PostMessageW(window, login_result_message, 0, 0);
}

std::string utf8_from_wide(const std::wstring& value) {
    const auto count = WideCharToMultiByte(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) throw std::runtime_error("login server name cannot be encoded as UTF-8");
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), count, nullptr, nullptr);
    return result;
}

std::wstring error_to_wide(const char* error) {
    return std::wstring(error, error + std::char_traits<char>::length(error));
}

void start_login_transport(ClientHost& host, HWND window) {
    if (!host.prepared_login || host.initial_wire_request.empty()) {
        throw std::logic_error("login request must be prepared first");
    }
    if (host.login_thread.joinable()) {
        host.login_thread.request_stop();
        if (host.login_connection) host.login_connection->close();
        host.login_thread.join();
    }

    const auto server = utf8_from_wide(host.login_server);
    const auto service = std::to_string(host.login_port);
    const auto request = host.initial_wire_request;
    const auto prepared = *host.prepared_login;
    auto* status_line_state = &host.status_line_state;
    const auto mailbox = host.login_mailbox;
    {
        const std::lock_guard lock(mailbox->mutex);
        mailbox->pending.reset();
    }
    host.login_connection = std::make_shared<tibia::network::TcpConnection>();
    const auto connection = host.login_connection;
    EnableWindow(host.login_button, FALSE);
    host.connection_status_text = L"Connecting to the login server...";
    InvalidateRect(window, nullptr, FALSE);

    host.login_thread = std::jthread([connection, mailbox, window, server, service,
        request, prepared, status_line_state](std::stop_token stop) {
        bool completed = false;
        try {
            connection->connect(server, service);
            if (stop.stop_requested()) return;
            connection->send_all(request);

            tibia::network::MessageDispatcher dispatcher;
            tibia::network::register_observed_character_list_handler(dispatcher,
                [mailbox, window, &completed](const tibia::network::CharacterListResponse& response) {
                    publish_login_event(mailbox, window,
                        LoginUiEvent{response, L"Character list received.", true});
                    completed = true;
                });
            tibia::network::register_observed_state4_notice_handlers(dispatcher,
                [mailbox, window, &completed](const tibia::network::State4Notice& notice) {
                    auto title = std::wstring(notice.title.begin(), notice.title.end());
                    const auto message = decode_ui_string(notice.message);
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt, message, true, false, false,
                            std::move(title)});
                    completed = true;
                });
            tibia::network::register_observed_state4_text_handler(dispatcher, *status_line_state,
                [mailbox, window](std::string_view text) {
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt, decode_ui_string(std::string(text)),
                            false, false, true});
                });
            tibia::network::register_observed_state4_opcode_28_handler(dispatcher,
                [mailbox, window, connection, &completed] {
                    completed = true;
                    connection->close();
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt, L"", true});
                });

            tibia::network::EncryptedFrameStream stream(prepared.xtea_key);
            std::array<std::uint8_t, 4096> buffer{};
            const auto response_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            while (!stop.stop_requested() && !completed) {
                if (std::chrono::steady_clock::now() >= response_deadline) {
                    throw std::runtime_error("timed out waiting for the login-server response");
                }
                if (!connection->wait_for_data(std::chrono::milliseconds(250))) continue;
                const auto received = connection->receive_some(buffer);
                if (received == 0) {
                    throw std::runtime_error("login server closed the connection before a character list arrived");
                }
                stream.append(std::span<const std::uint8_t>(buffer.data(), received));
                while (auto frame = stream.next()) {
                    if (frame->logical_size < 5 || frame->logical_size > frame->bytes.size()) {
                        throw std::runtime_error("login server returned an invalid decrypted message size");
                    }
                    const auto payload = std::span<const std::uint8_t>(frame->bytes.data() + 4,
                        frame->logical_size - 4);
                    std::size_t offset = 0;
                    while (offset < payload.size()) {
                        const auto opcode = payload[offset];
                        const auto message = payload.subspan(offset + 1);
                        const auto result = dispatcher.dispatch(
                            tibia::network::DispatchPhase::state4, opcode, message);
                        if (result.status == tibia::network::DispatchStatus::unknown_opcode) {
                            throw std::runtime_error("login server sent an unhandled opcode");
                        }
                        if (result.unconsumed_bytes > message.size()) {
                            throw std::runtime_error("login server handler returned an invalid message offset");
                        }
                        offset += message.size() - result.unconsumed_bytes + 1;
                        if (completed || stop.stop_requested()) break;
                    }
                }
            }
        } catch (const std::exception& error) {
            if (!stop.stop_requested()) {
                publish_login_event(mailbox, window,
                    LoginUiEvent{std::nullopt, error_to_wide(error.what()), true});
            }
        }
        connection->close();
    });
}

void start_game_transport(ClientHost& host, HWND window, const CharacterChoice& character,
    tibia::network::PreparedLogin prepared) {
    if (host.login_thread.joinable()) {
        host.login_thread.request_stop();
        if (host.login_connection) host.login_connection->close();
        host.login_thread.join();
    }
    const auto server = std::to_string(character.server_ipv4[0]) + "." +
        std::to_string(character.server_ipv4[1]) + "." +
        std::to_string(character.server_ipv4[2]) + "." +
        std::to_string(character.server_ipv4[3]);
    const auto service = std::to_string(character.server_port);
    const auto wire_request = tibia::network::frame_initial_login_request(prepared.request);
    const auto mailbox = host.login_mailbox;
    const auto game_world_mailbox = host.game_world_mailbox;
    const auto dat = host.dat;
    game_world_mailbox->clear();
    host.battle_targets = {};
    host.battle_context_menu.reset();
    host.pressed_battle_context_menu_action = -1;
    host.vip_context_menu.reset();
    host.pressed_vip_context_menu_action = -1;
    host.vip_context_player_name.clear();
    host.pressed_battle_creature_id = 0;
    const auto map_extra_ids = host.map_item_ids_with_extra_byte;
    const auto communication_preferences = tibia::client::read_communication_preferences(
        host.communication_config_path);
    const auto communication_session_mailbox = host.communication_session_mailbox;
    communication_session_mailbox->publish_ignore_list(communication_preferences.ignore_list);
    communication_session_mailbox->publish_private_channel_filter_state(false);
    communication_session_mailbox->publish_active_channel(std::nullopt);
    host.console_timestamps_enabled = communication_preferences.display.timestamps_enabled;
    host.console_levels_enabled = communication_preferences.display.levels_enabled;
    const auto visual_update_rate = host.framerate_limit;
    {
        const std::lock_guard lock(mailbox->mutex);
        mailbox->pending.reset();
    }
    host.login_connection = std::make_shared<tibia::network::TcpConnection>();
    const auto connection = host.login_connection;
    const auto send_mutex = std::make_shared<std::mutex>();
    host.report_violation_buffer =
        std::make_shared<tibia::client::ReportViolationBuffer>();
    const auto report_violation_buffer = host.report_violation_buffer;
    const auto player_level = std::make_shared<std::atomic_uint32_t>(0);
    host.stop_action_reset_pending = std::make_shared<std::atomic_bool>(false);
    const auto stop_action_reset_pending = host.stop_action_reset_pending;
    host.send_game_command = [connection, key = prepared.xtea_key, send_mutex](
        std::uint8_t opcode) {
        const std::array<std::uint8_t, 1> payload{opcode};
        std::array<std::uint8_t, 5> padding{};
        fill_system_random(padding);
        const auto frame = tibia::network::encode_encrypted_frame(payload, padding, key);
        const std::lock_guard lock(*send_mutex);
        connection->send_all(frame);
    };
    host.send_game_payload = [connection, key = prepared.xtea_key, send_mutex](
        std::span<const std::uint8_t> payload) {
        const auto encrypted_size = (payload.size() + 2U + 7U) & ~std::size_t{7U};
        std::vector<std::uint8_t> padding(encrypted_size - payload.size() - 2U);
        fill_system_random(padding);
        const auto frame = tibia::network::encode_encrypted_frame(payload, padding, key);
        const std::lock_guard lock(*send_mutex);
        connection->send_all(frame);
    };
    host.request_quest_flags = [connection, key = prepared.xtea_key, send_mutex](
        std::uint16_t quest_id) {
        const std::array<std::uint8_t, 3> payload{
            0xf1, static_cast<std::uint8_t>(quest_id & 0xffU),
            static_cast<std::uint8_t>(quest_id >> 8U)};
        std::array<std::uint8_t, 3> padding{};
        fill_system_random(padding);
        const auto frame = tibia::network::encode_encrypted_frame(payload, padding, key);
        const std::lock_guard lock(*send_mutex);
        connection->send_all(frame);
    };
    host.request_tutor_help = [connection, key = prepared.xtea_key, send_mutex] {
        const std::array<std::uint8_t, 3> payload{0x98, 0x08, 0x00};
        std::array<std::uint8_t, 3> padding{};
        fill_system_random(padding);
        const auto frame = tibia::network::encode_encrypted_frame(payload, padding, key);
        const std::lock_guard lock(*send_mutex);
        connection->send_all(frame);
    };
    host.submit_rule_violation = [connection, key = prepared.xtea_key, send_mutex,
        report_violation_buffer, player_level, player_name = character.wire_name](std::wstring_view text) {
        const auto question = encode_ui_string(text);
        report_violation_buffer->open(question, player_name,
            player_level->load(std::memory_order_relaxed));
        tibia::network::PacketWriter writer;
        writer.write_u8(0x96);
        writer.write_u8(6);
        writer.write_string(question);
        const auto payload = std::move(writer).take();
        const auto encrypted_size = (payload.size() + 2U + 7U) & ~std::size_t{7U};
        std::vector<std::uint8_t> padding(encrypted_size - payload.size() - 2U);
        fill_system_random(padding);
        const auto frame = tibia::network::encode_encrypted_frame(payload, padding, key);
        const std::lock_guard lock(*send_mutex);
        connection->send_all(frame);
    };
    host.send_report_violation_reply = [connection, key = prepared.xtea_key,
        send_mutex](std::wstring_view text) {
        const auto answer = encode_ui_string(text);
        if (answer.empty()) return;
        tibia::network::PacketWriter writer;
        writer.write_u8(0x96);
        writer.write_u8(8);
        writer.write_string(answer);
        const auto payload = std::move(writer).take();
        const auto encrypted_size = (payload.size() + 2U + 7U) & ~std::size_t{7U};
        std::vector<std::uint8_t> padding(encrypted_size - payload.size() - 2U);
        fill_system_random(padding);
        const auto frame = tibia::network::encode_encrypted_frame(payload, padding, key);
        const std::lock_guard lock(*send_mutex);
        connection->send_all(frame);
    };
    EnableWindow(host.connect_button, FALSE);
    host.connection_status_text = L"Connecting to the game world. Please wait.";
    InvalidateRect(window, nullptr, FALSE);

    host.login_thread = std::jthread([connection, mailbox, game_world_mailbox, dat, window, server, service, map_extra_ids,
        wire_request, communication_preferences, visual_update_rate, send_mutex,
        report_violation_buffer, player_level, communication_session_mailbox,
        stop_action_reset_pending,
        prepared = std::move(prepared)](std::stop_token stop) {
        bool completed = false;
        try {
            connection->connect(server, service);
            if (stop.stop_requested()) return;
            connection->send_all(wire_request);

            tibia::network::MessageDispatcher dispatcher;
            std::uint8_t current_map_floor = 0;
            std::uint32_t local_player_id = 0;
            std::array<std::uint8_t, 32> server_right_values{};
            bool server_right_values_received = false;
            std::optional<tibia::network::State6DecodedMap> current_map;
            tibia::network::State6DecodedMap initial_map_position_context;
            std::vector<tibia::network::State6Container> containers;
            tibia::network::State6ShopState shop_state;
            std::vector<tibia::network::State6InventorySlot> inventory;
            tibia::network::State6Opcode82State opcode_82_state;
            tibia::network::State6VisualEffectPool visual_effects;
            tibia::network::State6CreatureStateStore creature_states;
            tibia::network::State6LocalPlayerMovementGlobals player_movement_globals;
            tibia::network::State6SessionProjection session_projection;
            tibia::network::State6CommunicationEffectQueue communication_effects;
            tibia::client::TalkSystem talk_system;
            tibia::client::CommunicationSession communication_session;
            communication_session.settings() = communication_preferences.routing;
            communication_session.display_preferences() = communication_preferences.display;
            communication_session.replace_talkbox_filters(
                communication_preferences.white_list, communication_preferences.ignore_list);
            tibia::network::register_observed_ping_reply_handler(dispatcher,
                [connection, key = prepared.xtea_key, send_mutex](
                    std::span<const std::uint8_t> payload) {
                    const auto encrypted_size = (payload.size() + 2U + 7U) & ~std::size_t{7U};
                    std::vector<std::uint8_t> padding(encrypted_size - payload.size() - 2U);
                    fill_system_random(padding);
                    const auto reply = tibia::network::encode_encrypted_frame(payload, padding, key);
                    const std::lock_guard lock(*send_mutex);
                    connection->send_all(reply);
                });
            tibia::network::register_observed_state6_byte_block_handler(dispatcher,
                [&server_right_values, &server_right_values_received](
                    const std::array<std::uint8_t, 32>& values) {
                    server_right_values = values;
                    server_right_values_received = true;
                });
            tibia::network::register_observed_state6_game_start_handler(dispatcher,
                [mailbox, window, &local_player_id, &player_movement_globals](
                    const tibia::network::State6GameStart& start) {
                    local_player_id = start.local_player_id;
                    player_movement_globals.at_0x00605A24 = start.movement_tick_quantum;
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Game login accepted; local creature ID " +
                                std::to_wstring(start.local_player_id) + L"; movement tick quantum: " +
                                std::to_wstring(start.movement_tick_quantum) + L", " +
                                std::to_wstring(start.status_slot_0x28) +
                                L". Waiting for the initial map description.",
                            false, true});
                });
            tibia::network::register_observed_state6_map_description_handler(dispatcher,
                [mailbox, window, extra_ids = map_extra_ids, dat, &current_map_floor, &local_player_id, &current_map,
                    &creature_states](
                    const tibia::network::State6MapDescription& map) {
                    auto decoded = tibia::network::decode_state6_map_description(map, extra_ids, *dat);
                    decoded.local_player_id = local_player_id;
                    current_map = decoded;
                    creature_states.observe_map(decoded, true);
                    std::size_t object_count = 0;
                    for (const auto& tile : decoded.tiles) object_count += tile.objects.size();
                    current_map_floor = map.floor;
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Map description received at " + std::to_wstring(map.center_x) +
                                L"," + std::to_wstring(map.center_y) + L" floor " +
                                std::to_wstring(map.floor) + L": decoded " +
                                std::to_wstring(decoded.tiles.size()) + L" tiles and " +
                                std::to_wstring(object_count) + L" objects from " +
                                std::to_wstring(map.tile_stream.size()) + L" bytes.",
                            false, true});
                    return map.tile_stream.size() - decoded.unparsed_tail.size();
                });
            tibia::network::register_observed_state6_map_strip_handlers(dispatcher,
                [mailbox, window, extra_ids = map_extra_ids, dat, &current_map_floor, &current_map,
                    &creature_states](
                    const tibia::network::State6MapStrip& raw_strip) {
                    const auto strip = tibia::network::decode_state6_map_strip(
                        raw_strip.direction, current_map_floor, raw_strip.raw_stream, extra_ids,
                        current_map ? &*current_map : nullptr);
                    if (current_map) tibia::network::apply_state6_map_strip(*current_map, strip, *dat);
                    if (current_map) creature_states.observe_map(*current_map, false);
                    std::size_t objects = 0;
                    for (const auto& tile : strip.tiles) objects += tile.objects.size();
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Map edge update decoded: " + std::to_wstring(strip.tiles.size()) +
                                L" tiles, " + std::to_wstring(objects) + L" objects.",
                            false, true});
                    return strip.wire_bytes_consumed;
                });
            tibia::network::register_observed_state6_floor_transition_handlers(dispatcher,
                [mailbox, window, extra_ids = map_extra_ids, dat, &current_map,
                    &current_map_floor, &creature_states](
                    const tibia::network::State6FloorTransitionPacket& transition) {
                    if (!current_map)
                        throw std::runtime_error("state-6 floor transition arrived before the map description");
                    const auto result = tibia::network::apply_state6_floor_transition(
                        *current_map, transition, extra_ids, *dat);
                    current_map_floor = result.center_floor;
                    creature_states.observe_map(*current_map, false);
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Floor transition " +
                                std::wstring(transition.direction == tibia::network::State6FloorDirection::up
                                    ? L"up" : L"down") + L" to " +
                                std::to_wstring(result.center_x) + L"," +
                                std::to_wstring(result.center_y) + L" floor " +
                                std::to_wstring(result.center_floor) + L"; refreshed " +
                                std::to_wstring(result.decoded_tiles) + L" map tiles.",
                            false, true});
                    return result.wire_bytes_consumed;
                });
            tibia::network::register_observed_state6_map_tile_update_handler(dispatcher,
                [mailbox, window, &current_map, &creature_states, dat](
                    const tibia::network::State6MapTileUpdate& update) {
                    if (current_map)
                        tibia::network::apply_state6_map_tile_update(*current_map, update, *dat);
                    if (current_map) creature_states.observe_map(*current_map, false);
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Map tile updated at " + std::to_wstring(update.x) + L"," +
                                std::to_wstring(update.y) + L" floor " +
                                std::to_wstring(update.floor) + L" (" +
                                std::to_wstring(update.tile.objects.size()) + L" objects).",
                            false, true});
                }, map_extra_ids, [&current_map]() -> const tibia::network::State6DecodedMap* {
                    return current_map ? &*current_map : nullptr;
                });
            tibia::network::register_observed_state6_map_thing_add_handler(dispatcher,
                [mailbox, window, &current_map, &creature_states,
                    &player_movement_globals, dat](
                    const tibia::network::State6MapThingAdd& add) {
                    if (current_map)
                        tibia::network::apply_state6_map_thing_add(*current_map, *dat, add);
                    if (current_map) creature_states.observe_map(*current_map, false);
                    if (current_map && current_map->local_player_id != 0 &&
                        add.object.creature_id == current_map->local_player_id) {
                        (void)tibia::network::apply_observed_state6_local_player_reset(
                            player_movement_globals, creature_states, current_map->local_player_id);
                    }
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Object " + std::to_wstring(add.object.type) + L" added at " +
                                std::to_wstring(add.x) + L"," + std::to_wstring(add.y) +
                                L" floor " + std::to_wstring(add.floor) + L".",
                            false, true});
                }, map_extra_ids, [&current_map]() -> const tibia::network::State6DecodedMap* {
                    return current_map ? &*current_map : nullptr;
                });
            tibia::network::register_observed_state6_map_thing_transform_handler(dispatcher,
                [mailbox, window, &current_map, &creature_states, dat](
                    const tibia::network::State6MapThingTransform& transform) {
                    if (current_map) {
                        const auto tile = std::find_if(current_map->tiles.begin(),
                            current_map->tiles.end(), [&transform](const auto& entry) {
                                return entry.x == transform.x && entry.y == transform.y &&
                                    entry.floor == transform.floor;
                            });
                        if (tile != current_map->tiles.end() &&
                            transform.stack_index < tile->objects.size()) {
                            const auto creature_id =
                                tile->objects[transform.stack_index].creature_id;
                            if (creature_id != 0)
                                creature_states.set_map_presence(creature_id, false);
                        }
                        tibia::network::apply_state6_map_thing_transform(*current_map, transform, *dat);
                        creature_states.observe_map(*current_map, false);
                    }
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Object at " + std::to_wstring(transform.x) + L"," +
                                std::to_wstring(transform.y) + L" floor " +
                                std::to_wstring(transform.floor) + L" transformed at stack " +
                                std::to_wstring(transform.stack_index) + L".",
                            false, true});
                }, map_extra_ids, [&current_map]() -> const tibia::network::State6DecodedMap* {
                    return current_map ? &*current_map : nullptr;
                });
            tibia::network::register_observed_state6_map_thing_remove_handler(dispatcher,
                [mailbox, window, &current_map, &creature_states, dat](
                    const tibia::network::State6MapThingRemove& remove) {
                    if (current_map) {
                        const auto tile = std::find_if(current_map->tiles.begin(), current_map->tiles.end(),
                            [&remove](const auto& entry) {
                                return entry.x == remove.x && entry.y == remove.y && entry.floor == remove.floor;
                            });
                        if (tile != current_map->tiles.end() && remove.stack_index < tile->objects.size()) {
                            const auto creature_id = tile->objects[remove.stack_index].creature_id;
                            if (creature_id != 0)
                                creature_states.set_map_presence(creature_id, false);
                        }
                        tibia::network::apply_state6_map_thing_remove(*current_map, remove, *dat);
                    }
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Object removed at " + std::to_wstring(remove.x) + L"," +
                                std::to_wstring(remove.y) + L" floor " +
                                std::to_wstring(remove.floor) + L", stack " +
                                std::to_wstring(remove.stack_index) + L".",
                            false, true});
                });
            tibia::network::register_observed_state6_thing_move_handler(dispatcher,
                [mailbox, window, &current_map, &player_movement_globals,
                    &creature_states, dat](
                    const tibia::network::State6ThingMove& move) {
                    if (current_map) {
                        const auto first_timer_sample = static_cast<std::uint32_t>(
                            timeGetTime() - animation_epoch_tick);
                        const auto second_timer_sample = static_cast<std::uint32_t>(
                            timeGetTime() - animation_epoch_tick);
                        (void)tibia::network::apply_observed_state6_thing_move(
                            *current_map, creature_states, player_movement_globals, *dat,
                            first_timer_sample, second_timer_sample, move);
                    }
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Thing moved from " + std::to_wstring(move.old_x) + L"," +
                                std::to_wstring(move.old_y) + L" to " +
                                std::to_wstring(move.new_x) + L"," +
                                std::to_wstring(move.new_y) + L".", false, true});
                });
            tibia::network::register_observed_state6_container_handlers(dispatcher,
                [&containers](const tibia::network::State6Container& container) {
                    tibia::network::apply_state6_container_open(containers, container);
                },
                [&containers](std::uint8_t id) {
                    tibia::network::apply_state6_container_close(containers, id);
                },
                [&containers](const tibia::network::State6ContainerChange& change) {
                    tibia::network::apply_state6_container_change(containers, change);
                }, map_extra_ids);
            tibia::network::register_observed_state6_shop_handlers(dispatcher,
                [mailbox, window, &shop_state](const tibia::network::State6ShopUpdate& update) {
                    (void)shop_state.apply(update);
                    if (update.kind == tibia::network::State6ShopUpdateKind::open) {
                        publish_login_event(mailbox, window,
                            LoginUiEvent{std::nullopt,
                                L"Shop opened: " + std::wstring(update.title.begin(), update.title.end()) +
                                    L" (" + std::to_wstring(update.items.size()) + L" offers).",
                                false, true});
                    } else if (update.kind == tibia::network::State6ShopUpdateKind::close) {
                        publish_login_event(mailbox, window,
                            LoginUiEvent{std::nullopt, L"Shop closed.", false, true});
                    }
                }, map_extra_ids);
            tibia::network::register_observed_state6_inventory_handlers(dispatcher,
                [&inventory](const tibia::network::State6InventoryUpdate& update) {
                    tibia::network::apply_state6_inventory_update(inventory, update);
                }, map_extra_ids);
            tibia::network::register_observed_state6_notice_handlers(dispatcher,
                [mailbox, window, connection, &completed](const tibia::network::State6Notice& notice) {
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            decode_ui_string(notice.message), notice.performs_followup_cleanup,
                            false, false,
                            std::wstring(notice.title.begin(), notice.title.end())});
                    if (notice.performs_followup_cleanup) {
                        completed = true;
                        connection->close();
                    }
                });
            tibia::network::register_observed_state6_opcode_16_handler(dispatcher,
                [mailbox, window](const tibia::network::State6Opcode16Message& message) {
                    publish_login_event(mailbox, window,
                        LoginUiEvent{std::nullopt,
                            L"Game message: " + std::wstring(message.text.begin(), message.text.end()) +
                                L" [" + std::to_wstring(message.value) + L"].", false, true});
                });
            tibia::network::register_observed_state6_opcode_82_handler(dispatcher,
                [&opcode_82_state](const tibia::network::State6Opcode82Values& values) {
                    (void)tibia::network::apply_observed_state6_opcode_82(opcode_82_state, values);
                });
            tibia::network::State6VisualUpdates visual_updates;
            visual_updates.transform_position = [&current_map, &initial_map_position_context](
                const tibia::network::State6WirePosition& position) {
                return tibia::network::transform_state6_wire_position(
                    current_map ? *current_map : initial_map_position_context, position);
            };
            visual_updates.position_effect = [&visual_effects](
                const tibia::network::State6PositionEffect& effect) {
                (void)visual_effects.add(effect);
            };
            visual_updates.text_effect = [&visual_effects](
                const tibia::network::State6TextEffect& effect) {
                (void)visual_effects.add(effect);
            };
            visual_updates.projectile_effect = [&visual_effects](
                const tibia::network::State6ProjectileEffect& effect) {
                (void)visual_effects.add(effect, ::GetTickCount());
            };
            visual_updates.creature_value = [&creature_states](
                const tibia::network::State6CreatureValue& update) {
                (void)creature_states.apply(update, ::GetTickCount());
            };
            tibia::network::register_observed_state6_visual_update_handlers(
                dispatcher, std::move(visual_updates));
            tibia::network::register_observed_state6_creature_update_handlers(dispatcher,
                [&creature_states](const tibia::network::State6CreatureUpdate& update) {
                    (void)creature_states.apply(update,
                        static_cast<std::int32_t>(std::time(nullptr)));
                });
            tibia::network::register_observed_state6_notification_handlers(dispatcher,
                [&session_projection, player_level](const tibia::network::State6OpaqueNotification& update) {
                    session_projection.apply(update);
                    if (update.opcode == 0xA0 && update.value_count > 4)
                        player_level->store(update.values[4], std::memory_order_relaxed);
                });
            tibia::network::register_observed_state6_communication_handlers(dispatcher,
                {
                    [&talk_system, &communication_session, report_violation_buffer](
                        const tibia::network::State6SpeechMessage& message) {
                        if (message.mode == 8) {
                            report_violation_buffer->append_speech_reply(
                                message.speaker_id, message.speaker_name, message.level,
                                message.text, std::time(nullptr));
                            return;
                        }
                        if (message.mode == 7) return;
                        if (communication_session.should_display_talkbox(message))
                            (void)talk_system.apply(message);
                        (void)communication_session.apply_speech(message, std::time(nullptr));
                    },
                    [&communication_effects, &communication_session,
                        communication_session_mailbox, window](const auto& channels) {
                        communication_effects.enqueue_channel_list(channels);
                        communication_session.replace_available_channels(channels);
                        communication_session_mailbox->publish_available_channels(channels);
                        PostMessageW(window, channel_list_message, 0, 0);
                    },
                    [&communication_effects, &communication_session,
                        communication_session_mailbox](const tibia::network::State6ChannelOpen& channel) {
                        communication_effects.enqueue_channel_open(channel);
                        communication_session.open_channel(channel);
                        communication_session_mailbox->publish_active_channel(
                            communication_session.channel_buffer_identifier());
                    }
                });
            tibia::network::register_observed_state6_communication_control_handlers(dispatcher,
                [&communication_effects, &talk_system, &communication_session,
                    communication_session_mailbox, report_violation_buffer](
                    const tibia::network::State6CommunicationControl& update) {
                    communication_effects.enqueue_control(update);
                    (void)talk_system.apply(update);
                    (void)communication_session.apply_control(update, std::time(nullptr));
                    if (update.opcode == 0xB1)
                        report_violation_buffer->mark_server_closed();
                    if (update.opcode == 0xB3)
                        (void)communication_session.close_channel(update.field_u16,
                            std::time(nullptr));
                    communication_session_mailbox->publish_active_channel(
                        communication_session.channel_buffer_identifier());
                    communication_session_mailbox->publish_private_channel_filter_state(
                        communication_session.has_active_private_channel_filter());
                });
            tibia::network::register_observed_state6_opcode_b5_handler(dispatcher,
                [&current_map, &creature_states, &player_movement_globals](std::uint8_t key_state) {
                    if (!current_map || current_map->local_player_id == 0) return;
                    (void)tibia::network::apply_observed_state6_opcode_b5_input(
                        player_movement_globals, creature_states, current_map->local_player_id,
                        key_state);
                });
            tibia::network::register_observed_state6_remaining_record_handlers(dispatcher,
                [&session_projection](const tibia::network::State6RecoveredRecord& record) {
                    session_projection.apply(record);
                });

            tibia::network::EncryptedFrameStream stream(prepared.xtea_key);
            std::array<std::uint8_t, 4096> buffer{};
            auto response_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            bool received_initial_response = false;
            auto talk_update_time = std::chrono::steady_clock::now();
            constexpr auto status_update_interval = std::chrono::milliseconds(100);
            auto status_update_time = talk_update_time + status_update_interval;
            std::uint64_t communication_ignore_revision{};
            std::vector<std::string> active_ignore_list;
            const auto frame_period = visual_update_rate >= 201
                ? std::chrono::steady_clock::duration::zero()
                : std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(1.0 / std::max(10, visual_update_rate)));
            auto next_visual_update = talk_update_time;
            bool world_transition_posted = false;
            const auto publish_world_snapshot = [&] {
                tibia::client::CommunicationChannelView channel_view;
                channel_view.channels = communication_session.open_channels();
                channel_view.channel_message_counts.reserve(channel_view.channels.size());
                for (const auto& channel : channel_view.channels) {
                    const auto* messages = communication_session.channel_messages(channel.id);
                    channel_view.channel_message_counts.push_back(messages
                        ? messages->lines().size() : 0);
                }
                channel_view.selected_identifier =
                    communication_session.channel_buffer_identifier();
                channel_view.server_messages_enabled =
                    communication_session.server_messages_enabled(
                        channel_view.selected_identifier);
                const auto& selected_messages = communication_session.channel_messages().lines();
                channel_view.messages.assign(selected_messages.begin(), selected_messages.end());
                communication_session_mailbox->publish_channel_view(std::move(channel_view));
                if (dat)
                    game_world_mailbox->publish(current_map, creature_states,
                        player_movement_globals, talk_system,
                        communication_session.primary_messages(), *dat, visual_effects,
                        inventory,
                        containers,
                        session_projection.buddies(), session_projection.quests(), server_right_values,
                        server_right_values_received, opcode_82_state,
                        session_projection.native_status());
                if (current_map && !world_transition_posted) {
                    const auto snapshot = game_world_mailbox->read();
                    if (snapshot && tibia::client::resolve_map_window_camera_offset(*snapshot)) {
                        PostMessageW(window, game_world_message, 0, 0);
                        world_transition_posted = true;
                    }
                }
            };
            while (!stop.stop_requested() && !completed) {
                const auto now = std::chrono::steady_clock::now();
                if (communication_session_mailbox->read_ignore_list_if_changed(
                        communication_ignore_revision, active_ignore_list)) {
                    communication_session.replace_talkbox_filters(
                        communication_preferences.white_list, active_ignore_list);
                }
                if (const auto requested =
                        communication_session_mailbox->take_channel_selection()) {
                    if (*requested) {
                        if (!communication_session.select_channel(**requested)) continue;
                    } else {
                        communication_session.select_default_channel();
                    }
                    {
                        communication_session_mailbox->publish_active_channel(*requested);
                        publish_world_snapshot();
                        PostMessageW(window, game_world_message, 0, 0);
                    }
                }
                if (const auto identifier =
                        communication_session_mailbox->take_channel_close()) {
                    (void)communication_session.close_channel(*identifier,
                        std::time(nullptr));
                    communication_session_mailbox->publish_active_channel(
                        communication_session.channel_buffer_identifier());
                    communication_session_mailbox->publish_private_channel_filter_state(
                        communication_session.has_active_private_channel_filter());
                    publish_world_snapshot();
                    PostMessageW(window, game_world_message, 0, 0);
                }
                if (const auto target = communication_session_mailbox->
                        take_server_messages_toggle()) {
                    (void)communication_session.toggle_server_messages(*target);
                    publish_world_snapshot();
                    PostMessageW(window, game_world_message, 0, 0);
                }
                for (const auto& message : communication_session_mailbox->take_local_messages())
                    (void)communication_session.apply_local_message(
                        message.mode, message.text, std::time(nullptr));
                const auto status_messages =
                    communication_session_mailbox->take_status_messages();
                for (const auto& message : status_messages)
                    talk_system.set_status_message(message.text, message.persistent);
                if (stop_action_reset_pending->exchange(false)) {
                    player_movement_globals.at_0x00605A14 = 0;
                    player_movement_globals.at_0x006059DC = 0;
                    player_movement_globals.at_0x006059D8 = 0;
                    player_movement_globals.at_0x006059D4 = 0;
                    player_movement_globals.at_0x0060599C = 0;
                    player_movement_globals.at_0x00605998 = 0;
                    publish_world_snapshot();
                }
                if (frame_period == std::chrono::steady_clock::duration::zero() ||
                    now >= next_visual_update) {
                    const auto advance_effects = [dat](std::uint8_t effect_id) {
                        if (!dat) throw std::runtime_error("Tibia.dat is unavailable for effect timing");
                        const auto* effect = dat->effects.find(effect_id);
                        if (!effect) throw std::runtime_error("visual effect is missing from Tibia.dat");
                        return static_cast<std::uint32_t>(effect->animation_frames);
                    };
                    if (visual_effects.advance_tick(advance_effects, ::GetTickCount()))
                        publish_world_snapshot();
                    if (frame_period != std::chrono::steady_clock::duration::zero()) {
                        do { next_visual_update += frame_period; }
                        while (next_visual_update <= now);
                    }
                }
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - talk_update_time).count();
                bool talk_state_changed = !status_messages.empty();
                if (elapsed >= status_update_interval.count()) {
                    talk_system.advance(static_cast<std::uint32_t>(elapsed));
                    talk_update_time = now;
                    talk_state_changed = true;
                }
                while (now >= status_update_time) {
                    talk_system.advance_status_tick();
                    status_update_time += status_update_interval;
                    talk_state_changed = true;
                }
                if (talk_state_changed) publish_world_snapshot();
                if (!received_initial_response && std::chrono::steady_clock::now() >= response_deadline) {
                    throw std::runtime_error("timed out waiting for the game-server response");
                }
                if (!connection->wait_for_data(std::chrono::milliseconds(1))) continue;
                const auto received = connection->receive_some(buffer);
                if (received == 0) {
                    throw std::runtime_error("game server closed the connection");
                }
                received_initial_response = true;
                stream.append(std::span<const std::uint8_t>(buffer.data(), received));
                while (auto frame = stream.next()) {
                    if (frame->logical_size < 5 || frame->logical_size > frame->bytes.size()) {
                        throw std::runtime_error("game server returned an invalid decrypted message size");
                    }
                    const auto payload = std::span<const std::uint8_t>(frame->bytes.data() + 4,
                        frame->logical_size - 4);
                    std::size_t offset = 0;
                    while (offset < payload.size()) {
                        const auto opcode = payload[offset];
                        const auto message = payload.subspan(offset + 1);
                        const auto result = dispatcher.dispatch(
                            tibia::network::DispatchPhase::state6, opcode, message);
                        publish_world_snapshot();
                        if (result.status == tibia::network::DispatchStatus::unknown_opcode) {
                            publish_login_event(mailbox, window,
                                LoginUiEvent{std::nullopt,
                                    L"Game server replied with an unimplemented state-6 opcode " +
                                        std::to_wstring(opcode) + L"; world processing remains pending.",
                                    true, true});
                            completed = true;
                        } else if (opcode != 0x1E && opcode != 0x0B && opcode != 0x0A &&
                            opcode != 0x64 && (opcode < 0x65 || opcode > 0x72) &&
                            (opcode < 0x78 || opcode > 0x79) &&
                            (opcode < 0x7D || opcode > 0x7F) &&
                            (opcode < 0xAB || opcode > 0xB4)) {
                            publish_login_event(mailbox, window,
                                LoginUiEvent{std::nullopt,
                                    L"Observed state-6 opcode " + std::to_wstring(opcode) +
                                        L" parsed; world-state handling is pending.",
                                    false, true});
                        }
                        if (result.unconsumed_bytes > message.size())
                            throw std::runtime_error("game server handler returned an invalid message offset");
                        offset += message.size() - result.unconsumed_bytes + 1;
                        if (completed || stop.stop_requested()) break;
                    }
                }
            }
        } catch (const std::exception& error) {
            if (!stop.stop_requested()) {
                publish_login_event(mailbox, window,
                    LoginUiEvent{std::nullopt, error_to_wide(error.what()), true, true});
            }
        }
        connection->close();
        communication_session_mailbox->publish_private_channel_filter_state(false);
        communication_session_mailbox->publish_active_channel(std::nullopt);
        report_violation_buffer->clear();
    });
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

const wchar_t* map_window_item_frame_status_name(
    tibia::client::MapWindowItemFrameStatus status) noexcept {
    using Status = tibia::client::MapWindowItemFrameStatus;
    switch (status) {
    case Status::complete: return L"complete";
    case Status::no_map: return L"no_map";
    case Status::unresolved_filter_grid: return L"unresolved_filter_grid";
    case Status::unresolved_active_floor: return L"unresolved_active_floor";
    case Status::unresolved_map_item: return L"unresolved_map_item";
    case Status::unresolved_cell_data: return L"unresolved_cell_data";
    case Status::unresolved_effect_state: return L"unresolved_effect_state";
    }
    return L"unknown";
}

void report_world_render_diagnostic(std::wstring state) {
    static std::wstring last_state;
    static bool first_file_write = true;
    static const auto log_path = [] {
        std::array<wchar_t, 32768> module_path{};
        const auto length = GetModuleFileNameW(nullptr, module_path.data(),
            static_cast<DWORD>(module_path.size()));
        if (length == 0 || length >= module_path.size()) return std::filesystem::path{};
        return std::filesystem::path(std::wstring(module_path.data(), length)).parent_path() /
            L"tibia-world-render.log";
    }();
    if (state == last_state) return;
    last_state = std::move(state);
    last_state.push_back(L'\n');
    OutputDebugStringW(last_state.c_str());
    if (!log_path.empty()) {
        std::wofstream log(log_path, first_file_write ? std::ios::out : std::ios::app);
        if (log) {
            log << last_state;
            first_file_write = false;
        }
    }
}

void render_frame(ClientHost& host) {
    if (!host.renderer) return;
    const auto visible_dialog = active_dialog(host);
    const auto now = std::chrono::steady_clock::now();
    if (host.framerate_measurement_start == std::chrono::steady_clock::time_point{})
        host.framerate_measurement_start = now;
    ++host.framerate_measurement_frames;
    const auto elapsed = now - host.framerate_measurement_start;
    if (elapsed >= std::chrono::seconds(1)) {
        host.measured_framerate = static_cast<float>(host.framerate_measurement_frames) /
            std::chrono::duration<float>(elapsed).count();
        host.framerate_measurement_frames = 0;
        host.framerate_measurement_start = now;
    }
    host.renderer->begin_frame(host.screen == UiScreen::game_world
        ? tibia::render::RgbaColor{0, 0, 0, 255}
        : tibia::render::RgbaColor{24, 24, 24, 255});
    RECT client{};
    if (host.window) GetClientRect(host.window, &client);
    const int client_width = static_cast<int>(client.right - client.left);
    const int client_height = static_cast<int>(client.bottom - client.top);
    if (host.screen == UiScreen::game_world) {
        const auto snapshot = host.game_world_mailbox->read();
        if (!snapshot || !snapshot->map) {
            report_world_render_diagnostic(L"Tibia world render gate: snapshot=" +
                std::wstring(snapshot ? L"present" : L"missing") + L", map=missing");
        }
        if (snapshot && snapshot->map && (!host.dat || !host.tibia_spr ||
            !host.tibia_pic || !host.tibia_spr_bytes || !host.tibia_pic_bytes ||
            !host.map_tile_cache || !host.map_surface_renderer)) {
            report_world_render_diagnostic(L"Tibia world render gate: resources=" +
                std::wstring(host.dat ? L"dat:ok" : L"dat:missing") + L"," +
                (host.tibia_spr ? L"spr:ok" : L"spr:missing") + L"," +
                (host.tibia_pic ? L"pic:ok" : L"pic:missing") + L"," +
                (host.tibia_spr_bytes ? L"sprBytes:ok" : L"sprBytes:missing") + L"," +
                (host.tibia_pic_bytes ? L"picBytes:ok" : L"picBytes:missing") + L"," +
                (host.map_tile_cache ? L"tileCache:ok" : L"tileCache:missing") + L"," +
                (host.map_surface_renderer ? L"surfaceRenderer:ok" : L"surfaceRenderer:missing"));
        }
        if (snapshot && snapshot->map && host.dat && host.tibia_spr &&
            host.tibia_pic && host.tibia_spr_bytes && host.tibia_pic_bytes &&
            host.map_tile_cache && host.map_surface_renderer) {
            const auto camera = tibia::client::resolve_map_window_camera_offset(*snapshot);
            const auto layout = tibia::client::resolve_game_window_layout({
                client_width, client_height, host.console_height,
                tibia::client::map_panel_height, tibia::client::map_window_minimum_height, host.no_stretching});
            if (!camera || !layout) {
                std::size_t decoded_map_objects{};
                for (const auto& tile : snapshot->map->tiles)
                    decoded_map_objects += tile.objects.size();
                report_world_render_diagnostic(L"Tibia world render gate: camera=" +
                    std::wstring(camera ? L"resolved" : L"unresolved") + L", layout=" +
                    std::wstring(layout ? L"resolved" : L"unresolved") + L", mapTiles=" +
                    std::to_wstring(snapshot->map->tiles.size()) + L", creatures=" +
                    std::to_wstring(snapshot->creatures.size()) + L", centerFloor=" +
                    std::to_wstring(snapshot->map->center_floor) + L", center=" +
                    std::to_wstring(snapshot->map->center_x) + L"," +
                    std::to_wstring(snapshot->map->center_y) + L", localPlayer=" +
                    std::to_wstring(snapshot->map->local_player_id) + L", decodedObjects=" +
                    std::to_wstring(decoded_map_objects) + L", unparsedMapBytes=" +
                    std::to_wstring(snapshot->map->unparsed_tail.size()));
            }
            if (camera && layout) {
                const auto frame = tibia::client::prepare_map_window_item_frame(
                    *snapshot, *host.dat, host.animation_clock, *camera,
                    *host.map_tile_cache);
                std::size_t decoded_map_objects{};
                std::size_t map_tiles_with_ground{};
                std::size_t unresolved_map_dat_objects{};
                for (const auto& tile : snapshot->map->tiles) {
                    decoded_map_objects += tile.objects.size();
                    bool has_ground = false;
                    for (const auto& object : tile.objects) {
                        if (object.creature_id != 0 || object.type == 0 || object.type == 0x63)
                            continue;
                        const auto* item = host.dat->find_item(object.type);
                        if (item == nullptr) {
                            ++unresolved_map_dat_objects;
                            continue;
                        }
                        if (item->has_native_property(0)) has_ground = true;
                    }
                    if (has_ground) ++map_tiles_with_ground;
                }
                auto local_player_position = std::wstring(L"missing");
                for (const auto& creature : snapshot->creatures) {
                    if (creature.state.creature_id != snapshot->map->local_player_id) continue;
                    local_player_position = std::to_wstring(creature.world_x) + L"," +
                        std::to_wstring(creature.world_y) + L"," +
                        std::to_wstring(creature.world_floor);
                    break;
                }
                const auto queued_tiles = std::count_if(frame.visible_tiles.begin(),
                    frame.visible_tiles.end(), [](const auto& tile) {
                        return tile.batch.status == tibia::map::TileRenderStatus::queued;
                    });
                std::size_t resolved_objects{};
                std::size_t resolved_sprite_routes{};
                std::size_t unresolved_routes{};
                std::size_t waiting_routes{};
                const auto count_routes = [&](const auto& route) {
                    resolved_sprite_routes += route.routes.size();
                    if (route.status == tibia::render::MapItemVisitRouteStatus::waiting_for_effects)
                        ++waiting_routes;
                    if (route.status == tibia::render::MapItemVisitRouteStatus::unresolved_render_inputs ||
                        route.status == tibia::render::MapItemVisitRouteStatus::unresolved_sprite_layout ||
                        route.status == tibia::render::MapItemVisitRouteStatus::unresolved_cell_data)
                        ++unresolved_routes;
                };
                for (const auto& visit : frame.visits) {
                    resolved_objects += visit.visit.ordered_objects.size();
                    count_routes(visit.standard_route);
                    count_routes(visit.deferred_route);
                }
                report_world_render_diagnostic(L"Tibia world render frame: status=" +
                    std::wstring(map_window_item_frame_status_name(frame.status)) + L", mapTiles=" +
                    std::to_wstring(snapshot->map->tiles.size()) + L", creatures=" +
                    std::to_wstring(snapshot->creatures.size()) + L", centerFloor=" +
                    std::to_wstring(snapshot->map->center_floor) + L", center=" +
                    std::to_wstring(snapshot->map->center_x) + L"," +
                    std::to_wstring(snapshot->map->center_y) + L", localPlayer=" +
                    std::to_wstring(snapshot->map->local_player_id) + L"@" +
                    local_player_position + L", decodedObjects=" +
                    std::to_wstring(decoded_map_objects) + L", unparsedMapBytes=" +
                    std::to_wstring(snapshot->map->unparsed_tail.size()) + L", visits=" +
                    std::to_wstring(frame.visits.size()) + L", objects=" +
                    std::to_wstring(resolved_objects) + L", spriteRoutes=" +
                    std::to_wstring(resolved_sprite_routes) + L", unresolvedRoutes=" +
                    std::to_wstring(unresolved_routes) + L", waitingRoutes=" +
                    std::to_wstring(waiting_routes) + L", queuedTiles=" +
                    std::to_wstring(queued_tiles) + L", highestFloor=" +
                    std::to_wstring(frame.highest_active_floor) + L", groundTiles=" +
                    std::to_wstring(map_tiles_with_ground) + L", unknownDatObjects=" +
                    std::to_wstring(unresolved_map_dat_objects) + L", camera=" +
                    std::to_wstring(frame.camera_offset_x) + L"," +
                    std::to_wstring(frame.camera_offset_y));
                if (frame.status == tibia::client::MapWindowItemFrameStatus::complete ||
                    frame.status == tibia::client::MapWindowItemFrameStatus::unresolved_effect_state ||
                    frame.status == tibia::client::MapWindowItemFrameStatus::unresolved_map_item ||
                    frame.status == tibia::client::MapWindowItemFrameStatus::unresolved_cell_data) {
                    constexpr tibia::client::MapWindowLayout logical_map_view{
                        0, 0, tibia::client::map_window_reference_width,
                        tibia::client::map_window_reference_height};
                    const int draw_x = host.no_stretching
                        ? layout->map.x +
                            (layout->map.width - logical_map_view.width) / 2
                        : layout->map.x;
                    const int draw_y = host.no_stretching
                        ? layout->map.y +
                            (layout->map.height - logical_map_view.height) / 2
                        : layout->map.y;
                    const int draw_width = host.no_stretching
                        ? logical_map_view.width : layout->map.width;
                    const int draw_height = host.no_stretching
                        ? logical_map_view.height : layout->map.height;
                    host.renderer->set_draw_transform(
                        logical_map_view.width, logical_map_view.height,
                        draw_x, draw_y, draw_width, draw_height,
                        layout->map.x, layout->map.y,
                        layout->map.width, layout->map.height);
                    struct PendingMapText {
                        std::string text;
                        int x{};
                        int y{};
                        std::uint8_t color{};
                    };
                    std::vector<PendingMapText> pending_map_text;
                    const auto creature_tick = static_cast<std::uint32_t>(timeGetTime());
                    host.map_creature_renderer.prepare_frame(*snapshot, frame, *host.dat,
                        *host.tibia_spr_bytes, *host.tibia_spr,
                        host.animation_clock, creature_tick - animation_epoch_tick);
                    const auto map_draw_calls = host.map_surface_renderer->draw(*host.renderer, frame,
                        *host.tibia_spr_bytes, *host.tibia_spr,
                        *host.tibia_pic_bytes, *host.tibia_pic, logical_map_view,
                        0, 0,
                        [&host, &snapshot, &frame, &logical_map_view,
                            &pending_map_text](std::int32_t floor) {
                            tibia::client::MapWindowEffectRenderer::TextDraw draw_text;
                            if (host.show_textual_effects) {
                                draw_text = [&pending_map_text](std::string_view text,
                                    int x, int y, std::uint8_t color) {
                                    pending_map_text.push_back({std::string(text), x, y, color});
                                };
                            }
                            (void)host.map_effect_renderer.collect_floor_text_effects(
                                *snapshot,
                                static_cast<std::uint8_t>(floor),
                                frame.camera_offset_x, frame.camera_offset_y,
                                0, 0, draw_text);
                        },
                        [&host, &snapshot, &frame, &logical_map_view](std::int32_t floor,
                            std::int32_t local_x, std::int32_t local_y) {
                            (void)host.map_effect_renderer.draw_tile(*host.renderer,
                                *snapshot, *host.dat, *host.tibia_spr_bytes,
                                *host.tibia_spr, logical_map_view,
                                static_cast<std::uint8_t>(floor), local_x, local_y,
                                frame.camera_offset_x, frame.camera_offset_y, 0, 0);
                        },
                        [&host, &frame, &snapshot, &logical_map_view] {
                            (void)host.map_lighting_renderer.draw_ambient(*host.renderer,
                                frame, *host.tibia_pic_bytes, *host.tibia_pic,
                                static_cast<std::uint8_t>(
                                    std::clamp(host.ambient_light, 0, 128)),
                                host.show_light_effects, snapshot->world_light,
                                snapshot->map->center_floor, logical_map_view);
                        },
                        [&host, &snapshot, &frame, &logical_map_view](std::int32_t floor,
                            std::int32_t local_x, std::int32_t local_y) {
                            (void)host.map_creature_renderer.draw_tile(*host.renderer,
                                frame, logical_map_view, static_cast<std::uint8_t>(floor),
                                local_x, local_y, 0, 0);
                        });
                    report_world_render_diagnostic(L"Tibia world render output: tileDrawCalls=" +
                        std::to_wstring(map_draw_calls.draw_calls) + L", cellsWithPixels=" +
                        std::to_wstring(map_draw_calls.visible_cells_with_pixels) + L", nontransparentPixels=" +
                        std::to_wstring(map_draw_calls.nontransparent_pixels) + L", mapEffects=" +
                        std::to_wstring(snapshot->map_effects.size()) + L", lights=" +
                        std::to_wstring(frame.light_sources.size()));
                    const auto creature_info = host.map_creature_renderer.prepare_info_overlays(
                        *snapshot, *host.dat, host.creature_info_mode,
                        logical_map_view.width, logical_map_view.height,
                        host.ui_font_heights[2], frame.camera_offset_x,
                        frame.camera_offset_y,
                        [&host](std::string_view name) {
                            return bitmap_text_width(host, decode_ui_string(
                                std::string(name)), 2, host.window, false);
                        },
                        [&host](std::int32_t x, std::int32_t y) {
                            return host.map_lighting_renderer.average_light(x, y);
                        });
                    for (const auto& overlay : creature_info) {
                        host.renderer->draw_clipped(battle_health_track_texture,
                            overlay.bar_x, overlay.bar_y, 27, 4,
                            logical_map_view.x, logical_map_view.y,
                            logical_map_view.width, logical_map_view.height);
                        if (overlay.bar_fill_width > 0)
                            host.renderer->draw_clipped(
                                creature_info_solid_texture(host, overlay.color),
                                overlay.bar_x + 1, overlay.bar_y + 1,
                                overlay.bar_fill_width, 2,
                                logical_map_view.x, logical_map_view.y,
                                logical_map_view.width, logical_map_view.height);
                        draw_creature_info_name(host, overlay.name,
                            overlay.name_x, overlay.name_y, overlay.color,
                            logical_map_view);
                        if (overlay.draw_primary_mark) {
                            const auto& skin = tibia::client::ui_skins[
                                overlay.primary_mark_skin];
                            draw_skin(host, overlay.primary_mark_skin,
                                overlay.primary_mark_x, overlay.primary_mark_y,
                                skin.width, skin.height);
                        }
                        if (overlay.draw_secondary_mark) {
                            const auto& skin = tibia::client::ui_skins[
                                overlay.secondary_mark_skin];
                            draw_skin(host, overlay.secondary_mark_skin,
                                overlay.secondary_mark_x, overlay.secondary_mark_y,
                                skin.width, skin.height);
                        }
                    }
                    for (const auto& text : pending_map_text)
                        draw_map_text_effect(host, text.text, text.x, text.y,
                            text.color, logical_map_view);
                    const tibia::client::TalkboxLayoutContext talkbox_context{
                        logical_map_view.width, logical_map_view.height,
                        snapshot->status_visible ? host.ui_font_cell_widths[1] + 10 : 0,
                        frame.camera_offset_x, frame.camera_offset_y,
                        host.no_stretching};
                    const tibia::client::TalkboxFontView talkbox_font{
                        host.ui_font_images[1], host.talkbox_font_glyphs,
                        host.ui_font_cell_heights[1], host.ui_font_spacing[1],
                        host.ui_font_missing_widths[1]};
                    host.talkbox_renderer.draw(*host.renderer, snapshot->talkboxes,
                        *snapshot->map, talkbox_context, talkbox_font, logical_map_view);
                    draw_game_status_message(host, *snapshot, logical_map_view);
                    if (host.show_textual_effects)
                        (void)host.map_effect_renderer.draw_deferred_projectiles(
                            *host.renderer, *snapshot, *host.dat, *host.tibia_spr_bytes,
                            *host.tibia_spr, logical_map_view, frame.highest_active_floor,
                            frame.camera_offset_x, frame.camera_offset_y);
                    host.renderer->clear_draw_transform();
                    const auto map_frame = tibia::client::layout_map_window_frame(
                        layout->map);
                    for (const auto& edge : map_frame.edges)
                        draw_skin(host, edge.skin, edge.rect.x, edge.rect.y,
                            edge.rect.width, edge.rect.height);
                    const auto inventory_x = layout->sidebar.x +
                        tibia::client::inventory_panel_offset_x;
                    const auto inventory_y = tibia::client::inventory_panel_offset_y;
                    if (host.inventory_minimized) {
                        const int collapsed_x = layout->sidebar.x;
                        const RECT maximize_button{
                            collapsed_x + tibia::client::inventory_maximize_button_x,
                            tibia::client::inventory_maximize_button_y,
                            collapsed_x + tibia::client::inventory_maximize_button_x +
                                tibia::client::inventory_window_button_size,
                            tibia::client::inventory_maximize_button_y +
                                tibia::client::inventory_window_button_size};
                        const bool maximize_pressed =
                            (host.mouse_buttons & 1U) != 0 &&
                            contains_point(maximize_button, host.mouse);
                        draw_skin(host, 99,
                            collapsed_x + tibia::client::inventory_minimized_content_x,
                            tibia::client::inventory_minimized_content_y,
                            tibia::client::inventory_minimized_content_width,
                            tibia::client::inventory_minimized_content_height);
                        draw_skin(host, 100,
                            collapsed_x + tibia::client::inventory_minimized_content_x,
                            tibia::client::inventory_minimized_content_y + 44, 94, 13);
                        draw_skin(host, 101,
                            collapsed_x + tibia::client::inventory_minimized_content_x + 93,
                            tibia::client::inventory_minimized_content_y + 44, 1, 13);
                        draw_skin(host, maximize_pressed ? 141 : 140,
                            maximize_button.left, maximize_button.top,
                            tibia::client::inventory_window_button_size,
                            tibia::client::inventory_window_button_size);
                        draw_combat_control_panel(host, collapsed_x, true);
                    } else {
                    const RECT minimize_button{
                        layout->sidebar.x + tibia::client::inventory_minimize_button_x,
                        tibia::client::inventory_minimize_button_y,
                        layout->sidebar.x + tibia::client::inventory_minimize_button_x +
                            tibia::client::inventory_window_button_size,
                        tibia::client::inventory_minimize_button_y +
                            tibia::client::inventory_window_button_size};
                    const bool minimize_pressed =
                        (host.mouse_buttons & 1U) != 0 &&
                        contains_point(minimize_button, host.mouse);
                    draw_skin(host, minimize_pressed ? 139 : 138,
                        minimize_button.left, minimize_button.top,
                        tibia::client::inventory_window_button_size,
                        tibia::client::inventory_window_button_size);
                    draw_skin(host, 98, inventory_x, inventory_y + 124, 34, 21);
                    draw_skin(host, 98, inventory_x + 74, inventory_y + 124, 34, 21);
                    for (const auto& slot : tibia::client::inventory_slot_layouts) {
                        const int slot_y = inventory_y + slot.y;
                        draw_skin_tiled(host, 102, inventory_x + slot.frame_x,
                            inventory_y + slot.frame_y,
                            34, 34);
                        const auto item = std::find_if(snapshot->inventory.begin(),
                            snapshot->inventory.end(), [&slot](const auto& entry) {
                                return entry.slot == slot.slot;
                            });
                        if (item == snapshot->inventory.end() || item->item.type == 0)
                            draw_skin(host, slot.empty_object_type,
                                inventory_x + slot.x, slot_y, 32, 32);
                    }
                    host.inventory_panel_renderer.draw(*host.renderer, *snapshot,
                        *host.dat, *host.tibia_spr_bytes, *host.tibia_spr, *layout,
                        host.animation_clock);
                    for (const auto& slot : tibia::client::inventory_slot_layouts) {
                        const auto item = std::find_if(snapshot->inventory.begin(),
                            snapshot->inventory.end(), [&slot](const auto& entry) {
                                return entry.slot == slot.slot;
                            });
                        if (item == snapshot->inventory.end() || !item->item.has_extra)
                            continue;
                        const auto* object = host.dat->find_item(item->item.type);
                        if (!object) continue;
                        const auto quantity = tibia::client::inventory_item_quantity_label(
                            *object, item->item.extra);
                        if (quantity) {
                            const auto label = std::to_wstring(*quantity);
                            const auto text_width = bitmap_text_width(host, label, 1, host.window);
                            const auto position = tibia::client::inventory_quantity_position(
                                inventory_x + slot.x, inventory_y + slot.y,
                                text_width);
                            draw_bitmap_text(host, label, position.x, position.y, 0, false, 1, 192);
                        }
                        const auto secondary_quantity =
                            tibia::client::inventory_item_secondary_quantity_label(
                                *object, item->item.extra);
                        if (secondary_quantity) {
                            const auto secondary_label = std::to_wstring(*secondary_quantity);
                            const auto secondary_width = bitmap_text_width(
                                host, secondary_label, 1, host.window);
                            const auto secondary_position =
                                tibia::client::inventory_secondary_quantity_position(
                                    inventory_x + slot.x, inventory_y + slot.y,
                                    secondary_width);
                            draw_bitmap_text(host, secondary_label, secondary_position.x,
                                secondary_position.y, 0, false, 1, 192);
                        }
                    }
                    draw_combat_control_panel(host, layout->sidebar.x, false);
                    }
                    const int automap_y = host.inventory_minimized
                        ? tibia::client::automap_panel_minimized_inventory_y
                        : tibia::client::automap_panel_full_inventory_y;
                    draw_automap_panel(host, layout->sidebar.x, automap_y,
                        snapshot->automap_view ? &*snapshot->automap_view : nullptr);
                    const int status_window_y = automap_y +
                        tibia::client::layout_automap_panel(
                            layout->sidebar.x, automap_y).panel.height;
                    draw_player_status_bars(host, snapshot->player_statistics,
                        layout->sidebar.x, status_window_y);
                    draw_game_screen_control_buttons(host, layout->sidebar.x,
                        automap_y + tibia::client::automap_control_row_offset);
                    if (const auto skills_plan = skills_panel_window_plan(host, host.window))
                        draw_skills_content_window(host, *skills_plan);
                    if (const auto battle_plan = battle_panel_window_plan(host, host.window))
                        draw_battle_content_window(host, *battle_plan);
                    if (const auto vip_plan = vip_panel_window_plan(host, host.window))
                        draw_vip_content_window(host, *vip_plan, snapshot->vip_contacts);
                    draw_skills_context_menu(host);
                    draw_battle_context_menu(host);
                    if (host.dat && host.tibia_spr && host.tibia_spr_bytes)
                        draw_container_panels(host, host.window, *snapshot, *layout);
                    if (layout->console_panel) {
                        draw_console_panel_frame(host, layout->console.x,
                            layout->console.y, layout->console.width,
                            layout->console.height);
                        const auto message_frame =
                            tibia::client::resolve_console_message_frame_visuals(
                                *layout->console_panel, layout->console.x,
                                layout->console.y, true);
                        if (message_frame) {
                            for (const auto& piece : *message_frame)
                                draw_skin(host, piece.skin, piece.rect.x,
                                    piece.rect.y, piece.rect.width,
                                    piece.rect.height);
                        }
                        const auto report_snapshot = host.report_violation_buffer
                            ? host.report_violation_buffer->snapshot()
                            : tibia::client::ReportViolationBufferSnapshot{};
                        const bool report_buffer_open = report_snapshot.open;
                        if (report_buffer_open &&
                            !host.report_violation_buffer_was_open)
                            host.report_violation_tab_selected = true;
                        if (!report_buffer_open)
                            host.report_violation_tab_selected = false;
                        host.report_violation_buffer_was_open = report_buffer_open;
                        const bool report_tab_active = report_buffer_open &&
                            host.report_violation_tab_selected;
                        const auto channel_view = host.communication_session_mailbox
                            ? host.communication_session_mailbox->channel_view()
                            : tibia::client::CommunicationChannelView{};
                        const auto current_console_tick = GetTickCount64() / 100U;
                        const bool default_tab_active = !report_tab_active &&
                            !channel_view.selected_identifier;
                        update_console_tab_unread_state(
                            host.default_console_tab_state,
                            snapshot->primary_console_messages.size(),
                            default_tab_active, current_console_tick);
                        update_console_tab_unread_state(
                            host.report_console_tab_state,
                            report_snapshot.messages.size(), report_tab_active,
                            current_console_tick);
                        for (std::size_t index = 0;
                            index < channel_view.channels.size(); ++index) {
                            const auto& channel = channel_view.channels[index];
                            auto& state = host.channel_console_tab_states[channel.id];
                            const auto count = index <
                                channel_view.channel_message_counts.size()
                                ? channel_view.channel_message_counts[index] : 0;
                            update_console_tab_unread_state(state, count,
                                channel_view.selected_identifier == channel.id,
                                current_console_tick);
                        }
                        for (auto iterator = host.channel_console_tab_states.begin();
                            iterator != host.channel_console_tab_states.end();) {
                            const bool channel_open = std::any_of(
                                channel_view.channels.begin(), channel_view.channels.end(),
                                [identifier = iterator->first](const auto& channel) {
                                    return channel.id == identifier;
                                });
                            if (!channel_open)
                                iterator = host.channel_console_tab_states.erase(iterator);
                            else
                                ++iterator;
                        }
                        if (report_tab_active)
                            draw_report_violation_transcript(host, *layout->console_panel,
                                layout->console.x, layout->console.y, report_snapshot);
                        else if (channel_view.selected_identifier) {
                            auto channel_snapshot = *snapshot;
                            channel_snapshot.primary_console_messages = channel_view.messages;
                            draw_primary_console_transcript(host, *layout->console_panel,
                                layout->console.x, layout->console.y, channel_snapshot, true);
                        } else
                            draw_primary_console_transcript(host, *layout->console_panel,
                                layout->console.x, layout->console.y, *snapshot, true);
                        const auto default_tab =
                            tibia::client::resolve_default_console_tab_visual(
                                *layout->console_panel, layout->console.x,
                                layout->console.y, !report_tab_active &&
                                    !channel_view.selected_identifier, false);
                        if (default_tab) {
                            draw_skin(host, default_tab->skin, default_tab->rect.x,
                                default_tab->rect.y, default_tab->rect.width,
                                default_tab->rect.height);
                            draw_console_tab_title(host, default_tab->title,
                                default_tab->rect, default_tab->text_y,
                                default_tab->font_index,
                                console_tab_text_rgb(host.default_console_tab_state,
                                    default_tab_active, current_console_tick));
                        }
                        constexpr int channel_tab_width = 96;
                        const int channel_tab_base = 1 + (report_buffer_open ? 1 : 0);
                        const int channel_slots = report_tab_active ? 0 : std::max(0,
                            (layout->console_panel->tab_strip.width / channel_tab_width) - 1 -
                            (report_buffer_open ? 1 : 0));
                        const int max_channel_offset = std::max(0,
                            static_cast<int>(channel_view.channels.size()) - channel_slots);
                        if (channel_slots > 0 && channel_view.selected_identifier) {
                            const auto selected = std::find_if(
                                channel_view.channels.begin(), channel_view.channels.end(),
                                [&channel_view](const auto& channel) {
                                    return channel.id ==
                                        *channel_view.selected_identifier;
                                });
                            if (selected != channel_view.channels.end()) {
                                const auto selected_index = static_cast<int>(
                                    std::distance(channel_view.channels.begin(), selected));
                                if (selected_index < host.channel_tab_scroll_offset)
                                    host.channel_tab_scroll_offset = selected_index;
                                else if (selected_index >=
                                    host.channel_tab_scroll_offset + channel_slots)
                                    host.channel_tab_scroll_offset = selected_index -
                                        channel_slots + 1;
                            }
                        }
                        host.channel_tab_scroll_offset = std::clamp(
                            host.channel_tab_scroll_offset, 0, max_channel_offset);
                        for (int index = 0; index < channel_slots &&
                             host.channel_tab_scroll_offset + index <
                                static_cast<int>(channel_view.channels.size()); ++index) {
                            const auto& channel = channel_view.channels[
                                static_cast<std::size_t>(host.channel_tab_scroll_offset + index)];
                            const bool active = channel_view.selected_identifier == channel.id;
                            const int x = layout->console.x + layout->console_panel->tab_strip.x +
                                channel_tab_width * (index + channel_tab_base);
                            const int y = layout->console.y + layout->console_panel->tab_strip.y;
                            draw_skin(host, active ? 186 : 187, x, y,
                                channel_tab_width, 18);
                            const auto title = decode_ui_string(channel.name);
                            const tibia::client::ConsolePanelRect tab_rect{
                                x, y, channel_tab_width, 18};
                            const auto tab_state =
                                host.channel_console_tab_states.find(channel.id);
                            const auto text_rgb = tab_state ==
                                host.channel_console_tab_states.end()
                                ? 0x00808080U
                                : console_tab_text_rgb(tab_state->second, active,
                                    current_console_tick);
                            draw_console_tab_title(host, title, tab_rect, 4, 1,
                                text_rgb);
                        }
                        const auto tab = tibia::client::resolve_report_violation_tab_visual(
                            *layout->console_panel, layout->console.x, layout->console.y,
                            report_buffer_open, report_tab_active, true);
                        if (tab) {
                            draw_skin(host, tab->skin, tab->rect.x, tab->rect.y,
                                tab->rect.width, tab->rect.height);
                            draw_console_tab_title(host, tab->title, tab->rect,
                                tab->text_y, tab->font_index,
                                console_tab_text_rgb(host.report_console_tab_state,
                                    report_tab_active, current_console_tick));
                        }
                        if (report_tab_active && tab) {
                            draw_skin(host, tab->skin, tab->rect.x, tab->rect.y,
                                tab->rect.width, tab->rect.height);
                            draw_console_tab_title(host, tab->title, tab->rect,
                                tab->text_y, tab->font_index,
                                console_tab_text_rgb(host.report_console_tab_state,
                                    true, current_console_tick));
                        } else if (channel_view.selected_identifier) {
                            const auto selected = std::find_if(
                                channel_view.channels.begin(), channel_view.channels.end(),
                                [&channel_view](const auto& channel) {
                                    return channel.id ==
                                        *channel_view.selected_identifier;
                                });
                            if (selected != channel_view.channels.end()) {
                                const auto visible_index = static_cast<int>(
                                    std::distance(channel_view.channels.begin(), selected)) -
                                    host.channel_tab_scroll_offset;
                                if (visible_index >= 0 && visible_index < channel_slots) {
                                    const int x = layout->console.x +
                                        layout->console_panel->tab_strip.x +
                                        channel_tab_width *
                                            (visible_index + channel_tab_base);
                                    const int y = layout->console.y +
                                        layout->console_panel->tab_strip.y;
                                    const tibia::client::ConsolePanelRect active_rect{
                                        x, y, channel_tab_width, 18};
                                    draw_skin(host, 186, x, y, channel_tab_width, 18);
                                    const auto title = decode_ui_string(selected->name);
                                    const auto state =
                                        host.channel_console_tab_states.find(selected->id);
                                    draw_console_tab_title(host, title, active_rect, 4, 1,
                                        state == host.channel_console_tab_states.end()
                                            ? 0x00e0e0e0U
                                            : console_tab_text_rgb(state->second, true,
                                                current_console_tick));
                                }
                            }
                        } else if (default_tab_active && default_tab) {
                            draw_skin(host, default_tab->skin,
                                default_tab->rect.x, default_tab->rect.y,
                                default_tab->rect.width, default_tab->rect.height);
                            draw_console_tab_title(host, default_tab->title,
                                default_tab->rect, default_tab->text_y,
                                default_tab->font_index,
                                console_tab_text_rgb(host.default_console_tab_state,
                                    true, current_console_tick));
                        }
                        const auto controls =
                            tibia::client::resolve_console_panel_control_visuals(
                                *layout->console_panel, layout->console.x,
                                layout->console.y, true,
                                channel_view.server_messages_enabled,
                                host.console_volume_state);
                        if (controls) {
                            for (const auto& control : *controls) {
                                const RECT bounds{control.rect.x, control.rect.y,
                                    control.rect.x + control.rect.width,
                                    control.rect.y + control.rect.height};
                                const bool hovered = contains_point(bounds, host.mouse);
                                const bool pressed = hovered &&
                                    (host.mouse_buttons & 1U) != 0;
                                const auto skin = pressed ? control.pressed_skin
                                    : hovered ? control.hover_skin : control.skin;
                                draw_skin(host, skin, control.rect.x,
                                    control.rect.y, control.rect.width,
                                    control.rect.height);
                            }
                        }
                        const auto input =
                            tibia::client::resolve_console_text_input_visual(
                                *layout->console_panel, layout->console.x,
                                layout->console.y, true,
                                host.console_input_focused);
                        if (input) draw_console_text_input(host, *input);
                    }
                }
            }
        }
        if (host.screen == UiScreen::game_world)
            draw_console_context_menu(host);
        if (host.screen == UiScreen::game_world)
            draw_vip_context_menu(host);
        if (host.window && visible_dialog == ActiveDialog::options) {
            const auto dialog = options_rect(host.window, host.options_dialog_offset);
            draw_dialog_frame(host, dialog);
            draw_options_controls(host, dialog);
        }
        if (host.window && visible_dialog == ActiveDialog::console_options)
            draw_console_options_page(host);
        if (host.window && visible_dialog == ActiveDialog::hotkey_options)
            draw_hotkey_options_page(host);
        if (host.window && visible_dialog == ActiveDialog::general_options)
            draw_general_options_page(host);
        if (host.window && visible_dialog == ActiveDialog::network_options)
            draw_network_options_page(host);
        if (host.window && visible_dialog == ActiveDialog::graphics_options)
            draw_graphics_options_page(host);
        if (host.window && visible_dialog == ActiveDialog::advanced_graphics_options)
            draw_advanced_graphics_options_page(host);
        if (host.window && visible_dialog == ActiveDialog::help)
            draw_help_dialog(host);
        if (host.window && visible_dialog == ActiveDialog::hints)
            draw_hints_dialog(host);
        if (host.window && visible_dialog == ActiveDialog::hint_content)
            draw_hint_content_dialog(host);
        if (host.window && visible_dialog == ActiveDialog::report_violation)
            draw_report_violation_dialog(host);
        if (host.window &&
            visible_dialog == ActiveDialog::rule_violation_request)
            draw_rule_violation_request_dialog(host);
        if (host.window && visible_dialog == ActiveDialog::new_channel)
            draw_new_channel_dialog(host);
        if (host.window && visible_dialog == ActiveDialog::quest_log)
            draw_quest_log_dialog(host);
        if (host.window && visible_dialog == ActiveDialog::quest_line)
            draw_quest_line_dialog(host);
        if (!host.windows_mouse_pointer && host.mouse_position_known && host.window)
            tibia::client::draw_game_mouse_cursor(*host.renderer, host.mouse.x,
                host.mouse.y, ui_units(host.window, 16));
        (void)host.renderer->end_frame();
        return;
    }
    const float background_scale_x = static_cast<float>(client_width) / 1280.0F;
    const float background_scale_y = static_cast<float>(client_height) / 720.0F;
    for (const auto& tile : host.tiles) {
        const int left = static_cast<int>(std::lround(tile.x * 2.0F * background_scale_x));
        const int top = static_cast<int>(std::lround(tile.y * 1.5F * background_scale_y));
        const int right = static_cast<int>(std::lround(
            (tile.x + 32) * 2.0F * background_scale_x));
        const int bottom = static_cast<int>(std::lround(
            (tile.y + 32) * 1.5F * background_scale_y));
        const int width = right - left;
        const int height = bottom - top;
        host.renderer->draw(tile.texture, left, top, width, height);
    }
    if (host.window) {
        const auto menu = menu_rect(host.window);
        draw_menu_frame(host, menu);
        constexpr std::array<std::wstring_view, 5> labels{
            L"Enter Game", L"Access Account", L"Options", L"Info", L"Exit Game"};
        const auto scale = ui_scale(host.window);
        const int button_x = menu.left + static_cast<int>(std::lround(15.0F * scale));
        const int button_y = menu.top + static_cast<int>(std::lround(15.0F * scale));
        const int button_w = static_cast<int>(std::lround(86.0F * scale));
        const int button_h = static_cast<int>(std::lround(20.0F * scale));
        for (std::size_t i = 0; i < labels.size(); ++i) {
            const int y = button_y + static_cast<int>(i) * static_cast<int>(std::lround(30.0F * scale));
            draw_skin(host, host.pressed_menu_index == static_cast<int>(i) ? 19 : 18,
                button_x, y, button_w, button_h);
            draw_bitmap_text(host, labels[i], button_x,
                y + static_cast<int>(std::lround(6.0F * scale)), button_w, true);
        }
        if (host.screen == UiScreen::main_menu && visible_dialog == ActiveDialog::options) {
            const auto dialog = options_rect(host.window, host.options_dialog_offset);
            draw_dialog_frame(host, dialog);
            draw_options_controls(host, dialog);
        }
        if (host.screen == UiScreen::main_menu && visible_dialog == ActiveDialog::info)
            draw_info_dialog(host);
        if (visible_dialog == ActiveDialog::help) draw_help_dialog(host);
        if (visible_dialog == ActiveDialog::hints) draw_hints_dialog(host);
        if (visible_dialog == ActiveDialog::hint_content) draw_hint_content_dialog(host);
        if (visible_dialog == ActiveDialog::report_violation)
            draw_report_violation_dialog(host);
        if (visible_dialog == ActiveDialog::rule_violation_request)
            draw_rule_violation_request_dialog(host);
        if (host.screen == UiScreen::main_menu &&
            visible_dialog == ActiveDialog::console_options)
            draw_console_options_page(host);
        if (host.screen == UiScreen::main_menu &&
            visible_dialog == ActiveDialog::hotkey_options)
            draw_hotkey_options_page(host);
        if (host.screen == UiScreen::main_menu &&
            visible_dialog == ActiveDialog::general_options)
            draw_general_options_page(host);
        if (host.screen == UiScreen::main_menu &&
            visible_dialog == ActiveDialog::network_options)
            draw_network_options_page(host);
        if (host.screen == UiScreen::main_menu &&
            visible_dialog == ActiveDialog::graphics_options)
            draw_graphics_options_page(host);
        if (host.screen == UiScreen::main_menu &&
            visible_dialog == ActiveDialog::advanced_graphics_options)
            draw_advanced_graphics_options_page(host);
    }
    if (host.window && host.screen == UiScreen::login &&
        visible_dialog == ActiveDialog::none) {
        draw_login_screen(host);
    } else if (host.window && host.screen == UiScreen::character_selection &&
        visible_dialog == ActiveDialog::none) {
        draw_character_screen(host);
    }
    if (host.window && visible_dialog == ActiveDialog::motd) draw_motd_dialog(host);
    if (host.window && visible_dialog == ActiveDialog::account_data_warning)
        draw_account_data_warning_dialog(host);
    if (host.window && visible_dialog == ActiveDialog::notice) draw_notice(host);
    if (host.window && visible_dialog == ActiveDialog::connection_status)
        draw_connection_status(host);
    if (!host.windows_mouse_pointer && host.mouse_position_known && host.window)
        tibia::client::draw_game_mouse_cursor(*host.renderer, host.mouse.x,
            host.mouse.y, ui_units(host.window, 16));
    (void)host.renderer->end_frame();
}

LRESULT CALLBACK window_proc_impl(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* host = reinterpret_cast<ClientHost*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_NCCREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* create_host = static_cast<ClientHost*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_host));
        if (create_host) create_host->window = window;
        return DefWindowProcW(window, message, wparam, lparam);
    }
    case WM_CREATE:
        SetTimer(window, game_cycle_timer_id, 100, nullptr);
        if (host) {
            host->account_label = CreateWindowExW(0, L"STATIC", L"Account number:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->account_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(1001)), nullptr, nullptr);
            host->password_label = CreateWindowExW(0, L"STATIC", L"Password:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->password_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(1002)), nullptr, nullptr);
            SendMessageW(host->password_edit, EM_LIMITTEXT, 29, 0);
            host->header_label = CreateWindowExW(0, L"STATIC", L"Tibia 7.92  |  Account Login",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->login_button = CreateWindowExW(0, L"BUTTON", L"Login",
                WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(login_button_id)), nullptr, nullptr);
            host->character_heading = CreateWindowExW(0, L"STATIC", L"Select character:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->character_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(1005)), nullptr, nullptr);
            host->account_status_label = CreateWindowExW(0, L"STATIC", L"Account Status: awaiting login",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->connect_button = CreateWindowExW(0, L"BUTTON", L"Connect",
                WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(1006)), nullptr, nullptr);
            host->back_button = CreateWindowExW(0, L"BUTTON", L"Back",
                WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(back_button_id)), nullptr, nullptr);
            EnableWindow(host->connect_button, FALSE);
            refresh_character_choices(*host);
            host->validation_label = CreateWindowExW(0, L"STATIC", host->resource_summary.c_str(),
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->ui_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
            host->ui_title_font = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
            host->ui_edit_brush = CreateSolidBrush(RGB(24, 25, 27));
            for (const auto control : {host->account_label, host->account_edit,
                host->password_label, host->password_edit,
                host->login_button, host->character_heading,
                host->character_list, host->account_status_label, host->connect_button,
                host->back_button, host->validation_label})
                if (control) SendMessageW(control, WM_SETFONT,
                    reinterpret_cast<WPARAM>(host->ui_font), TRUE);
            if (host->header_label) SendMessageW(host->header_label, WM_SETFONT,
                reinterpret_cast<WPARAM>(host->ui_title_font), TRUE);
            for (std::size_t i = 0; i < menu_button_ids.size(); ++i) {
                constexpr std::array<const wchar_t*, 5> labels{
                    L"Enter Game", L"Access Account", L"Options", L"Info", L"Exit Game"};
                host->menu_buttons[i] = CreateWindowExW(0, L"BUTTON", labels[i],
                    WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(menu_button_ids[i])), nullptr, nullptr);
            }
            host->options_heading = CreateWindowExW(0, L"STATIC", L"Advanced Graphics Options",
                WS_CHILD | SS_CENTER, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->engine_label = CreateWindowExW(0, L"STATIC", L"Select graphics engine:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            constexpr std::array<const wchar_t*, 3> engine_names{L"DirectX5", L"DirectX9", L"OpenGL"};
            constexpr std::array<int, 3> engine_ids{engine_directdraw_id, engine_d3d9_id, engine_opengl_id};
            for (std::size_t i = 0; i < host->engine_radios.size(); ++i) {
                host->engine_radios[i] = CreateWindowExW(0, L"BUTTON", engine_names[i],
                    WS_CHILD | WS_TABSTOP | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0),
                    0, 0, 0, 0, window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(engine_ids[i])), nullptr, nullptr);
            }
            host->ambient_label = CreateWindowExW(0, L"STATIC", L"Set ambient light: 100 %",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->ambient_slider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | TBS_HORZ | TBS_AUTOTICKS, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ambient_slider_id)), nullptr, nullptr);
            SendMessageW(host->ambient_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 128));
            SendMessageW(host->ambient_slider, TBM_SETPOS, TRUE, 100);
            host->light_effects_check = CreateWindowExW(0, L"BUTTON", L"Show Light Effects",
                WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(light_effects_id)), nullptr, nullptr);
            host->stretch_check = CreateWindowExW(0, L"BUTTON", L"Don't Stretch/Shrink Game Window",
                WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(stretch_game_id)), nullptr, nullptr);
            host->pointer_check = CreateWindowExW(0, L"BUTTON", L"Windows Mouse Pointer",
                WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(pointer_id)), nullptr, nullptr);
            SendMessageW(host->light_effects_check, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(host->pointer_check, BM_SETCHECK, BST_CHECKED, 0);
            host->framerate_label = CreateWindowExW(0, L"STATIC", L"Adjust framerate limit: 60",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->framerate_slider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | TBS_HORZ | TBS_AUTOTICKS, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(framerate_slider_id)), nullptr, nullptr);
            SendMessageW(host->framerate_slider, TBM_SETRANGE, TRUE, MAKELPARAM(10, 201));
            SendMessageW(host->framerate_slider, TBM_SETPOS, TRUE, 60);
            host->current_framerate_label = CreateWindowExW(0, L"STATIC", L"Current framerate: --",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            host->options_help_button = CreateWindowExW(0, L"BUTTON", L"Help", WS_CHILD | WS_TABSTOP,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(options_help_id)), nullptr, nullptr);
            host->options_ok_button = CreateWindowExW(0, L"BUTTON", L"Ok", WS_CHILD | WS_TABSTOP,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(options_ok_id)), nullptr, nullptr);
            host->options_cancel_button = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_TABSTOP,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(options_cancel_id)), nullptr, nullptr);
            host->options_backend = host->backend;
            for (std::size_t i = 0; i < engine_ids.size(); ++i) {
                const auto selected = (i == 0 && host->backend == tibia::render::GraphicsBackend::direct_draw) ||
                    (i == 1 && host->backend == tibia::render::GraphicsBackend::direct3d9) ||
                    (i == 2 && host->backend == tibia::render::GraphicsBackend::open_gl);
                if (selected) SendMessageW(host->engine_radios[i], BM_SETCHECK, BST_CHECKED, 0);
            }
            host->ui_menu_font = CreateFontW(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
            for (const auto control : {host->options_heading, host->engine_label, host->ambient_label,
                host->light_effects_check, host->stretch_check, host->pointer_check,
                host->framerate_label, host->current_framerate_label, host->options_help_button,
                host->options_ok_button, host->options_cancel_button})
                if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(host->ui_font), TRUE);
            for (const auto control : host->engine_radios)
                if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(host->ui_font), TRUE);
            for (const auto control : host->menu_buttons)
                if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(host->ui_menu_font), TRUE);
            show_screen(*host, UiScreen::main_menu);
            layout_controls(*host);
            SetFocus(window);
        }
        return 0;
    case WM_SETCURSOR:
        if (host && LOWORD(lparam) == HTCLIENT) {
            SetCursor(host->windows_mouse_pointer ? LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)) : nullptr);
            return TRUE;
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (host) {
            auto dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, RGB(235, 220, 180));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
        }
        break;
    case WM_DRAWITEM:
        if (host) {
            const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
            if (item && item->CtlType == ODT_BUTTON) {
                const auto id = static_cast<int>(item->CtlID);
                if (std::find(menu_button_ids.begin(), menu_button_ids.end(), id) != menu_button_ids.end()) {
                    (void)item;
                    return TRUE;
                }
            }
        }
        break;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
        if (host) {
            auto dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, RGB(240, 236, 218));
            SetBkColor(dc, RGB(24, 25, 27));
            return reinterpret_cast<LRESULT>(host->ui_edit_brush);
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wparam) == login_button_id) {
            const auto password_length = GetWindowTextLengthW(host->password_edit);
            const auto account = parse_account_number(host->account_edit);
            if (!account) {
                show_notice(*host, L"Tibia",
                    L"Account number must contain decimal digits and fit the 32-bit login field.");
            } else if (password_length == 0) {
                show_notice(*host, L"Tibia", L"Enter a password.");
            } else if (password_length > 29) {
                show_notice(*host, L"Tibia", L"Password must be shorter than 30 characters.");
            } else {
                try {
                    host->prepared_login = prepare_login(*host, host->password_edit, *account);
                    host->initial_wire_request = tibia::network::frame_initial_login_request(
                        host->prepared_login->request);
                    start_login_transport(*host, window);
                } catch (const std::exception& error) {
                    const auto message = error_to_wide(error.what());
                    show_notice(*host, L"Tibia", message);
                }
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && HIWORD(wparam) == BN_CLICKED) {
            const auto id = LOWORD(wparam);
            if (id == menu_button_ids[0]) {
                show_screen(*host, UiScreen::login);
                SetFocus(window);
                return 0;
            }
            if (id == menu_button_ids[1]) {
                tibia::client::open_account_management_page();
                return 0;
            }
            if (id == menu_button_ids[2]) {
                host->options_open = true;
                host->options_backend = host->backend;
                show_screen(*host, UiScreen::main_menu);
                return 0;
            }
            if (id == menu_button_ids[3]) {
                host->info_open = true;
                host->info_dialog_offset = {};
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (id == menu_button_ids[4]) { PostMessageW(window, WM_CLOSE, 0, 0); return 0; }
            if (id == options_cancel_id) { host->options_open = false; show_screen(*host, UiScreen::main_menu); return 0; }
            if (id == options_help_id) {
                host->notice_text = L"Choose a graphics engine and adjust the display options. Graphics engine changes apply after restart.";
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (id == options_ok_id) {
                const auto previous_backend = host->renderer->backend();
                try {
                    auto preferences = tibia::client::read_communication_preferences(
                        host->communication_config_path);
                    activate_graphics_backend(*host, host->options_backend);
                    preferences.engine = host->options_backend ==
                        tibia::render::GraphicsBackend::direct_draw ? 0 :
                        host->options_backend == tibia::render::GraphicsBackend::open_gl ? 1 : 2;
                    tibia::client::write_communication_preferences(
                        host->communication_config_path, preferences);
                } catch (const std::exception& error) {
                    if (host->renderer->backend() != previous_backend) {
                        try { activate_graphics_backend(*host, previous_backend); }
                        catch (...) {}
                    }
                    host->notice_text = error_to_wide(error.what());
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
                host->options_open = false;
                show_screen(*host, UiScreen::main_menu);
                host->notice_text = std::wstring(L"Active renderer: ") +
                    graphics_backend_name(host->renderer->backend());
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (id >= engine_directdraw_id && id <= engine_opengl_id) {
                host->options_backend = id == engine_directdraw_id ? tibia::render::GraphicsBackend::direct_draw :
                    id == engine_opengl_id ? tibia::render::GraphicsBackend::open_gl : tibia::render::GraphicsBackend::direct3d9;
                return 0;
            }
        }
        if (host && HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == light_effects_id) {
            host->show_light_effects = SendMessageW(host->light_effects_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
            return 0;
        }
        if (host && HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == stretch_game_id) {
            host->no_stretching = SendMessageW(host->stretch_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
            return 0;
        }
        if (host && HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == pointer_id) {
            host->windows_mouse_pointer = SendMessageW(host->pointer_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
            return 0;
        }
        if (LOWORD(wparam) == 1005 && HIWORD(wparam) == LBN_SELCHANGE && host) {
            host->selected_character = static_cast<int>(SendMessageW(host->character_list, LB_GETCURSEL, 0, 0));
            EnableWindow(host->connect_button, host->selected_character >= 0 &&
                static_cast<std::size_t>(host->selected_character) < host->characters.size());
            return 0;
        }
        if (LOWORD(wparam) == 1006 && host && host->selected_character >= 0) {
            const auto& selected = host->characters[static_cast<std::size_t>(host->selected_character)];
            const auto password_length = GetWindowTextLengthW(host->password_edit);
            const auto account = parse_account_number(host->account_edit);
            if (!account) {
                SetWindowTextW(host->validation_label,
                    L"Account number must contain decimal digits and fit the 32-bit login field.");
            } else if (password_length == 0 || password_length > 29) {
                SetWindowTextW(host->validation_label,
                    L"Enter a password shorter than 30 characters before connecting.");
            } else {
                try {
                    auto prepared = prepare_game_login(*host, host->password_edit, *account, selected);
                    start_game_transport(*host, window, selected, std::move(prepared));
                } catch (const std::exception& error) {
                    const auto message = error_to_wide(error.what());
                    SetWindowTextW(host->validation_label, message.c_str());
                }
            }
            return 0;
        }
        if (LOWORD(wparam) == back_button_id && host) {
            show_screen(*host, UiScreen::login);
            SetFocus(window);
            return 0;
        }
    case login_result_message:
        if (host) {
            std::optional<LoginUiEvent> event;
            {
                const std::lock_guard lock(host->login_mailbox->mutex);
                event.swap(host->login_mailbox->pending);
            }
            if (event) {
                if (event->finished || event->message_of_the_day ||
                    (!event->game_session && !event->status.empty()))
                    host->connection_status_text.clear();
                else if (event->game_session)
                    host->connection_status_text = L"Entering the game...";
                if (event->characters) apply_character_list_response(*host, *event->characters);
                else if (event->message_of_the_day) {
                    host->motd_text = event->status;
                    host->motd_open = true;
                    host->pressed_motd_button = -1;
                    SetWindowTextW(host->validation_label, L"");
                } else if (!event->game_session && !event->status.empty()) {
                    host->notice_title = event->title.empty() ? L"Tibia" : event->title;
                    host->notice_text = event->status;
                    host->notice_dialog_offset = {};
                    host->pressed_notice_button = -1;
                } else if (!event->status.empty()) {
                    if (!event->game_session)
                        host->connection_status_text = event->status;
                }
                if (event->finished) {
                    EnableWindow(event->game_session || !event->title.empty()
                        ? host->connect_button : host->login_button, TRUE);
                }
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        break;
    case game_world_message:
        if (host) {
            host->connection_status_text.clear();
            const auto snapshot = host->game_world_mailbox->read();
            if (snapshot && snapshot->map &&
                tibia::client::resolve_map_window_camera_offset(*snapshot)) {
                if (host->screen != UiScreen::game_world)
                    show_screen(*host, UiScreen::game_world);
                else
                    InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        break;
    case channel_list_message:
        if (host && host->communication_session_mailbox) {
            host->available_channels =
                host->communication_session_mailbox->available_channels();
            if (!host->available_channels.empty()) {
                host->new_channel_dialog_open = true;
                host->new_channel_selected = -1;
                host->new_channel_scroll = 0;
                host->private_channel_name.clear();
                host->private_channel_focused = false;
                host->pressed_new_channel_button = -1;
                host->pressed_new_channel_scroll_button = -1;
                host->new_channel_scrollbar_dragging = false;
                host->new_channel_dialog_offset = {};
                SetFocus(window);
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_HSCROLL:
        if (host && host->options_open) {
            const auto control = reinterpret_cast<HWND>(lparam);
            if (control == host->ambient_slider) {
                host->ambient_light = static_cast<int>(SendMessageW(control, TBM_GETPOS, 0, 0));
                const auto label = L"Set ambient light: " + std::to_wstring(host->ambient_light) + L" %";
                SetWindowTextW(host->ambient_label, label.c_str());
            } else if (control == host->framerate_slider) {
                host->framerate_limit = static_cast<int>(SendMessageW(control, TBM_GETPOS, 0, 0));
                const auto label = L"Adjust framerate limit: " + std::to_wstring(host->framerate_limit);
                SetWindowTextW(host->framerate_label, label.c_str());
            }
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        if (host) {
            try { render_frame(*host); }
            catch (...) { PostMessageW(window, WM_CLOSE, 0, 0); }
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_SIZE:
        if (host && host->renderer_initialized && LOWORD(lparam) && HIWORD(lparam)) {
            try {
                host->renderer->resize(LOWORD(lparam), HIWORD(lparam));
                layout_controls(*host);
                InvalidateRect(window, nullptr, FALSE);
            }
            catch (...) { PostMessageW(window, WM_CLOSE, 0, 0); }
        }
        return 0;
    case WM_TIMER:
        if (host && wparam == game_cycle_timer_id) {
            ++host->animation_clock;
            return 0;
        }
        if (host && wparam == login_caret_timer_id &&
            (host->screen == UiScreen::login || host->hotkey_options_open ||
                host->report_violation_dialog.open ||
                host->rule_violation_request_form.open)) {
            const int phase = static_cast<int>((GetTickCount64() / 500U) & 1U);
            if (phase != host->login_caret_phase) {
                host->login_caret_phase = phase;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        if (host && wparam == game_frame_timer_id && host->screen == UiScreen::game_world) {
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_GETMINMAXINFO:
        if (auto* limits = reinterpret_cast<MINMAXINFO*>(lparam)) {
            // Original FUN_004bb7e0 sets a 550x450 minimum client area.
            // FUN_004bb3a0 expands it by the current non-client frame size.
            RECT client{};
            RECT outer{};
            if (GetClientRect(window, &client) && GetWindowRect(window, &outer)) {
                const int frame_width = static_cast<int>(outer.right - outer.left) -
                    static_cast<int>(client.right - client.left);
                const int frame_height = static_cast<int>(outer.bottom - outer.top) -
                    static_cast<int>(client.bottom - client.top);
                limits->ptMinTrackSize.x = 550 + frame_width;
                limits->ptMinTrackSize.y = 450 + frame_height;
            }
            return 0;
        }
        break;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (host && wparam < host->keys.size()) host->keys[wparam] = true;
        if (wparam == VK_ESCAPE) { SendMessageW(window, escape_ui_message, 0, 0); return 0; }
        if (host && host->screen == UiScreen::game_world &&
            active_dialog(*host) == ActiveDialog::none &&
            !host->console_input_focused &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
            (wparam == 'S' || wparam == 'B' || wparam == 'P')) {
            const auto type = wparam == 'S'
                ? tibia::client::ContentWindowType::skills
                : wparam == 'B'
                    ? tibia::client::ContentWindowType::battle
                    : tibia::client::ContentWindowType::vip;
            auto& open = wparam == 'S'
                ? host->skills_window_open
                : wparam == 'B' ? host->battle_window_open : host->vip_window_open;
            const bool opening = !open;
            if (opening) remember_content_window_heights(*host, window);
            open = !open;
            if (open && std::find_if(host->restored_content_windows.begin(),
                    host->restored_content_windows.end(), [type](const auto& item) {
                        return item.type == type;
                    }) == host->restored_content_windows.end())
                host->restored_content_windows.push_back({type, 0});
            if (open) remember_content_window_height(*host, window, type);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->new_channel_dialog_open && wparam == VK_RETURN) {
            submit_new_channel_dialog(*host);
            return 0;
        }
        if (host && host->screen == UiScreen::game_world &&
            host->console_input_focused && wparam == VK_RETURN) {
            const bool report_tab_active = host->report_violation_buffer &&
                host->report_violation_buffer->is_open() &&
                host->report_violation_tab_selected;
            if (report_tab_active && !host->console_input_text.empty() &&
                host->send_report_violation_reply) {
                try {
                    host->send_report_violation_reply(host->console_input_text);
                    host->console_input_text.clear();
                    host->console_input_caret = 0;
                    host->console_input_anchor = 0;
                } catch (const std::exception& error) {
                    host->notice_text = error_to_wide(error.what());
                }
            } else if (!report_tab_active) {
                try {
                    submit_default_console_input(*host);
                } catch (const std::exception& error) {
                    host->notice_text = error_to_wide(error.what());
                }
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->hotkey_options_open &&
            (wparam == VK_UP || wparam == VK_DOWN)) {
            const auto next = wparam == VK_UP
                ? (host->selected_hotkey + tibia::client::hotkey_count - 1) %
                    tibia::client::hotkey_count
                : (host->selected_hotkey + 1) % tibia::client::hotkey_count;
            select_hotkey(*host, next);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->screen == UiScreen::game_world &&
            host->console_input_focused &&
            (wparam == VK_LEFT || wparam == VK_RIGHT || wparam == VK_HOME ||
                wparam == VK_END || wparam == VK_DELETE ||
                ((GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                    (wparam == 'A' || wparam == 'C' || wparam == 'V' ||
                        wparam == 'X' || wparam == VK_INSERT)) ||
                (wparam == VK_INSERT &&
                    (GetKeyState(VK_SHIFT) & 0x8000) != 0))) {
            host->keys[wparam] = false;
            const auto length = host->console_input_text.size();
            const auto selection_start = std::min(host->console_input_anchor,
                host->console_input_caret);
            const auto selection_end = std::max(host->console_input_anchor,
                host->console_input_caret);
            const bool control_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (control_down && wparam == 'A') {
                if (length == 0) {
                    select_all_console_transcript(*host);
                    host->console_input_anchor = 0;
                    host->console_input_caret = 0;
                } else {
                    host->console_input_anchor = 0;
                    host->console_input_caret = length;
                }
            } else if ((control_down && (wparam == 'C' ||
                wparam == VK_INSERT))) {
                if (copy_console_input_selection(*host, window) &&
                    wparam == VK_DELETE) {
                    host->console_input_text.erase(selection_start,
                        selection_end - selection_start);
                    host->console_input_caret = selection_start;
                    host->console_input_anchor = selection_start;
                }
            } else if ((control_down && wparam == 'V') ||
                (wparam == VK_INSERT &&
                    (GetKeyState(VK_SHIFT) & 0x8000) != 0)) {
                paste_console_input_text(*host, window);
            } else if (control_down && wparam == 'X') {
                (void)copy_console_input_selection(*host, window);
                host->console_input_text.erase(selection_start,
                    selection_end - selection_start);
                host->console_input_caret = selection_start;
                host->console_input_anchor = selection_start;
            } else if (wparam == VK_LEFT) {
                if (host->console_input_caret > 0)
                    --host->console_input_caret;
            } else if (wparam == VK_RIGHT) {
                if (host->console_input_caret < length)
                    ++host->console_input_caret;
            } else if (wparam == VK_HOME) {
                host->console_input_caret = 0;
            } else if (wparam == VK_END) {
                host->console_input_caret = length;
            } else if (selection_start != selection_end) {
                host->console_input_text.erase(selection_start,
                    selection_end - selection_start);
                host->console_input_caret = selection_start;
            } else if (host->console_input_caret < length) {
                host->console_input_text.erase(host->console_input_caret, 1);
            }
            host->console_input_anchor = host->console_input_caret;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->screen == UiScreen::game_world &&
            !host->console_input_focused &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == 'C') {
            (void)copy_console_transcript_selection(*host, window);
            return 0;
        }
        if (host && host->quest_line_open &&
            (wparam == VK_UP || wparam == VK_DOWN)) {
            const auto snapshot = host->game_world_mailbox->read();
            if (snapshot) {
                const auto line = std::find_if(snapshot->quest_lines.begin(),
                    snapshot->quest_lines.end(), [id = host->quest_line_id](const auto& entry) {
                        return entry.id == id;
                    });
                if (line != snapshot->quest_lines.end() && !line->flags.empty()) {
                    const int last = static_cast<int>(line->flags.size()) - 1;
                    host->quest_flag_selected = std::clamp(
                        host->quest_flag_selected + (wparam == VK_UP ? -1 : 1), 0, last);
                    host->quest_flag_scroll = tibia::client::quest_log_scroll_for_selection(
                        host->quest_flag_selected, host->quest_flag_scroll,
                        line->flags.size(), ui_units(window, 118),
                        ui_units(window, tibia::client::quest_log_row_height),
                        ui_units(window, tibia::client::quest_log_content_margin));
                }
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->quest_log_open &&
            (wparam == VK_UP || wparam == VK_DOWN)) {
            const auto snapshot = host->game_world_mailbox->read();
            if (snapshot && !snapshot->quest_log_entries.empty()) {
                const int last = static_cast<int>(snapshot->quest_log_entries.size()) - 1;
                host->quest_log_selected = std::clamp(
                    host->quest_log_selected + (wparam == VK_UP ? -1 : 1), 0, last);
                host->quest_log_scroll = tibia::client::quest_log_scroll_for_selection(
                    host->quest_log_selected, host->quest_log_scroll,
                    snapshot->quest_log_entries.size(), ui_units(window, 268),
                    ui_units(window, tibia::client::quest_log_row_height),
                    ui_units(window, tibia::client::quest_log_content_margin));
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->screen == UiScreen::game_world && wparam == 'M' &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
            host->communication_session_mailbox) {
            const auto channel_view =
                host->communication_session_mailbox->channel_view();
            host->communication_session_mailbox->request_server_messages_toggle(
                channel_view.selected_identifier);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->screen == UiScreen::game_world &&
            active_dialog(*host) == ActiveDialog::none &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == 'E') {
            (void)close_selected_console_channel(*host);
            return 0;
        }
        if (host && host->screen == UiScreen::game_world &&
            active_dialog(*host) == ActiveDialog::none &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == 'O') {
            if (host->send_game_payload) {
                try {
                    const auto payload =
                        tibia::network::encode_state6_channel_list_request();
                    host->send_game_payload(payload);
                } catch (const std::exception& error) {
                    show_notice(*host, L"Tibia", error_to_wide(error.what()));
                }
            }
            return 0;
        }
        if (host && host->screen == UiScreen::login) {
            if (wparam == VK_TAB) {
                host->login_focus_field = 1 - host->login_focus_field;
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (wparam == VK_RETURN) {
                SendMessageW(window, WM_COMMAND,
                    MAKEWPARAM(login_button_id, BN_CLICKED), 0);
                return 0;
            }
        }
        if (host && host->screen == UiScreen::character_selection &&
            wparam == VK_RETURN && host->selected_character >= 0) {
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(1006, BN_CLICKED), 0);
            return 0;
        }
        break;
    case WM_KEYUP: case WM_SYSKEYUP:
        if (host && wparam < host->keys.size()) host->keys[wparam] = false;
        break;
    case escape_ui_message:
        if (host) {
            if (!host->connection_status_text.empty()) {
                return 0;
            } else if (host->battle_context_menu) {
                host->battle_context_menu.reset();
                host->pressed_battle_context_menu_action = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->vip_context_menu) {
                host->vip_context_menu.reset();
                host->pressed_vip_context_menu_action = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->skills_context_menu) {
                host->skills_context_menu.reset();
                host->pressed_skills_context_menu_action = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->console_context_menu) {
                host->console_context_menu.reset();
                host->pressed_console_context_menu_action = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (!host->notice_text.empty()) {
                host->notice_text.clear();
                host->notice_title = L"Tibia";
                host->notice_dialog_offset = {};
                host->pressed_notice_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->motd_open) {
                host->motd_open = false;
                host->pressed_motd_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->account_data_warning_open) {
                host->account_data_warning_open = false;
                host->pressed_account_data_warning_button = -1;
                host->account_data_warning_dialog_offset = {};
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->new_channel_dialog_open) {
                host->new_channel_dialog_open = false;
                host->private_channel_name.clear();
                host->private_channel_focused = false;
                host->pressed_new_channel_button = -1;
                host->pressed_new_channel_scroll_button = -1;
                host->new_channel_scrollbar_dragging = false;
                host->new_channel_dialog_offset = {};
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->hint_content_open) {
                host->hint_content_open = false;
                host->active_hint = nullptr;
                host->pressed_hint_content_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->hints_open) {
                host->hints_open = false;
                host->pressed_hints_button = -1;
                host->hints_scroll_dragging = false;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->quest_line_open) {
                host->quest_line_open = false;
                host->pressed_quest_line_button = -1;
                host->pressed_quest_scroll_button = -1;
                host->quest_scroll_dragging = false;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->quest_log_open) {
                host->quest_log_open = false;
                host->pressed_quest_log_button = -1;
                host->pressed_quest_scroll_button = -1;
                host->quest_scroll_dragging = false;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->rule_violation_request_form.open) {
                host->rule_violation_request_form.close();
                KillTimer(window, login_caret_timer_id);
                host->rule_violation_request_focus_field = -1;
                host->pressed_rule_violation_request_button = -1;
                host->rule_violation_request_dialog_offset = {};
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->report_violation_dialog.open) {
                host->report_violation_dialog.close();
                KillTimer(window, login_caret_timer_id);
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->advanced_graphics_options_open) {
                host->framerate_limit = host->advanced_graphics_original_framerate;
                host->advanced_graphics_options_open = false;
                host->pressed_advanced_graphics_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->general_options_open) {
                host->general_options_open = false;
                host->pressed_general_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->network_options_open) {
                host->network_options_open = false;
                host->pressed_network_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->graphics_options_open) {
                host->graphics_options_open = false;
                host->pressed_graphics_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->console_options_open) {
                host->console_options_open = false;
                host->pressed_console_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->hotkey_options_open) {
                host->hotkey_options_open = false;
                host->hotkey_text_focused = false;
                host->pressed_hotkey_options_control = -1;
                KillTimer(window, login_caret_timer_id);
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->info_open) {
                host->info_open = false;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->help_open) {
                host->help_open = false;
                host->pressed_help_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->options_open) {
                host->options_open = false;
                if (host->options_from_game) host->options_from_game = false;
                else show_screen(*host, UiScreen::main_menu);
            } else if (host->screen == UiScreen::character_selection) {
                show_screen(*host, UiScreen::login);
                SetFocus(window);
            } else if (host->screen == UiScreen::login) {
                show_screen(*host, UiScreen::main_menu);
            } else if (host->screen == UiScreen::game_world) {
                return 0;
            } else {
                PostMessageW(window, WM_CLOSE, 0, 0);
            }
        }
        return 0;
    case WM_CHAR:
        if (host && host->rule_violation_request_form.open &&
            host->rule_violation_request_focus_field >= 0) {
            auto& form = host->rule_violation_request_form;
            std::wstring* field = nullptr;
            if (host->rule_violation_request_focus_field == 0)
                field = &form.player_name;
            else if (host->rule_violation_request_focus_field == 1 &&
                form.statement_enabled)
                field = &form.statement;
            else if (host->rule_violation_request_focus_field == 3)
                field = &form.comment;
            if (field) {
                if (wparam == VK_BACK) {
                    if (!field->empty()) field->pop_back();
                } else if (wparam >= 32 && wparam <= 0xffff) {
                    field->push_back(static_cast<wchar_t>(wparam));
                }
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        if (host && host->new_channel_dialog_open && host->private_channel_focused) {
            if (wparam == VK_BACK) {
                if (!host->private_channel_name.empty())
                    host->private_channel_name.pop_back();
            } else if (wparam >= 32 && wparam <= 0xffff &&
                host->private_channel_name.size() < 29) {
                host->private_channel_name.push_back(static_cast<wchar_t>(wparam));
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->report_violation_dialog.open &&
            host->report_violation_dialog.text_focused) {
            if (wparam == VK_BACK) {
                host->report_violation_dialog.erase_character();
            } else if (wparam >= 32 && wparam <= 0xffff &&
                host->report_violation_dialog.text.size() < 255) {
                host->report_violation_dialog.type(static_cast<wchar_t>(wparam));
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->screen == UiScreen::login) {
            auto& field = host->login_focus_field == 0 ? host->login_account_text : host->login_password_text;
            const std::size_t max_length = host->login_focus_field == 0 ? 8U : 29U;
            if (wparam == VK_BACK) {
                if (!field.empty()) field.pop_back();
            } else if (wparam >= 32 && wparam <= 0xffff && field.size() < max_length) {
                const auto character = static_cast<wchar_t>(wparam);
                if (host->login_focus_field != 0 || (character >= L'0' && character <= L'9'))
                    field.push_back(character);
            }
            SetWindowTextW(host->account_edit, host->login_account_text.c_str());
            SetWindowTextW(host->password_edit, host->login_password_text.c_str());
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->hotkey_options_open && host->hotkey_text_focused) {
            if (wparam == VK_BACK) {
                if (!host->hotkey_text.empty()) host->hotkey_text.pop_back();
            } else if (wparam >= 32 && wparam <= 0xffff && host->hotkey_text.size() < 255) {
                host->hotkey_text.push_back(static_cast<wchar_t>(wparam));
            }
            commit_hotkey_editor(*host);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && host->screen == UiScreen::game_world &&
            host->console_input_focused) {
            const auto selection_start = std::min(host->console_input_anchor,
                host->console_input_caret);
            const auto selection_end = std::max(host->console_input_anchor,
                host->console_input_caret);
            if (wparam == VK_BACK) {
                if (selection_start != selection_end) {
                    host->console_input_text.erase(selection_start,
                        selection_end - selection_start);
                    host->console_input_caret = selection_start;
                    host->console_input_anchor = selection_start;
                } else if (host->console_input_caret > 0 &&
                    host->console_input_caret <= host->console_input_text.size()) {
                    host->console_input_text.erase(host->console_input_caret - 1, 1);
                    --host->console_input_caret;
                    host->console_input_anchor = host->console_input_caret;
                }
            } else if (wparam >= 32 && wparam <= 255 &&
                host->console_input_text.size() -
                    (selection_end - selection_start) < 255) {
                if (selection_start != selection_end) {
                    host->console_input_text.erase(selection_start,
                        selection_end - selection_start);
                    host->console_input_caret = selection_start;
                    host->console_input_anchor = selection_start;
                }
                const auto position = std::min(host->console_input_caret,
                    host->console_input_text.size());
                host->console_input_text.insert(position, 1,
                    static_cast<wchar_t>(wparam));
                host->console_input_caret = position + 1;
                host->console_input_anchor = host->console_input_caret;
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (host && wparam >= 32 && wparam <= 0xffff) host->typed_text.push_back(static_cast<wchar_t>(wparam));
        break;
    case WM_MOUSEMOVE:
        if (host) {
            host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            host->mouse_position_known = true;
            if (!host->windows_mouse_pointer ||
                host->pressed_console_transcript_scrollbar_part == 0 ||
                host->pressed_console_transcript_scrollbar_part == 1)
                InvalidateRect(window, nullptr, FALSE);
            if (host->skills_context_menu) InvalidateRect(window, nullptr, FALSE);
            host->dragging = (wparam & MK_LBUTTON) != 0;
            if (host->console_transcript_selecting &&
                host->screen == UiScreen::game_world &&
                update_console_transcript_selection(*host, host->mouse)) {
                host->console_transcript_selection_active =
                    host->console_transcript_selection_start_row !=
                        host->console_transcript_selection_end_row ||
                    host->console_transcript_selection_start_offset !=
                        host->console_transcript_selection_end_offset;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->screen == UiScreen::game_world) {
                if (host->console_resize_dragging && host->dragging) {
                    RECT client{};
                    if (GetClientRect(window, &client)) {
                        const auto client_height = static_cast<int>(
                            client.bottom - client.top);
                        const auto maximum_height = std::max(
                            tibia::client::map_panel_height,
                            client_height - tibia::client::map_window_minimum_height -
                                tibia::client::map_window_bottom_margin);
                        host->console_height = std::clamp(
                            client_height - static_cast<int>(host->mouse.y) -
                                tibia::client::map_window_bottom_margin,
                            tibia::client::map_panel_height, maximum_height);
                        InvalidateRect(window, nullptr, FALSE);
                    }
                }
                if (host->skills_window_resize_dragging && host->dragging) {
                    RECT client{};
                    if (GetClientRect(window, &client)) {
                        const auto plan = skills_panel_window_plan(*host, window);
                        if (plan) {
                            const auto minimum_height =
                                tibia::client::content_window_frame_header_height;
                            const auto maximum_height = std::max(minimum_height,
                                static_cast<int>(client.bottom) - plan->window.y);
                            host->skills_window_resized_height = std::clamp(
                                host->skills_window_resize_start_height +
                                    static_cast<int>(host->mouse.y) -
                                    host->skills_window_resize_start_y,
                                minimum_height, maximum_height);
                            InvalidateRect(window, nullptr, FALSE);
                        }
                    }
                }
                if ((host->battle_window_resize_dragging ||
                        host->vip_window_resize_dragging) && host->dragging) {
                    RECT client{};
                    if (GetClientRect(window, &client)) {
                        const auto resize_window = [&](bool battle) {
                            const auto minimum_height =
                                tibia::client::content_window_frame_header_height +
                                (battle ? tibia::client::battle_panel_row_height
                                    : tibia::client::vip_panel_body_padding);
                            int origin_y{};
                            if (battle) {
                                if (const auto panel = battle_panel_window_plan(*host, window))
                                    origin_y = panel->window.y;
                            } else if (const auto panel = vip_panel_window_plan(*host, window)) {
                                origin_y = panel->window.y;
                            }
                            if (origin_y == 0) return;
                            const auto maximum_height = std::max(minimum_height,
                                static_cast<int>(client.bottom) - origin_y);
                            const auto height = std::clamp(
                                (battle ? host->battle_window_resize_start_height
                                    : host->vip_window_resize_start_height) +
                                    static_cast<int>(host->mouse.y) -
                                    (battle ? host->battle_window_resize_start_y
                                        : host->vip_window_resize_start_y),
                                minimum_height, maximum_height);
                            if (battle) host->battle_window_resized_height = height;
                            else host->vip_window_resized_height = height;
                            InvalidateRect(window, nullptr, FALSE);
                        };
                        if (host->battle_window_resize_dragging)
                            resize_window(true);
                        if (host->vip_window_resize_dragging)
                            resize_window(false);
                    }
                }
                if (host->moving_container_id >= 0) {
                    RECT client{};
                    if (GetClientRect(window, &client)) {
                        const auto snapshot = host->game_world_mailbox->read();
                        const auto game_layout = tibia::client::resolve_game_window_layout({
                            client.right - client.left, client.bottom - client.top,
                            host->console_height, tibia::client::map_panel_height,
                            tibia::client::map_window_minimum_height,
                            host->no_stretching});
                        if (snapshot && game_layout) {
                            const auto panels = container_panel_layouts(*host, window,
                                *snapshot, *game_layout);
                            const auto panel = std::find_if(panels.begin(), panels.end(),
                                [host](const auto& item) {
                                    return item.id == host->moving_container_id;
                                });
                            const auto state = std::find_if(
                                host->container_panel_states.begin(),
                                host->container_panel_states.end(), [host](const auto& item) {
                                    return item.id == host->moving_container_id;
                                });
                            if (panel != panels.end() && state !=
                                    host->container_panel_states.end()) {
                                const auto maximum_y = std::max(0,
                                    static_cast<int>(client.bottom) -
                                        panel->window.height);
                                state->position_y = std::clamp(
                                    host->container_move_start_position_y +
                                        static_cast<int>(host->mouse.y) -
                                        host->container_move_start_y,
                                    0, maximum_y);
                                InvalidateRect(window, nullptr, FALSE);
                            }
                        }
                    }
                }
                if (host->resizing_container_id >= 0) {
                    RECT client{};
                    if (GetClientRect(window, &client)) {
                        const auto snapshot = host->game_world_mailbox->read();
                        const auto game_layout = tibia::client::resolve_game_window_layout({
                            client.right - client.left, client.bottom - client.top,
                            host->console_height, tibia::client::map_panel_height,
                            tibia::client::map_window_minimum_height,
                            host->no_stretching});
                        if (snapshot && game_layout) {
                            const auto panels = container_panel_layouts(*host, window,
                                *snapshot, *game_layout);
                            const auto panel = std::find_if(panels.begin(), panels.end(),
                                [host](const auto& item) {
                                    return item.id == host->resizing_container_id;
                                });
                            const auto state = std::find_if(
                                host->container_panel_states.begin(),
                                host->container_panel_states.end(), [host](const auto& item) {
                                    return item.id == host->resizing_container_id;
                                });
                            if (panel != panels.end() && state !=
                                    host->container_panel_states.end()) {
                                state->height = std::clamp(
                                    host->container_resize_start_height +
                                        static_cast<int>(host->mouse.y) -
                                        host->container_resize_start_y,
                                    tibia::client::container_panel_minimum_height,
                                    panel->maximum_height);
                                state->restore_height = state->height;
                                InvalidateRect(window, nullptr, FALSE);
                            }
                        }
                    }
                }
                const auto hovered = battle_panel_creature_at(
                    *host, window, host->mouse);
                if (hovered != host->battle_targets.hovered_creature) {
                    host->battle_targets.hovered_creature = hovered;
                    InvalidateRect(window, nullptr, FALSE);
                }
                if (host->battle_context_menu) InvalidateRect(window, nullptr, FALSE);
            }
            if (host->skills_scrollbar_dragging && host->dragging) {
                if (const auto plan = skills_panel_window_plan(*host, window)) {
                    const auto& range = plan->skills.scroll_range;
                    const auto& geometry = plan->skills.scrollbar;
                    host->skills_scroll_value =
                        tibia::client::skills_panel_scroll_after_thumb_drag(
                            range, geometry.track.height, geometry.thumb.height,
                            host->skills_scrollbar_grab_offset,
                            host->mouse.y - geometry.track.y);
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
            if (host->battle_scrollbar_dragging && host->dragging) {
                if (const auto plan = battle_panel_window_plan(*host, window)) {
                    const auto& geometry = plan->scrollbar;
                    const auto travel = geometry.track.height - geometry.thumb.height;
                    if (travel > 0 && plan->panel.scroll_range > 0) {
                        const auto relative = std::clamp(
                            static_cast<int>(host->mouse.y) -
                                host->battle_scrollbar_grab_offset -
                                geometry.track.y,
                            0, travel);
                        host->battle_scroll_value = static_cast<int>(
                            static_cast<std::int64_t>(relative) *
                            plan->panel.scroll_range / travel);
                    }
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
            if (host->vip_scrollbar_dragging && host->dragging) {
                if (const auto plan = vip_panel_window_plan(*host, window)) {
                    host->vip_scroll_value =
                        tibia::client::skills_panel_scroll_after_thumb_drag(
                            plan->scroll, plan->scrollbar.track.height,
                            plan->scrollbar.thumb.height,
                            host->vip_scrollbar_grab_offset,
                            host->mouse.y - plan->scrollbar.track.y);
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
            if (host->screen == UiScreen::game_world &&
                host->console_input_focused && host->dragging) {
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height, tibia::client::map_window_minimum_height,
                    host->no_stretching});
                if (layout && layout->console_panel) {
                    const auto input =
                        tibia::client::resolve_console_text_input_visual(
                            *layout->console_panel, layout->console.x,
                            layout->console.y, true, true);
                    if (input) {
                        const auto text = std::wstring_view(
                            host->console_input_text).substr(0,
                                input->max_length);
                        host->console_input_caret = console_input_caret_for_x(
                            *host, text, input->font_index, window,
                            host->mouse.x - input->rect.x - input->inset);
                    }
                }
            }
            if (host->hints_scroll_dragging && host->dragging) {
                const auto list = hints_list_rect(window, host->hints_dialog_offset);
                const int line_height = ui_units(window,
                    std::max(1, host->ui_font_heights[2]));
                const int visible = std::max(1,
                    static_cast<int>(list.bottom - list.top) / line_height);
                const auto geometry = options_list_scrollbar_geometry(window, list,
                    host->hints.size(), visible, host->hints_scroll);
                host->hints_scroll = tibia::client::ui_scrollbar_scroll_after_thumb_drag(
                    host->mouse.y - list.top, host->hints_scroll_grab_offset, geometry,
                    host->hints.size(), visible);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->hint_content_scroll_dragging && host->dragging) {
                const auto view = hint_content_view_rect(window,
                    host->hint_content_dialog_offset);
                const int visible = hint_content_visible_lines(*host, view);
                const auto geometry = options_list_scrollbar_geometry(window, view,
                    host->hint_content_lines.size(), visible,
                    host->hint_content_scroll);
                host->hint_content_scroll = tibia::client::ui_scrollbar_scroll_after_thumb_drag(
                    host->mouse.y - view.top, host->hint_content_scroll_grab_offset,
                    geometry, host->hint_content_lines.size(), visible);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->quest_scroll_dragging && host->dragging) {
                update_quest_scroll_drag(*host, host->mouse);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->list_scrollbar_dragging && host->dragging) {
                update_options_list_scrollbar_drag(*host);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->console_transcript_scrollbar_dragging &&
                host->screen == UiScreen::game_world &&
                host->console_transcript_scrollbar_geometry_valid) {
                const int maximum = std::max(0,
                    host->console_transcript_item_count -
                        host->console_transcript_visible_rows);
                const auto& viewport = host->console_transcript_scrollbar_viewport;
                const int first_visible = std::clamp(maximum -
                    host->console_transcript_scroll_from_bottom, 0, maximum);
                const auto geometry = tibia::client::resolve_console_message_scrollbar_geometry(
                    static_cast<int>(viewport.right - viewport.left),
                    static_cast<int>(viewport.bottom - viewport.top),
                    host->console_transcript_item_count,
                    host->console_transcript_visible_rows, first_visible);
                const int travel = geometry.track.height - geometry.thumb.height;
                const int pointer_y = static_cast<int>(host->mouse.y) - viewport.top;
                const int relative = std::clamp(pointer_y -
                    host->console_transcript_scrollbar_grab_offset -
                    geometry.track.y, 0, std::max(0, travel + 1));
                const int first = travel > 0
                    ? static_cast<int>(static_cast<std::int64_t>(relative) *
                        (static_cast<std::int64_t>(maximum) + 1) /
                        (static_cast<std::int64_t>(travel) + 1))
                    : 0;
                host->console_transcript_scroll_from_bottom = maximum -
                    std::clamp(first, 0, maximum);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->new_channel_scrollbar_dragging && host->dragging) {
                const auto list = new_channel_list_rect(*host);
                const int margin = ui_units(window, 3);
                const int visible_rows = std::max(1,
                    (static_cast<int>(list.bottom - list.top) - 2 * margin) /
                        ui_units(window, std::max(1, host->ui_font_heights[3])));
                const int width = static_cast<int>(list.right - list.left);
                const int height = static_cast<int>(list.bottom - list.top);
                const auto geometry = tibia::client::ui_scrollbar_geometry(
                    width, height, host->available_channels.size(), visible_rows,
                    host->new_channel_scroll, margin, ui_units(window, 12),
                    ui_units(window, 12), ui_units(window, 6));
                host->new_channel_scroll = tibia::client::ui_scrollbar_scroll_after_thumb_drag(
                    host->mouse.y - list.top, host->new_channel_scrollbar_grab_offset,
                    geometry, host->available_channels.size(), visible_rows);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->dragged_dialog != ClientHost::DraggedDialog::none && host->dragging) {
                RECT base{};
                POINT* offset{};
                if (host->dragged_dialog == ClientHost::DraggedDialog::general_options) {
                    base = general_options_dialog_rect(window);
                    offset = &host->general_options_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::advanced_graphics_options) {
                    base = advanced_graphics_options_dialog_rect(window);
                    offset = &host->advanced_graphics_options_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::graphics_options) {
                    base = graphics_options_dialog_rect(window);
                    offset = &host->graphics_options_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::network_options) {
                    base = network_options_dialog_rect(window);
                    offset = &host->network_options_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::login) {
                    base = login_dialog_rect(window);
                    offset = &host->login_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::character_selection) {
                    base = character_selection_dialog_rect(window);
                    offset = &host->character_selection_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::options) {
                    base = options_rect(window);
                    offset = &host->options_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::console_options) {
                    base = console_options_dialog_rect(window);
                    offset = &host->console_options_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::hotkey_options) {
                    base = hotkey_options_dialog_rect(window);
                    offset = &host->hotkey_options_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::notice) {
                    base = notice_rect(*host);
                    offset = &host->notice_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::new_channel) {
                    base = new_channel_dialog_rect(*host);
                    offset = &host->new_channel_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::motd) {
                    base = motd_dialog_rect(*host);
                    offset = &host->motd_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::account_data_warning) {
                    base = account_data_warning_dialog_rect(*host);
                    offset = &host->account_data_warning_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::help) {
                    base = help_dialog_rect(window);
                    offset = &host->help_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::hints) {
                    base = hints_dialog_rect(window);
                    offset = &host->hints_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::hint_content) {
                    base = hint_content_dialog_rect(window);
                    offset = &host->hint_content_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::report_rule_violation) {
                    base = report_violation_dialog_rect(window);
                    offset = &host->report_rule_violation_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::rule_violation_request) {
                    base = rule_violation_request_dialog_rect(window);
                    offset = &host->rule_violation_request_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::quest_log) {
                    base = quest_log_dialog_rect(window);
                    offset = &host->quest_log_dialog_offset;
                } else if (host->dragged_dialog == ClientHost::DraggedDialog::quest_line) {
                    base = quest_line_dialog_rect(window);
                    offset = &host->quest_line_dialog_offset;
                } else {
                    base = info_dialog_rect(window);
                    offset = &host->info_dialog_offset;
                }
                const POINT proposed{
                    host->dialog_drag_start_offset.x + host->mouse.x - host->dialog_drag_anchor.x,
                    host->dialog_drag_start_offset.y + host->mouse.y - host->dialog_drag_anchor.y};
                const auto clamped = clamp_dialog_rect(window, base, proposed);
                *offset = {clamped.left - base.left, clamped.top - base.top};
                InvalidateRect(window, nullptr, FALSE);
            }
            if (host->advanced_graphics_options_open && host->dragging &&
                (host->pressed_advanced_graphics_control == 3 ||
                    host->pressed_advanced_graphics_control == 7)) {
                update_advanced_graphics_slider(*host,
                    host->pressed_advanced_graphics_control, host->mouse.x);
                InvalidateRect(window, nullptr, FALSE);
            }
        }
        break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        if (host) {
            const bool notice_open = !host->notice_text.empty();
            const bool modal_open = !host->connection_status_text.empty() ||
                notice_open || host->motd_open ||
                host->account_data_warning_open || host->new_channel_dialog_open ||
                host->help_open ||
                host->hints_open || host->hint_content_open ||
                host->rule_violation_request_form.open ||
                host->report_violation_dialog.open ||
                host->quest_log_open || host->quest_line_open;
            const POINT mouse{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            host->mouse = mouse;
            if (host->battle_context_menu) {
                if (message == WM_LBUTTONDOWN) {
                    const auto* entry = battle_menu_entry_at(*host, mouse);
                    host->pressed_battle_context_menu_action = entry
                        ? static_cast<int>(entry->action) : -1;
                    if (entry) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                    } else {
                        host->battle_context_menu.reset();
                    }
                    InvalidateRect(window, nullptr, FALSE);
                    if (entry) return 0;
                } else if (message == WM_RBUTTONDOWN) {
                    host->battle_context_menu.reset();
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (host->vip_context_menu) {
                if (message == WM_LBUTTONDOWN) {
                    const auto* entry = vip_context_menu_entry_at(*host, mouse);
                    host->pressed_vip_context_menu_action = entry
                        ? static_cast<int>(entry->action) : -1;
                    if (entry) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                    } else {
                        host->vip_context_menu.reset();
                    }
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
                if (message == WM_RBUTTONDOWN) {
                    host->vip_context_menu.reset();
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (host->skills_context_menu) {
                if (message == WM_LBUTTONDOWN) {
                    const auto* entry = skills_menu_entry_at(*host, mouse);
                    host->pressed_skills_context_menu_action = entry
                        ? static_cast<int>(entry->action) : -1;
                    if (entry) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                    } else {
                        host->skills_context_menu.reset();
                    }
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
                if (message == WM_RBUTTONDOWN) {
                    host->skills_context_menu.reset();
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (host->console_context_menu) {
                if (message == WM_LBUTTONDOWN) {
                    const auto* item = console_context_menu_item_at(*host, mouse);
                    host->pressed_console_context_menu_action = item
                        ? static_cast<int>(item->action) : -1;
                    if (item) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                    } else {
                        host->console_context_menu.reset();
                    }
                    InvalidateRect(window, nullptr, FALSE);
                    if (item) return 0;
                } else if (message == WM_RBUTTONDOWN) {
                    host->console_context_menu.reset();
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (message == WM_RBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = vip_panel_window_plan(*host, window);
                    plan && contains_point(skills_rect(plan->window), mouse)) {
                    host->battle_context_menu.reset();
                    host->skills_context_menu.reset();
                    host->console_context_menu.reset();
                    open_vip_context_menu(*host, window, *plan, mouse);
                    return 0;
                }
                const auto creature_id = battle_panel_creature_at(
                    *host, window, mouse);
                if (creature_id != 0) {
                    host->skills_context_menu.reset();
                    open_battle_context_menu(*host, window, creature_id, mouse);
                    return 0;
                }
                if (const auto plan = skills_panel_window_plan(*host, window);
                    plan && contains_point(skills_rect(plan->window), mouse)) {
                    host->battle_context_menu.reset();
                    open_skills_context_menu(*host, window, mouse);
                    return 0;
                }
                if (contains_console_transcript_point(
                        host->console_transcript_clip, mouse)) {
                    const auto row = console_transcript_row_at(*host, mouse);
                    if (row >= 0) {
                        host->battle_context_menu.reset();
                        host->skills_context_menu.reset();
                        open_console_context_menu(*host, window, mouse, row);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                RECT client{};
                GetClientRect(window, &client);
                const auto snapshot = host->game_world_mailbox->read();
                const auto game_layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height,
                    tibia::client::map_window_minimum_height,
                    host->no_stretching});
                if (snapshot && game_layout) {
                    const auto panels = container_panel_layouts(*host, window,
                        *snapshot, *game_layout);
                    const auto as_rect = [](const auto& bounds) {
                        return RECT{bounds.x, bounds.y,
                            bounds.x + bounds.width, bounds.y + bounds.height};
                    };
                    for (auto panel = panels.rbegin(); panel != panels.rend(); ++panel) {
                        if (contains_point(as_rect(panel->minimize_button), mouse)) {
                            host->pressed_container_header_id = panel->id;
                            host->pressed_container_header_button = 1;
                        } else if (!panel->minimized &&
                            (contains_point(as_rect(panel->resize_hit_area), mouse) ||
                                contains_point(as_rect(panel->resize_grip), mouse))) {
                            host->resizing_container_id = panel->id;
                            host->container_resize_start_y = mouse.y;
                            host->container_resize_start_height = panel->window.height;
                            const auto state = std::find_if(
                                host->container_panel_states.begin(),
                                host->container_panel_states.end(),
                                [panel](const auto& item) {
                                    return item.id == panel->id;
                                });
                            if (state != host->container_panel_states.end())
                                state->z_order = ++host->next_container_z_order;
                        } else if (contains_point(as_rect(panel->frame.title_bar), mouse)) {
                            const bool on_control =
                                contains_point(as_rect(panel->higher_button), mouse) ||
                                contains_point(as_rect(panel->minimize_button), mouse) ||
                                contains_point(as_rect(panel->close_button), mouse);
                            if (!on_control) {
                                host->moving_container_id = panel->id;
                                host->container_move_start_y = mouse.y;
                                host->container_move_start_position_y = panel->window.y;
                                const auto state = std::find_if(
                                    host->container_panel_states.begin(),
                                    host->container_panel_states.end(),
                                    [panel](const auto& item) {
                                        return item.id == panel->id;
                                    });
                                if (state != host->container_panel_states.end())
                                    state->z_order = ++host->next_container_z_order;
                            }
                        }
                        if (host->pressed_container_header_id >= 0 ||
                            host->resizing_container_id >= 0 ||
                            host->moving_container_id >= 0) {
                            SetCapture(window);
                            host->mouse_buttons |= 1U;
                            InvalidateRect(window, nullptr, FALSE);
                            return 0;
                        }
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = skills_panel_window_plan(*host, window)) {
                    const auto header = tibia::client::layout_skills_panel_header(
                        plan->window);
                    if (contains_point(skills_rect(header.close_button), mouse))
                        host->pressed_skills_header_button = 0;
                    else if (contains_point(skills_rect(header.minimize_button), mouse))
                        host->pressed_skills_header_button = 1;
                    if (host->pressed_skills_header_button >= 0) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = skills_panel_window_plan(*host, window);
                    plan && !host->skills_window_minimized &&
                    plan->window.height > tibia::client::content_window_frame_header_height &&
                    contains_point(skills_rect(plan->resize_hit_area), mouse)) {
                    host->skills_window_resize_dragging = true;
                    host->skills_window_resize_start_y = mouse.y;
                    host->skills_window_resize_start_height = plan->window.height;
                    host->skills_window_resized_height = plan->window.height;
                    SetCapture(window);
                    host->mouse_buttons |= 1U;
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = battle_panel_window_plan(*host, window)) {
                    const auto header_height =
                        tibia::client::content_window_frame_header_height;
                    if (plan->window.height > header_height &&
                        mouse.y >= plan->window.y + plan->window.height - 4 &&
                        mouse.y < plan->window.y + plan->window.height) {
                        host->battle_window_resize_dragging = true;
                        host->battle_window_resize_start_y = mouse.y;
                        host->battle_window_resize_start_height = plan->window.height;
                        host->battle_window_resized_height = plan->window.height;
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
                if (const auto plan = vip_panel_window_plan(*host, window)) {
                    const auto header_height =
                        tibia::client::content_window_frame_header_height;
                    if (plan->window.height > header_height &&
                        mouse.y >= plan->window.y + plan->window.height - 4 &&
                        mouse.y < plan->window.y + plan->window.height) {
                        host->vip_window_resize_dragging = true;
                        host->vip_window_resize_start_y = mouse.y;
                        host->vip_window_resize_start_height = plan->window.height;
                        host->vip_window_resized_height = plan->window.height;
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = battle_panel_window_plan(*host, window)) {
                    const auto header = tibia::client::layout_skills_panel_header(
                        plan->window);
                    if (contains_point(skills_rect(header.close_button), mouse))
                        host->pressed_battle_header_button = 0;
                    else if (contains_point(skills_rect(header.minimize_button), mouse))
                        host->pressed_battle_header_button = 1;
                    if (host->pressed_battle_header_button >= 0) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = vip_panel_window_plan(*host, window)) {
                    const auto& geometry = plan->scrollbar;
                    const auto rect_for = [](const auto& rect) {
                        return RECT{rect.x, rect.y, rect.x + rect.width,
                            rect.y + rect.height};
                    };
                    host->pressed_vip_scrollbar_part = -1;
                    host->vip_scrollbar_dragging = false;
                    if (contains_point(rect_for(geometry.up_arrow), mouse)) {
                        host->pressed_vip_scrollbar_part = 0;
                        host->vip_scroll_value =
                            tibia::client::skills_panel_scroll_by_lines(
                                plan->scroll, -1);
                    } else if (contains_point(rect_for(geometry.down_arrow), mouse)) {
                        host->pressed_vip_scrollbar_part = 1;
                        host->vip_scroll_value =
                            tibia::client::skills_panel_scroll_by_lines(
                                plan->scroll, 1);
                    } else if (geometry.thumb_visible &&
                        contains_point(rect_for(geometry.thumb), mouse)) {
                        host->vip_scrollbar_dragging = true;
                        host->vip_scrollbar_grab_offset = mouse.y - geometry.thumb.y;
                    } else if (geometry.thumb_visible &&
                        contains_point(rect_for(geometry.track), mouse)) {
                        host->pressed_vip_scrollbar_part = 2;
                        host->vip_scroll_value =
                            tibia::client::skills_panel_scroll_after_track_click(
                                plan->scroll, geometry, mouse.y);
                    }
                    if (host->pressed_vip_scrollbar_part >= 0 ||
                        host->vip_scrollbar_dragging) {
                        SetCapture(window);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = vip_panel_window_plan(*host, window)) {
                    const auto header = tibia::client::layout_skills_panel_header(
                        plan->window);
                    if (contains_point(skills_rect(header.close_button), mouse))
                        host->pressed_vip_header_button = 0;
                    else if (contains_point(skills_rect(header.minimize_button), mouse))
                        host->pressed_vip_header_button = 1;
                    if (host->pressed_vip_header_button >= 0) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = skills_panel_window_plan(*host, window)) {
                    const auto& geometry = plan->skills.scrollbar;
                    const auto rect_for = [](const auto& rect) {
                        return RECT{rect.x, rect.y, rect.x + rect.width,
                            rect.y + rect.height};
                    };
                    host->pressed_skills_scrollbar_part = -1;
                    host->skills_scrollbar_dragging = false;
                    if (contains_point(rect_for(geometry.up_arrow), mouse)) {
                        host->pressed_skills_scrollbar_part = 0;
                        host->skills_scroll_value =
                            tibia::client::skills_panel_scroll_by_lines(
                                plan->skills.scroll_range, -1);
                    } else if (contains_point(rect_for(geometry.down_arrow), mouse)) {
                        host->pressed_skills_scrollbar_part = 1;
                        host->skills_scroll_value =
                            tibia::client::skills_panel_scroll_by_lines(
                                plan->skills.scroll_range, 1);
                    } else if (geometry.thumb_visible &&
                        contains_point(rect_for(geometry.thumb), mouse)) {
                        host->skills_scrollbar_dragging = true;
                        host->skills_scrollbar_grab_offset =
                            mouse.y - geometry.thumb.y;
                    } else if (geometry.thumb_visible &&
                        contains_point(rect_for(geometry.track), mouse)) {
                        host->pressed_skills_scrollbar_part = 2;
                        host->skills_scroll_value =
                            tibia::client::skills_panel_scroll_after_track_click(
                                plan->skills.scroll_range, geometry, mouse.y);
                    }
                    if (host->pressed_skills_scrollbar_part >= 0 ||
                        host->skills_scrollbar_dragging) {
                        SetCapture(window);
                        InvalidateRect(window, nullptr, FALSE);
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                if (const auto plan = battle_panel_window_plan(*host, window)) {
                    const auto& geometry = plan->scrollbar;
                    const auto rect_for = [](const auto& rect) {
                        return RECT{rect.x, rect.y, rect.x + rect.width,
                            rect.y + rect.height};
                    };
                    host->pressed_battle_scrollbar_part = -1;
                    host->battle_scrollbar_dragging = false;
                    if (contains_point(rect_for(geometry.up_arrow), mouse)) {
                        host->pressed_battle_scrollbar_part = 0;
                        host->battle_scroll_value = std::max(0,
                            plan->panel.scroll_offset -
                                tibia::client::battle_panel_scroll_step);
                    } else if (contains_point(rect_for(geometry.down_arrow), mouse)) {
                        host->pressed_battle_scrollbar_part = 1;
                        host->battle_scroll_value = std::min(
                            plan->panel.scroll_range,
                            plan->panel.scroll_offset +
                                tibia::client::battle_panel_scroll_step);
                    } else if (geometry.thumb_visible &&
                        contains_point(rect_for(geometry.thumb), mouse)) {
                        host->battle_scrollbar_dragging = true;
                        host->battle_scrollbar_grab_offset =
                            mouse.y - geometry.thumb.y;
                    } else if (geometry.thumb_visible &&
                        contains_point(rect_for(geometry.track), mouse)) {
                        host->pressed_battle_scrollbar_part = 2;
                        const auto page = std::max(0,
                            plan->panel.viewport.height -
                                tibia::client::battle_panel_row_height);
                        if (mouse.y < geometry.thumb.y)
                            host->battle_scroll_value = std::max(0,
                                plan->panel.scroll_offset - page);
                        else
                            host->battle_scroll_value = std::min(
                                plan->panel.scroll_range,
                                plan->panel.scroll_offset + page);
                    }
                    if (host->pressed_battle_scrollbar_part >= 0 ||
                        host->battle_scrollbar_dragging) {
                        SetCapture(window);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                host->pressed_battle_creature_id =
                    battle_panel_creature_at(*host, window, mouse);
                if (host->pressed_battle_creature_id != 0) {
                    SetCapture(window);
                    host->mouse_buttons |= 1U;
                    return 0;
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world &&
                host->console_transcript_scrollbar_geometry_valid &&
                host->console_transcript_item_count >
                    host->console_transcript_visible_rows) {
                const auto& viewport = host->console_transcript_scrollbar_viewport;
                const int maximum = std::max(0,
                    host->console_transcript_item_count -
                        host->console_transcript_visible_rows);
                const int first_visible = std::clamp(maximum -
                    host->console_transcript_scroll_from_bottom, 0, maximum);
                const auto geometry = tibia::client::resolve_console_message_scrollbar_geometry(
                    static_cast<int>(viewport.right - viewport.left),
                    static_cast<int>(viewport.bottom - viewport.top),
                    host->console_transcript_item_count,
                    host->console_transcript_visible_rows, first_visible);
                const RECT thumb{viewport.left + geometry.thumb.x,
                    viewport.top + geometry.thumb.y,
                    viewport.left + geometry.thumb.x + geometry.thumb.width,
                    viewport.top + geometry.thumb.y + geometry.thumb.height};
                const RECT up_arrow{viewport.left + geometry.up_arrow.x,
                    viewport.top + geometry.up_arrow.y,
                    viewport.left + geometry.up_arrow.x + geometry.up_arrow.width,
                    viewport.top + geometry.up_arrow.y + geometry.up_arrow.height};
                const RECT down_arrow{viewport.left + geometry.down_arrow.x,
                    viewport.top + geometry.down_arrow.y,
                    viewport.left + geometry.down_arrow.x + geometry.down_arrow.width,
                    viewport.top + geometry.down_arrow.y + geometry.down_arrow.height};
                const RECT track{viewport.left + geometry.track.x,
                    viewport.top + geometry.track.y,
                    viewport.left + geometry.track.x + geometry.track.width,
                    viewport.top + geometry.track.y + geometry.track.height};
                host->pressed_console_transcript_scrollbar_part = -1;
                host->console_transcript_scrollbar_dragging = false;
                if (mouse.x >= up_arrow.left && mouse.x <= up_arrow.right &&
                    mouse.y >= up_arrow.top && mouse.y <= up_arrow.bottom) {
                    host->pressed_console_transcript_scrollbar_part = 0;
                } else if (mouse.x >= down_arrow.left &&
                    mouse.x <= down_arrow.right &&
                    mouse.y >= down_arrow.top && mouse.y <= down_arrow.bottom) {
                    host->pressed_console_transcript_scrollbar_part = 1;
                } else if (geometry.thumb_visible &&
                    mouse.x >= thumb.left && mouse.x <= thumb.right &&
                    mouse.y >= thumb.top && mouse.y <= thumb.bottom) {
                    host->console_transcript_scrollbar_dragging = true;
                    host->console_transcript_scrollbar_grab_offset =
                        mouse.y - thumb.top;
                } else if (geometry.thumb_visible &&
                    mouse.x >= track.left && mouse.x <= track.right &&
                    mouse.y >= track.top && mouse.y <= track.bottom) {
                    host->pressed_console_transcript_scrollbar_part = 2;
                }
                if (host->pressed_console_transcript_scrollbar_part >= 0 ||
                    host->console_transcript_scrollbar_dragging) {
                    host->mouse_buttons |= 1U;
                    SetCapture(window);
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world &&
                contains_console_transcript_point(
                    host->console_transcript_clip, mouse) &&
                !host->console_transcript_rows.empty()) {
                host->console_transcript_selecting = true;
                host->console_transcript_selection_active = false;
                if (update_console_transcript_selection(*host, mouse)) {
                    host->console_transcript_selection_start_row =
                        host->console_transcript_selection_end_row;
                    host->console_transcript_selection_start_offset =
                        host->console_transcript_selection_end_offset;
                    SetCapture(window);
                    host->mouse_buttons |= 1U;
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world) {
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height, tibia::client::map_window_minimum_height,
                    host->no_stretching});
                const auto report_snapshot = host->report_violation_buffer
                    ? host->report_violation_buffer->snapshot()
                    : tibia::client::ReportViolationBufferSnapshot{};
                const bool report_buffer_open = report_snapshot.open;
                if (report_buffer_open && !host->report_violation_buffer_was_open)
                    host->report_violation_tab_selected = true;
                if (!report_buffer_open)
                    host->report_violation_tab_selected = false;
                host->report_violation_buffer_was_open = report_buffer_open;
                if (layout && layout->console_panel) {
                    const bool report_tab_active = report_buffer_open &&
                        host->report_violation_tab_selected;
                    const auto channel_view = host->communication_session_mailbox
                        ? host->communication_session_mailbox->channel_view()
                        : tibia::client::CommunicationChannelView{};
                    const auto default_tab =
                        tibia::client::resolve_default_console_tab_visual(
                            *layout->console_panel, layout->console.x,
                            layout->console.y, !report_tab_active, false);
                    const auto report_tab =
                        tibia::client::resolve_report_violation_tab_visual(
                            *layout->console_panel, layout->console.x,
                            layout->console.y, report_buffer_open,
                            report_tab_active, true);
                    if (default_tab && contains_point(RECT{
                            default_tab->rect.x, default_tab->rect.y,
                            default_tab->rect.x + default_tab->rect.width,
                            default_tab->rect.y + default_tab->rect.height}, mouse)) {
                        host->report_violation_tab_selected = false;
                        if (host->communication_session_mailbox)
                            host->communication_session_mailbox->request_channel_selection(std::nullopt);
                        host->console_input_focused = false;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    constexpr int channel_tab_width = 96;
                    const int channel_tab_base = 1 + (report_buffer_open ? 1 : 0);
                    const int channel_slots = report_tab_active ? 0 : std::max(0,
                        (layout->console_panel->tab_strip.width / channel_tab_width) - 1 -
                        (report_buffer_open ? 1 : 0));
                    const auto controls =
                        tibia::client::resolve_console_panel_control_visuals(
                            *layout->console_panel, layout->console.x,
                            layout->console.y, true);
                    if (controls &&
                        contains_point(RECT{(*controls)[0].rect.x, (*controls)[0].rect.y,
                            (*controls)[0].rect.x + (*controls)[0].rect.width,
                            (*controls)[0].rect.y + (*controls)[0].rect.height}, mouse)) {
                        (void)close_selected_console_channel(*host);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    if (controls && host->communication_session_mailbox &&
                        contains_point(RECT{(*controls)[1].rect.x, (*controls)[1].rect.y,
                            (*controls)[1].rect.x + (*controls)[1].rect.width,
                            (*controls)[1].rect.y + (*controls)[1].rect.height}, mouse)) {
                        host->communication_session_mailbox->request_server_messages_toggle(
                            channel_view.selected_identifier);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    if (controls && host->send_game_payload &&
                        contains_point(RECT{(*controls)[3].rect.x, (*controls)[3].rect.y,
                            (*controls)[3].rect.x + (*controls)[3].rect.width,
                            (*controls)[3].rect.y + (*controls)[3].rect.height}, mouse)) {
                        const auto payload =
                            tibia::network::encode_state6_channel_list_request();
                        host->send_game_payload(payload);
                        return 0;
                    }
                    const int max_channel_offset = std::max(0,
                        static_cast<int>(channel_view.channels.size()) - channel_slots);
                    if (controls && channel_slots > 0 &&
                        contains_point(RECT{(*controls)[4].rect.x, (*controls)[4].rect.y,
                            (*controls)[4].rect.x + (*controls)[4].rect.width,
                            (*controls)[4].rect.y + (*controls)[4].rect.height}, mouse)) {
                        host->channel_tab_scroll_offset = std::min(max_channel_offset,
                            host->channel_tab_scroll_offset + 1);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    if (controls && channel_slots > 0 &&
                        contains_point(RECT{(*controls)[5].rect.x, (*controls)[5].rect.y,
                            (*controls)[5].rect.x + (*controls)[5].rect.width,
                            (*controls)[5].rect.y + (*controls)[5].rect.height}, mouse)) {
                        host->channel_tab_scroll_offset = std::max(0,
                            host->channel_tab_scroll_offset - 1);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    if (controls &&
                        contains_point(RECT{(*controls)[6].rect.x, (*controls)[6].rect.y,
                            (*controls)[6].rect.x + (*controls)[6].rect.width,
                            (*controls)[6].rect.y + (*controls)[6].rect.height}, mouse)) {
                        host->console_volume_state = static_cast<std::uint8_t>(
                            (host->console_volume_state + 1U) % 3U);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    for (int index = 0; index < channel_slots &&
                         host->channel_tab_scroll_offset + index <
                            static_cast<int>(channel_view.channels.size()); ++index) {
                        const int x = layout->console.x + layout->console_panel->tab_strip.x +
                            channel_tab_width * (index + channel_tab_base);
                        const int y = layout->console.y + layout->console_panel->tab_strip.y;
                        if (!contains_point(RECT{x, y, x + channel_tab_width, y + 18}, mouse))
                            continue;
                        if (host->communication_session_mailbox)
                            host->communication_session_mailbox->request_channel_selection(
                                channel_view.channels[static_cast<std::size_t>(
                                    host->channel_tab_scroll_offset + index)].id);
                        host->report_violation_tab_selected = false;
                        host->console_input_focused = false;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    if (report_tab && contains_point(RECT{
                            report_tab->rect.x, report_tab->rect.y,
                            report_tab->rect.x + report_tab->rect.width,
                            report_tab->rect.y + report_tab->rect.height}, mouse)) {
                        host->report_violation_tab_selected = true;
                        host->console_input_focused = false;
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !notice_open &&
                host->new_channel_dialog_open) {
                const auto dialog = new_channel_dialog_rect(*host,
                    host->new_channel_dialog_offset);
                const auto list = new_channel_list_rect(*host);
                const auto field = new_channel_private_field_rect(*host);
                host->pressed_new_channel_button = -1;
                host->pressed_new_channel_scroll_button = -1;
                host->private_channel_focused = contains_point(field, mouse);
                for (int index = 0; index < 2; ++index)
                    if (contains_point(new_channel_button_rect(*host, index), mouse))
                        host->pressed_new_channel_button = index;
                if (contains_point(list, mouse)) {
                    const int margin = ui_units(window, 3);
                    const int row_height = ui_units(window,
                        std::max(1, host->ui_font_heights[3]));
                    const int visible_rows = std::max(1,
                        (static_cast<int>(list.bottom - list.top) - 2 * margin) /
                            row_height);
                    const int width = static_cast<int>(list.right - list.left);
                    const int height = static_cast<int>(list.bottom - list.top);
                    const auto geometry = tibia::client::ui_scrollbar_geometry(
                        width, height, host->available_channels.size(), visible_rows,
                        host->new_channel_scroll, margin, ui_units(window, 12),
                        ui_units(window, 12), ui_units(window, 6));
                    const auto up = options_list_scrollbar_part_rect(list,
                        geometry.up_arrow);
                    const auto down = options_list_scrollbar_part_rect(list,
                        geometry.down_arrow);
                    const auto track = options_list_scrollbar_part_rect(list,
                        geometry.track);
                    if (contains_point(up, mouse)) {
                        host->new_channel_scroll = std::max(0,
                            host->new_channel_scroll - 1);
                        host->pressed_new_channel_scroll_button = 0;
                    } else if (contains_point(down, mouse)) {
                        host->new_channel_scroll = std::min(
                            std::max(0, static_cast<int>(host->available_channels.size()) -
                                visible_rows), host->new_channel_scroll + 1);
                        host->pressed_new_channel_scroll_button = 1;
                    } else if (geometry.thumb_visible && contains_point(track, mouse) &&
                        contains_point(options_list_scrollbar_part_rect(list,
                            geometry.thumb), mouse)) {
                        host->new_channel_scrollbar_dragging = true;
                        host->new_channel_scrollbar_grab_offset = mouse.y - list.top -
                            geometry.thumb.top;
                    } else if (contains_point(track, mouse)) {
                        host->new_channel_scroll =
                            tibia::client::ui_scrollbar_scroll_after_track_click(
                                mouse.y - list.top, geometry, host->new_channel_scroll,
                                host->available_channels.size(), visible_rows);
                    } else {
                        const int row = (mouse.y - list.top - margin) / row_height;
                        const int index = host->new_channel_scroll + row;
                        if (row >= 0 && row < visible_rows &&
                            static_cast<std::size_t>(index) <
                                host->available_channels.size())
                            host->new_channel_selected = index;
                    }
                    host->private_channel_focused = false;
                }
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::new_channel;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->new_channel_dialog_offset;
                }
                SetFocus(window);
                SetCapture(window);
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONDOWN && notice_open) {
                const auto dialog = notice_rect(*host, host->notice_dialog_offset);
                host->pressed_notice_button = contains_point(notice_button_rect(*host), mouse)
                    ? 0 : -1;
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::notice;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->notice_dialog_offset;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !notice_open && host->motd_open) {
                const auto dialog = motd_dialog_rect(*host, host->motd_dialog_offset);
                host->pressed_motd_button = -1;
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::motd;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->motd_dialog_offset;
                }
                if (contains_point(motd_button_rect(*host, 0), mouse))
                    host->pressed_motd_button = 0;
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !notice_open &&
                host->account_data_warning_open) {
                const auto dialog = account_data_warning_dialog_rect(*host,
                    host->account_data_warning_dialog_offset);
                host->pressed_account_data_warning_button =
                    contains_point(account_data_warning_button_rect(*host), mouse) ? 0 : -1;
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::account_data_warning;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->account_data_warning_dialog_offset;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !notice_open &&
                host->rule_violation_request_form.open) {
                const auto dialog = rule_violation_request_dialog_rect(window,
                    host->rule_violation_request_dialog_offset);
                auto& form = host->rule_violation_request_form;
                host->pressed_rule_violation_request_button = -1;
                host->pressed_rule_violation_request_checkbox = false;
                host->rule_violation_request_focus_field = -1;
                for (const int field : {0, 1, 3}) {
                    if (field == 1 && !form.statement_enabled) continue;
                    if (contains_point(rule_violation_request_edit_rect(window,
                            host->rule_violation_request_dialog_offset, field), mouse))
                        host->rule_violation_request_focus_field = field;
                }
                for (int index = 0; index < 2; ++index)
                    if (contains_point(rule_violation_request_button_rect(window,
                            host->rule_violation_request_dialog_offset, index), mouse))
                        host->pressed_rule_violation_request_button = index;
                const auto checkbox = rule_violation_request_checkbox_rect(window,
                    host->rule_violation_request_dialog_offset);
                host->pressed_rule_violation_request_checkbox =
                    contains_point(checkbox, mouse) && form.ip_banishment_enabled;
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top &&
                    mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog =
                        ClientHost::DraggedDialog::rule_violation_request;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset =
                        host->rule_violation_request_dialog_offset;
                }
                const auto select_list_row = [&](bool actions) {
                    const auto target = actions
                        ? ClientHost::ListScrollbarTarget::rule_violation_actions
                        : ClientHost::ListScrollbarTarget::rule_violation_reasons;
                    const auto list = options_list_rect(*host, target);
                    if (!contains_point(list, mouse)) return false;
                    if (begin_options_list_scrollbar_interaction(*host,
                            target, mouse)) {
                        SetCapture(window);
                        host->mouse_buttons |= 1U;
                        InvalidateRect(window, nullptr, FALSE);
                        return true;
                    }
                    const int margin = ui_units(window, 3);
                    const int row_height = ui_units(window,
                        std::max(1, host->ui_font_line_advances[3]));
                    const int visible = options_list_visible_items(*host, target);
                    if (row_height <= 0 || mouse.y < list.top + margin ||
                        mouse.y >= list.bottom - margin ||
                        mouse.x >= list.right - ui_units(window, 15))
                        return true;
                    const int row = (mouse.y - list.top - margin) / row_height;
                    if (row < 0 || row >= visible) return true;
                    const int index = options_list_first_visible(*host, target) + row;
                    const auto& entries = actions ? form.available_actions :
                        form.available_reasons;
                    if (index >= 0 && static_cast<std::size_t>(index) < entries.size()) {
                        if (actions)
                            (void)form.select_action(entries[static_cast<std::size_t>(index)]);
                        else if (form.select_reason(entries[static_cast<std::size_t>(index)]))
                            host->rule_violation_request_action_scroll = 0;
                    }
                    return true;
                };
                if (select_list_row(false) || select_list_row(true)) {
                    SetFocus(window);
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
                SetTimer(window, login_caret_timer_id, 100, nullptr);
                SetFocus(window);
                SetCapture(window);
                host->mouse_buttons |= 1U;
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONDOWN && !notice_open && host->report_violation_dialog.open) {
                const auto dialog = report_violation_dialog_rect(window,
                    host->report_rule_violation_dialog_offset);
                host->report_violation_dialog.pressed_button = -1;
                host->report_violation_dialog.text_focused = contains_point(
                    report_violation_input_rect(window,
                        host->report_rule_violation_dialog_offset), mouse);
                for (int index = 0; index < 2; ++index)
                    if (contains_point(report_violation_button_rect(window,
                        host->report_rule_violation_dialog_offset, index), mouse))
                        host->report_violation_dialog.pressed_button = index;
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::report_rule_violation;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->report_rule_violation_dialog_offset;
                }
                if (host->report_violation_dialog.text_focused)
                    SetTimer(window, login_caret_timer_id, 100, nullptr);
                else
                    KillTimer(window, login_caret_timer_id);
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !notice_open && host->hint_content_open) {
                const auto dialog = hint_content_dialog_rect(window,
                    host->hint_content_dialog_offset);
                host->pressed_hint_content_button = contains_point(
                    hint_content_ok_rect(window, host->hint_content_dialog_offset), mouse) ? 0 : -1;
                host->hint_content_scroll_dragging = false;
                const auto view = hint_content_view_rect(window,
                    host->hint_content_dialog_offset);
                const int visible = hint_content_visible_lines(*host, view);
                const auto geometry = options_list_scrollbar_geometry(window, view,
                    host->hint_content_lines.size(), visible,
                    host->hint_content_scroll);
                const auto up = options_list_scrollbar_part_rect(view, geometry.up_arrow);
                const auto down = options_list_scrollbar_part_rect(view, geometry.down_arrow);
                const auto track = options_list_scrollbar_part_rect(view, geometry.track);
                host->pressed_hint_content_scroll_part = -1;
                if (contains_point(up, mouse)) {
                    host->pressed_hint_content_scroll_part = 0;
                    host->hint_content_scroll = std::max(0, host->hint_content_scroll - 1);
                } else if (contains_point(down, mouse)) {
                    host->pressed_hint_content_scroll_part = 1;
                    host->hint_content_scroll = std::min(
                        std::max(0, static_cast<int>(host->hint_content_lines.size()) - visible),
                        host->hint_content_scroll + 1);
                } else if (geometry.thumb_visible && contains_point(
                    options_list_scrollbar_part_rect(view, geometry.thumb), mouse)) {
                    host->hint_content_scroll_dragging = true;
                    host->hint_content_scroll_grab_offset =
                        mouse.y - view.top - geometry.thumb.top;
                } else if (geometry.thumb_visible && contains_point(track, mouse)) {
                    host->hint_content_scroll = tibia::client::ui_scrollbar_scroll_after_track_click(
                        mouse.y - view.top, geometry, host->hint_content_scroll,
                        host->hint_content_lines.size(), visible);
                    host->pressed_hint_content_scroll_part = 2;
                }
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::hint_content;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->hint_content_dialog_offset;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !notice_open &&
                !host->hint_content_open && host->hints_open) {
                const auto dialog = hints_dialog_rect(window, host->hints_dialog_offset);
                host->pressed_hints_button = -1;
                host->hints_scroll_dragging = false;
                for (int index = 0; index < 2; ++index)
                    if (contains_point(hints_button_rect(window,
                        host->hints_dialog_offset, index), mouse))
                        host->pressed_hints_button = index;
                const auto list = hints_list_rect(window, host->hints_dialog_offset);
                const int line_height = ui_units(window,
                    std::max(1, host->ui_font_heights[2]));
                const int visible = std::max(1,
                    static_cast<int>(list.bottom - list.top) / line_height);
                if (contains_point(list, mouse) &&
                    mouse.x < list.right - ui_units(window, 15)) {
                    const int row = (mouse.y - list.top) / line_height;
                    const int selected = host->hints_scroll + row;
                    if (selected >= 0 && selected < static_cast<int>(host->hints.size()))
                        host->hints_selected = selected;
                }
                const auto geometry = options_list_scrollbar_geometry(window, list,
                    host->hints.size(), visible, host->hints_scroll);
                const auto up = options_list_scrollbar_part_rect(list, geometry.up_arrow);
                const auto down = options_list_scrollbar_part_rect(list, geometry.down_arrow);
                const auto track = options_list_scrollbar_part_rect(list, geometry.track);
                host->pressed_hints_scroll_part = -1;
                if (contains_point(up, mouse)) {
                    host->pressed_hints_scroll_part = 0;
                    host->hints_scroll = std::max(0, host->hints_scroll - 1);
                } else if (contains_point(down, mouse)) {
                    host->pressed_hints_scroll_part = 1;
                    host->hints_scroll = std::min(
                        std::max(0, static_cast<int>(host->hints.size()) - visible),
                        host->hints_scroll + 1);
                } else if (geometry.thumb_visible && contains_point(
                    options_list_scrollbar_part_rect(list, geometry.thumb), mouse)) {
                    host->hints_scroll_dragging = true;
                    host->hints_scroll_grab_offset =
                        mouse.y - list.top - geometry.thumb.top;
                } else if (geometry.thumb_visible && contains_point(track, mouse)) {
                    host->hints_scroll = tibia::client::ui_scrollbar_scroll_after_track_click(
                        mouse.y - list.top, geometry, host->hints_scroll,
                        host->hints.size(), visible);
                    host->pressed_hints_scroll_part = 2;
                }
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::hints;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->hints_dialog_offset;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !notice_open && !host->hints_open &&
                !host->report_violation_dialog.open && host->help_open) {
                const auto dialog = help_dialog_rect(window, host->help_dialog_offset);
                host->pressed_help_button = -1;
                if (contains_point(help_ok_rect(window, host->help_dialog_offset), mouse)) {
                    host->pressed_help_button = 6;
                } else {
                    for (int index = 0; index < 6; ++index) {
                        if (contains_point(help_action_button_rect(window,
                            host->help_dialog_offset, index), mouse))
                            host->pressed_help_button = index;
                    }
                }
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::help;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->help_dialog_offset;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !notice_open && host->quest_line_open) {
                const auto dialog = quest_line_dialog_rect(window,
                    host->quest_line_dialog_offset);
                host->pressed_quest_line_button = contains_point(
                    quest_line_close_rect(window, host->quest_line_dialog_offset), mouse) ? 0 : -1;
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::quest_line;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->quest_line_dialog_offset;
                }
                const auto flags = quest_line_flags_rect(window,
                    host->quest_line_dialog_offset);
                const int row_height = ui_units(window,
                    tibia::client::quest_log_row_height);
                host->pressed_quest_scroll_button = -1;
                const bool scroll_part = contains_point(flags, mouse) &&
                    begin_quest_scroll_interaction(*host, mouse, true);
                if (!scroll_part && contains_point(flags, mouse) && row_height > 0 &&
                    mouse.x < flags.right - ui_units(window,
                        tibia::client::quest_log_content_margin +
                            tibia::client::quest_scrollbar_width)) {
                    const int visible_row = (mouse.y - flags.top - ui_units(window,
                        tibia::client::quest_log_content_margin)) / row_height;
                    const int row = host->quest_flag_scroll + visible_row;
                    const int visible_rows = tibia::client::quest_log_visible_rows(
                        flags.bottom - flags.top, row_height, ui_units(window,
                            tibia::client::quest_log_content_margin));
                    const auto snapshot = host->game_world_mailbox->read();
                    if (snapshot) {
                        const auto line = std::find_if(snapshot->quest_lines.begin(),
                            snapshot->quest_lines.end(), [id = host->quest_line_id](const auto& entry) {
                                return entry.id == id;
                            });
                        if (!host->quest_line_flags_pending &&
                            line != snapshot->quest_lines.end() && visible_row >= 0 &&
                            visible_row < visible_rows &&
                            static_cast<std::size_t>(row) < line->flags.size())
                            host->quest_flag_selected = row;
                    }
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            } else if (message == WM_LBUTTONDOWN && !notice_open && host->quest_log_open) {
                const auto dialog = quest_log_dialog_rect(window,
                    host->quest_log_dialog_offset);
                host->pressed_quest_log_button = -1;
                for (int index = 0; index < 2; ++index)
                    if (contains_point(quest_log_button_rect(window,
                        host->quest_log_dialog_offset, index), mouse))
                        host->pressed_quest_log_button = index;
                if (mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + ui_units(window, 17)) {
                    host->dragged_dialog = ClientHost::DraggedDialog::quest_log;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = host->quest_log_dialog_offset;
                }
                const auto list = quest_log_list_rect(window,
                    host->quest_log_dialog_offset);
                const int row_height = ui_units(window,
                    tibia::client::quest_log_row_height);
                host->pressed_quest_scroll_button = -1;
                const bool scroll_part = contains_point(list, mouse) &&
                    begin_quest_scroll_interaction(*host, mouse, false);
                if (!scroll_part && contains_point(list, mouse) && row_height > 0 &&
                    mouse.x < list.right - ui_units(window,
                        tibia::client::quest_log_content_margin +
                            tibia::client::quest_scrollbar_width)) {
                    const int visible_row = (mouse.y - list.top - ui_units(window,
                        tibia::client::quest_log_content_margin)) / row_height;
                    const int row = host->quest_log_scroll + visible_row;
                    const int visible_rows = tibia::client::quest_log_visible_rows(
                        list.bottom - list.top, row_height, ui_units(window,
                            tibia::client::quest_log_content_margin));
                    const auto snapshot = host->game_world_mailbox->read();
                    if (snapshot && visible_row >= 0 && visible_row < visible_rows &&
                        static_cast<std::size_t>(row) < snapshot->quest_log_entries.size())
                        host->quest_log_selected = row;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open) {
                RECT dialog{};
                POINT* offset{};
                int title_height{};
                if (host->advanced_graphics_options_open) {
                    dialog = advanced_graphics_options_dialog_rect(window,
                        host->advanced_graphics_options_dialog_offset);
                    offset = &host->advanced_graphics_options_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->network_options_open) {
                    dialog = network_options_dialog_rect(window,
                        host->network_options_dialog_offset);
                    offset = &host->network_options_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->graphics_options_open) {
                    dialog = graphics_options_dialog_rect(window,
                        host->graphics_options_dialog_offset);
                    offset = &host->graphics_options_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->general_options_open) {
                    dialog = general_options_dialog_rect(window,
                        host->general_options_dialog_offset);
                    offset = &host->general_options_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->console_options_open) {
                    dialog = console_options_dialog_rect(window,
                        host->console_options_dialog_offset);
                    offset = &host->console_options_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->hotkey_options_open) {
                    dialog = hotkey_options_dialog_rect(window,
                        host->hotkey_options_dialog_offset);
                    offset = &host->hotkey_options_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->options_open) {
                    dialog = options_rect(window, host->options_dialog_offset);
                    offset = &host->options_dialog_offset;
                    title_height = ui_units(window, 21);
                } else if (host->info_open) {
                    dialog = info_dialog_rect(window, host->info_dialog_offset);
                    offset = &host->info_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->screen == UiScreen::login) {
                    dialog = login_dialog_rect(window, host->login_dialog_offset);
                    offset = &host->login_dialog_offset;
                    title_height = ui_units(window, 17);
                } else if (host->screen == UiScreen::character_selection) {
                    dialog = character_selection_dialog_rect(window,
                        host->character_selection_dialog_offset);
                    offset = &host->character_selection_dialog_offset;
                    title_height = ui_units(window, 17);
                }
                if (offset && mouse.x >= dialog.left && mouse.x < dialog.right &&
                    mouse.y >= dialog.top && mouse.y < dialog.top + title_height) {
                    host->dragged_dialog = host->hotkey_options_open ?
                        ClientHost::DraggedDialog::hotkey_options :
                        host->console_options_open ?
                        ClientHost::DraggedDialog::console_options :
                        host->advanced_graphics_options_open ?
                            ClientHost::DraggedDialog::advanced_graphics_options :
                        host->graphics_options_open ? ClientHost::DraggedDialog::graphics_options :
                        host->network_options_open ? ClientHost::DraggedDialog::network_options :
                        host->general_options_open ? ClientHost::DraggedDialog::general_options :
                        host->options_open ? ClientHost::DraggedDialog::options :
                        host->info_open ? ClientHost::DraggedDialog::info :
                        host->screen == UiScreen::character_selection ?
                            ClientHost::DraggedDialog::character_selection :
                            ClientHost::DraggedDialog::login;
                    host->dialog_drag_anchor = mouse;
                    host->dialog_drag_start_offset = *offset;
                    host->pressed_login_button = -1;
                    SetCapture(window);
                }
            }
            if (message == WM_LBUTTONDOWN && modal_open) {
                SetFocus(window);
            }
            if (message == WM_LBUTTONDOWN && host->info_open) SetFocus(window);
            if (message == WM_LBUTTONDOWN && host->info_open) {
                host->pressed_info_button = contains_point(
                    info_ok_rect(window, host->info_dialog_offset), mouse) ? 0 : -1;
                const auto website_button = info_website_rect(window, host->info_dialog_offset);
                if (contains_point(website_button, mouse)) host->pressed_info_button = 2;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open && host->network_options_open) {
                host->pressed_network_options_control = -1;
                if (contains_point(network_options_row_rect(window,
                    host->network_options_dialog_offset, 0), mouse))
                    host->pressed_network_options_control = 0;
                for (std::size_t index = 0; index < 3; ++index) {
                    if (contains_point(network_options_button_rect(window,
                        host->network_options_dialog_offset, index), mouse))
                        host->pressed_network_options_control = static_cast<int>(index) + 1;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->advanced_graphics_options_open) {
                host->pressed_advanced_graphics_control = -1;
                for (std::size_t index = 0; index < 3; ++index) {
                    if (contains_point(advanced_graphics_engine_button_rect(window,
                        host->advanced_graphics_options_dialog_offset, index), mouse))
                        host->pressed_advanced_graphics_control = static_cast<int>(index);
                }
                const auto ambient = advanced_graphics_slider_rect(window,
                    host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_ambient_slider_y);
                if (contains_point(ambient, mouse)) {
                    const int button_width = ui_units(window, 12);
                    host->pressed_advanced_graphics_control = mouse.x < ambient.left + button_width
                        ? 13 : mouse.x >= ambient.right - button_width ? 14 : 3;
                }
                if (contains_point(advanced_graphics_checkbox_rect(window,
                    host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_light_effects_y), mouse))
                    host->pressed_advanced_graphics_control = 4;
                if (contains_point(advanced_graphics_checkbox_rect(window,
                    host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_stretch_y), mouse))
                    host->pressed_advanced_graphics_control = 5;
                if (contains_point(advanced_graphics_checkbox_rect(window,
                    host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_mouse_pointer_y), mouse))
                    host->pressed_advanced_graphics_control = 6;
                const auto framerate = advanced_graphics_slider_rect(window,
                    host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_framerate_slider_y);
                if (contains_point(framerate, mouse)) {
                    const int button_width = ui_units(window, 12);
                    host->pressed_advanced_graphics_control = mouse.x < framerate.left + button_width
                        ? 17 : mouse.x >= framerate.right - button_width ? 18 : 7;
                }
                for (std::size_t index = 0; index < 3; ++index)
                    if (contains_point(advanced_graphics_button_rect(window,
                        host->advanced_graphics_options_dialog_offset, index), mouse))
                        host->pressed_advanced_graphics_control = static_cast<int>(index) + 8;
                if (host->pressed_advanced_graphics_control == 3 ||
                    host->pressed_advanced_graphics_control == 7)
                    update_advanced_graphics_slider(*host,
                        host->pressed_advanced_graphics_control, mouse.x);
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open && host->graphics_options_open &&
                !host->advanced_graphics_options_open) {
                host->pressed_graphics_options_control = -1;
                if (contains_point(graphics_options_fullscreen_rect(window,
                    host->graphics_options_dialog_offset), mouse))
                    host->pressed_graphics_options_control = 0;
                const auto list = graphics_options_list_rect(window,
                    host->graphics_options_dialog_offset);
                const bool scrollbar_part = contains_point(list, mouse) &&
                    begin_options_list_scrollbar_interaction(*host,
                        ClientHost::ListScrollbarTarget::graphics_modes, mouse);
                if (!scrollbar_part && contains_point(list, mouse) &&
                    mouse.x < list.right - ui_units(window, 15)) {
                    const int row_height = ui_units(window,
                        tibia::client::graphics_options_row_height);
                    const int row = (mouse.y - list.top - ui_units(window, 3)) / row_height;
                    const int index = host->graphics_mode_scroll + row;
                    if (row >= 0 && row < tibia::client::graphics_options_visible_rows &&
                        index >= 0 && static_cast<std::size_t>(index) < host->graphics_display_modes.size()) {
                        host->graphics_selected_mode = index;
                        host->pressed_graphics_options_control = -2;
                    }
                }
                if (contains_point(graphics_options_advanced_rect(window,
                    host->graphics_options_dialog_offset), mouse))
                    host->pressed_graphics_options_control = 4;
                for (std::size_t index = 0; index < 3; ++index)
                    if (contains_point(graphics_options_button_rect(window,
                        host->graphics_options_dialog_offset, index), mouse))
                        host->pressed_graphics_options_control = static_cast<int>(index) + 1;
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open && host->general_options_open) {
                host->pressed_general_options_control = -1;
                for (std::size_t index = 0; index < tibia::client::general_options_labels.size(); ++index) {
                    if (contains_point(general_options_row_rect(window,
                        host->general_options_dialog_offset, index), mouse)) {
                        host->pressed_general_options_control = static_cast<int>(index);
                        break;
                    }
                }
                for (std::size_t index = 0; index < 2; ++index) {
                    if (contains_point(general_options_button_rect(window,
                        host->general_options_dialog_offset, index), mouse))
                        host->pressed_general_options_control = static_cast<int>(index) + 5;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open && host->console_options_open) {
                host->pressed_console_options_control = -1;
                for (std::size_t index = 0; index < tibia::client::console_options_labels.size(); ++index) {
                    if (contains_point(console_options_row_rect(window,
                        host->console_options_dialog_offset, index), mouse)) {
                        host->pressed_console_options_control = static_cast<int>(index);
                        break;
                    }
                }
                for (std::size_t index = 0; index < 3; ++index) {
                    if (contains_point(console_options_button_rect(window,
                        host->console_options_dialog_offset, index), mouse))
                        host->pressed_console_options_control = static_cast<int>(index) + 6;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open && host->hotkey_options_open) {
                host->pressed_hotkey_options_control = -1;
                const auto list = hotkey_options_list_rect(window,
                    host->hotkey_options_dialog_offset);
                const bool scrollbar_part = contains_point(list, mouse) &&
                    begin_options_list_scrollbar_interaction(*host,
                        ClientHost::ListScrollbarTarget::hotkeys, mouse);
                if (!scrollbar_part && contains_point(list, mouse) &&
                    mouse.x < list.right - ui_units(window, 15)) {
                    const std::size_t first = host->hotkey_list_scroll;
                    const int row = (mouse.y - list.top - ui_units(window, 2)) /
                        ui_units(window, tibia::client::hotkey_list_row_height);
                    if (row >= 0 && row < static_cast<int>(tibia::client::hotkey_list_visible_rows))
                        select_hotkey(*host, first + static_cast<std::size_t>(row));
                }
                const auto field = hotkey_options_control_rect(window,
                    host->hotkey_options_dialog_offset, 0,
                    tibia::client::hotkey_text_y, 0, tibia::client::hotkey_text_height);
                host->hotkey_text_focused = contains_point(field, mouse);
                if (contains_point(hotkey_options_control_rect(window,
                    host->hotkey_options_dialog_offset, 0,
                    tibia::client::hotkey_automatic_y, 0,
                    tibia::client::hotkey_automatic_height), mouse))
                    host->pressed_hotkey_options_control = 7;
                const auto select = hotkey_options_control_rect(window,
                    host->hotkey_options_dialog_offset, tibia::client::hotkey_select_x,
                    tibia::client::hotkey_select_y, tibia::client::hotkey_button_width,
                    tibia::client::hotkey_button_height);
                if (contains_point(select, mouse)) host->pressed_hotkey_options_control = 0;
                const auto clear = hotkey_options_control_rect(window,
                    host->hotkey_options_dialog_offset, tibia::client::hotkey_clear_x,
                    tibia::client::hotkey_clear_y, tibia::client::hotkey_button_width,
                    tibia::client::hotkey_button_height);
                if (contains_point(clear, mouse)) host->pressed_hotkey_options_control = 1;
                for (std::size_t index = 0; index < tibia::client::hotkey_mode_labels.size(); ++index) {
                    const auto button = hotkey_options_control_rect(window,
                        host->hotkey_options_dialog_offset, tibia::client::hotkey_mode_x,
                        tibia::client::hotkey_mode_y[index], tibia::client::hotkey_button_width,
                        tibia::client::hotkey_button_height);
                    if (contains_point(button, mouse))
                        host->pressed_hotkey_options_control = static_cast<int>(index) + 2;
                }
                for (std::size_t index = 0; index < 2; ++index)
                    if (contains_point(hotkey_options_footer_button_rect(window,
                        host->hotkey_options_dialog_offset, index), mouse))
                        host->pressed_hotkey_options_control = static_cast<int>(index) + 5;
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open && host->screen == UiScreen::login) {
                if (contains_point(login_account_field_rect(window, host->login_dialog_offset), host->mouse))
                    host->login_focus_field = 0;
                else if (contains_point(login_password_field_rect(window, host->login_dialog_offset), host->mouse))
                    host->login_focus_field = 1;
                host->pressed_login_button = -1;
                if (contains_point(login_ok_rect(window, host->login_dialog_offset), host->mouse))
                    host->pressed_login_button = 0;
                else if (contains_point(login_cancel_rect(window, host->login_dialog_offset), host->mouse))
                    host->pressed_login_button = 1;
                else {
                    const auto panel = login_rect(window, host->login_dialog_offset);
                    RECT link_button{panel.left + ui_units(window, 114),
                        panel.top + ui_units(window, 58),
                        panel.left + ui_units(window, 200), panel.top + ui_units(window, 78)};
                    if (contains_point(link_button, host->mouse)) host->pressed_login_button = 2;
                }
                SetFocus(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::character_selection) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const auto list = character_list_rect(window,
                    host->character_selection_dialog_offset);
                if (contains_point(list, host->mouse)) {
                    const int margin = ui_units(window, 3);
                    const int row_height = ui_units(window,
                        std::max(1, host->ui_font_heights[3]));
                    const int visible_row = (host->mouse.y - list.top - margin) / row_height;
                    const int visible_rows = std::max(1,
                        (static_cast<int>(list.bottom - list.top) - 2 * margin) / row_height);
                    const int row = host->character_list_scroll + visible_row;
                    const auto geometry = options_list_scrollbar_geometry(window, list,
                        host->characters.size(), visible_rows, host->character_list_scroll);
                    const auto up = options_list_scrollbar_part_rect(list, geometry.up_arrow);
                    const auto down = options_list_scrollbar_part_rect(list, geometry.down_arrow);
                    const auto track = options_list_scrollbar_part_rect(list, geometry.track);
                    const int maximum = std::max(0,
                        static_cast<int>(host->characters.size()) - visible_rows);
                    if (contains_point(up, host->mouse)) {
                        host->pressed_character_scroll_button = 0;
                        host->character_list_scroll = std::max(0, host->character_list_scroll - 1);
                    } else if (contains_point(down, host->mouse)) {
                        host->pressed_character_scroll_button = 1;
                        host->character_list_scroll = std::min(maximum, host->character_list_scroll + 1);
                    } else if (geometry.thumb_visible && contains_point(track, host->mouse)) {
                        const int first = tibia::client::ui_scrollbar_scroll_after_track_click(
                            host->mouse.y - list.top, geometry, host->character_list_scroll,
                            host->characters.size(), visible_rows);
                        host->character_list_scroll = std::clamp(first, 0, maximum);
                    } else if (visible_row >= 0 && visible_row < visible_rows && row >= 0 &&
                        static_cast<std::size_t>(row) < host->characters.size()) {
                        host->selected_character = row;
                        SendMessageW(host->character_list, LB_SETCURSEL,
                            static_cast<WPARAM>(row), 0);
                        EnableWindow(host->connect_button, TRUE);
                    }
                    InvalidateRect(window, nullptr, FALSE);
                }
                host->pressed_login_button =
                    contains_point(character_ok_rect(window, host->character_selection_dialog_offset), host->mouse) ? 0 :
                    contains_point(character_cancel_rect(window, host->character_selection_dialog_offset), host->mouse) ? 1 : -1;
                SetFocus(window);
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::main_menu && !host->options_open && !host->info_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const auto menu = menu_rect(window);
                const auto scale = ui_scale(window);
                for (std::size_t i = 0; i < menu_button_ids.size(); ++i) {
                    RECT button{menu.left + static_cast<int>(std::lround(15.0F * scale)),
                        menu.top + static_cast<int>(std::lround((15.0F + 30.0F * i) * scale)), 0, 0};
                    button.right = button.left + static_cast<int>(std::lround(86.0F * scale));
                    button.bottom = button.top + static_cast<int>(std::lround(20.0F * scale));
                    if (PtInRect(&button, host->mouse)) {
                        host->pressed_menu_index = static_cast<int>(i);
                        InvalidateRect(window, nullptr, FALSE);
                        break;
                    }
                }
            }
            if (message == WM_LBUTTONDOWN && !modal_open && host->options_open &&
                !host->console_options_open && !host->general_options_open &&
                !host->network_options_open && !host->graphics_options_open &&
                !host->hotkey_options_open) {
                const auto dialog = options_rect(window, host->options_dialog_offset);
                const float scale = ui_scale(window);
                const auto s = [scale](int value) {
                    return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
                };
                host->pressed_options_button = -1;
                constexpr int row_y[]{32, 65, 98, 131, 164, 214};
                for (int i = 0; i < 6; ++i) {
                    RECT button{dialog.left + s(18), dialog.top + s(row_y[i]),
                        dialog.left + s(104), dialog.top + s(row_y[i] + 20)};
                    if (contains_point(button, host->mouse)) host->pressed_options_button = i;
                }
                RECT ok{dialog.left + s(230), dialog.top + s(264),
                    dialog.left + s(273), dialog.top + s(284)};
                if (contains_point(ok, host->mouse)) host->pressed_options_button = 6;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONDOWN && !modal_open &&
                host->screen == UiScreen::game_world && !host->options_open &&
                !host->info_open && !host->console_options_open &&
                !host->hotkey_options_open && !host->general_options_open &&
                !host->network_options_open && !host->graphics_options_open &&
                !host->advanced_graphics_options_open) {
                host->pressed_inventory_control = -1;
                host->pressed_combat_control = -1;
                host->pressed_combat_action = -1;
                host->pressed_game_screen_control = -1;
                host->pressed_automap_control = -1;
                host->pressed_automap_compass = -1;
                host->console_resize_dragging = false;
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height, tibia::client::map_window_minimum_height,
                    host->no_stretching});
                if (layout) {
                    const auto console_separator_y = layout->console.y;
                    if (mouse.x >= layout->console.x &&
                        mouse.x < layout->console.x + layout->console.width &&
                        mouse.y >= console_separator_y - 5 &&
                        mouse.y <= console_separator_y + 1)
                        host->console_resize_dragging = true;
                    host->console_input_focused = false;
                    const int automap_y = host->inventory_minimized
                        ? tibia::client::automap_panel_minimized_inventory_y
                        : tibia::client::automap_panel_full_inventory_y;
                    const auto automap = tibia::client::layout_automap_panel(
                        layout->sidebar.x, automap_y);
                    for (std::size_t index = 0; index < automap.controls.size(); ++index) {
                        const auto& bounds = automap.controls[index].bounds;
                        if (contains_point(RECT{bounds.x, bounds.y,
                                bounds.x + bounds.width, bounds.y + bounds.height}, mouse))
                            host->pressed_automap_control = static_cast<int>(index);
                    }
                    const auto& centre = automap.centre.bounds;
                    if (contains_point(RECT{centre.x, centre.y,
                            centre.x + centre.width, centre.y + centre.height}, mouse))
                        host->pressed_automap_control = 4;
                    const auto& compass = automap.compass;
                    if (contains_point(RECT{compass.x, compass.y,
                            compass.x + compass.width, compass.y + compass.height}, mouse)) {
                        const int local_x = mouse.x - compass.x;
                        const int local_y = mouse.y - compass.y;
                        const int middle_x = compass.width / 2;
                        const int middle_y = compass.height / 2;
                        if (local_x >= middle_x - 5 && local_x < middle_x + 5 &&
                            local_y >= middle_y - 5 && local_y < middle_y + 5)
                            host->pressed_automap_compass = 0;
                        else if (local_x >= middle_x - 5 && local_x < middle_x + 5 &&
                            local_y < middle_y - 5)
                            host->pressed_automap_compass = 1;
                        else if (local_x >= middle_x - 5 && local_x < middle_x + 5 &&
                            local_y >= middle_y + 5)
                            host->pressed_automap_compass = 2;
                        else if (local_x < middle_x - 5 &&
                            local_y >= middle_y - 5 && local_y < middle_y + 5)
                            host->pressed_automap_compass = 3;
                        else if (local_x >= middle_x + 5 &&
                            local_y >= middle_y - 5 && local_y < middle_y + 5)
                            host->pressed_automap_compass = 4;
                        else
                            host->pressed_automap_compass = 5;
                    }
                    const auto game_screen_buttons =
                        tibia::client::layout_game_screen_control_buttons(
                            layout->sidebar.x,
                            automap_y + tibia::client::automap_control_row_offset);
                    for (std::size_t index = 0;
                        index < game_screen_buttons.size(); ++index) {
                        const auto& button = game_screen_buttons[index].bounds;
                        if (contains_point(RECT{button.x, button.y,
                                button.x + button.width, button.y + button.height}, mouse)) {
                            host->pressed_game_screen_control =
                                static_cast<int>(index);
                            break;
                        }
                    }
                    if (layout->console_panel) {
                        const auto input =
                            tibia::client::resolve_console_text_input_visual(
                                *layout->console_panel, layout->console.x,
                                layout->console.y, true, false);
                        if (input) {
                            host->console_input_focused = contains_point(
                                RECT{input->rect.x, input->rect.y,
                                    input->rect.x + input->rect.width,
                                    input->rect.y + input->rect.height},
                                host->mouse);
                            if (host->console_input_focused) {
                                const auto text = std::wstring_view(
                                    host->console_input_text).substr(0,
                                        input->max_length);
                                host->console_input_caret =
                                    console_input_caret_for_x(*host, text,
                                        input->font_index, window,
                                        host->mouse.x - input->rect.x -
                                            input->inset);
                                host->console_input_anchor =
                                    host->console_input_caret;
                            }
                        }
                    }
                    const bool restoring = host->inventory_minimized;
                    const int button_x = layout->sidebar.x + (restoring
                        ? tibia::client::inventory_maximize_button_x
                        : tibia::client::inventory_minimize_button_x);
                    const int button_y = restoring
                        ? tibia::client::inventory_maximize_button_y
                        : tibia::client::inventory_minimize_button_y;
                    const RECT button{button_x, button_y,
                        button_x + tibia::client::inventory_window_button_size,
                        button_y + tibia::client::inventory_window_button_size};
                    if (contains_point(button, mouse))
                        host->pressed_inventory_control = restoring ? 1 : 0;
                    if (!restoring) {
                        const auto& icons = restoring
                            ? tibia::client::compact_combat_icons
                            : tibia::client::expanded_combat_icons;
                        for (std::size_t index = 0; index < icons.size(); ++index) {
                            if (contains_point(combat_control_rect(layout->sidebar.x,
                                    restoring, index), mouse)) {
                                host->pressed_combat_control = static_cast<int>(index);
                                break;
                            }
                        }
                    }
                    if (restoring) {
                        for (std::size_t index = 0;
                            index < tibia::client::compact_combat_icons.size(); ++index) {
                            if (contains_point(combat_control_rect(layout->sidebar.x,
                                    true, index), mouse)) {
                                host->pressed_combat_control = static_cast<int>(index);
                                break;
                            }
                        }
                    }
                    if (host->pressed_combat_control < 0) {
                        const auto action_count = restoring
                            ? tibia::client::compact_combat_actions.size() +
                                tibia::client::compact_combat_utility_icons.size()
                            : tibia::client::expanded_combat_actions.size();
                        for (std::size_t index = 0; index < action_count; ++index) {
                            if (contains_point(combat_action_rect(layout->sidebar.x,
                                    restoring, index), mouse)) {
                                host->pressed_combat_action = static_cast<int>(index);
                                break;
                            }
                        }
                    }
                }
            }
            if (message == WM_LBUTTONDOWN &&
                (host->pressed_automap_control >= 0 || host->pressed_automap_compass >= 0))
                InvalidateRect(window, nullptr, FALSE);
            SetCapture(window);
            host->mouse_buttons |= message == WM_LBUTTONDOWN ? 1U : message == WM_RBUTTONDOWN ? 2U : 4U;
        }
        break;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        if (host) {
            host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const bool automap_modal_open = !host->connection_status_text.empty() ||
                !host->notice_text.empty() || host->motd_open ||
                host->account_data_warning_open || host->new_channel_dialog_open ||
                host->help_open || host->hints_open || host->hint_content_open ||
                host->rule_violation_request_form.open ||
                host->report_violation_dialog.open || host->quest_log_open ||
                host->quest_line_open;
            if (message == WM_LBUTTONUP && host->console_resize_dragging) {
                host->console_resize_dragging = false;
                save_console_height(*host);
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                (host->pressed_automap_control >= 0 ||
                    host->pressed_automap_compass >= 0)) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int control = host->pressed_automap_control;
                const int compass_direction = host->pressed_automap_compass;
                host->pressed_automap_control = -1;
                host->pressed_automap_compass = -1;
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height,
                    tibia::client::map_window_minimum_height, host->no_stretching});
                if (!automap_modal_open && layout && host->screen == UiScreen::game_world) {
                    const int automap_y = host->inventory_minimized
                        ? tibia::client::automap_panel_minimized_inventory_y
                        : tibia::client::automap_panel_full_inventory_y;
                    const auto automap = tibia::client::layout_automap_panel(
                        layout->sidebar.x, automap_y);
                    tibia::map::AutomapAction action{};
                    bool activate = false;
                    if (control >= 0 && control < 4) {
                        const auto& bounds = automap.controls[static_cast<std::size_t>(control)].bounds;
                        if (contains_point(RECT{bounds.x, bounds.y,
                                bounds.x + bounds.width, bounds.y + bounds.height}, host->mouse)) {
                            constexpr std::array<tibia::map::AutomapAction, 4> actions{
                                tibia::map::AutomapAction::zoom_in,
                                tibia::map::AutomapAction::zoom_out,
                                tibia::map::AutomapAction::higher_floor,
                                tibia::map::AutomapAction::lower_floor};
                            action = actions[static_cast<std::size_t>(control)];
                            activate = true;
                        }
                    } else if (control == 4) {
                        const auto& bounds = automap.centre.bounds;
                        if (contains_point(RECT{bounds.x, bounds.y,
                                bounds.x + bounds.width, bounds.y + bounds.height}, host->mouse)) {
                            action = tibia::map::AutomapAction::center;
                            activate = true;
                        }
                    } else if (compass_direction >= 0 && compass_direction < 5 &&
                        contains_point(RECT{automap.compass.x, automap.compass.y,
                            automap.compass.x + automap.compass.width,
                            automap.compass.y + automap.compass.height}, host->mouse)) {
                        constexpr std::array<tibia::map::AutomapAction, 5> actions{
                            tibia::map::AutomapAction::center,
                            tibia::map::AutomapAction::north,
                            tibia::map::AutomapAction::south,
                            tibia::map::AutomapAction::west,
                            tibia::map::AutomapAction::east};
                        action = actions[static_cast<std::size_t>(compass_direction)];
                        activate = true;
                    }
                    if (activate) {
                        host->game_world_mailbox->apply_automap_action(action);
                        if (action == tibia::map::AutomapAction::zoom_in ||
                            action == tibia::map::AutomapAction::zoom_out) {
                            try {
                                auto preferences = tibia::client::read_communication_preferences(
                                    host->communication_config_path);
                                preferences.automap_zoom = host->game_world_mailbox->automap_zoom();
                                tibia::client::write_communication_preferences(
                                    host->communication_config_path, preferences);
                            } catch (const std::exception&) {
                            }
                        }
                    }
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                (host->console_transcript_selecting ||
                    host->pressed_console_transcript_scrollbar_part >= 0 ||
                    host->console_transcript_scrollbar_dragging)) {
                if (host->pressed_console_transcript_scrollbar_part == 0 ||
                    host->pressed_console_transcript_scrollbar_part == 1)
                    apply_console_transcript_arrow_click(*host, window,
                        host->mouse,
                        host->pressed_console_transcript_scrollbar_part);
                else if (host->pressed_console_transcript_scrollbar_part == 2)
                    apply_console_transcript_track_click(*host, window, host->mouse);
                host->console_transcript_selecting = false;
                host->pressed_console_transcript_scrollbar_part = -1;
                host->console_transcript_scrollbar_dragging = false;
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                host->pressed_console_context_menu_action >= 0) {
                const auto pressed = static_cast<
                    tibia::client::ConsoleContextMenuAction>(
                        host->pressed_console_context_menu_action);
                const auto* item = console_context_menu_item_at(*host,
                    host->mouse);
                host->pressed_console_context_menu_action = -1;
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                if (item && item->action == pressed)
                    apply_console_context_menu_action(*host, window, pressed);
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                host->pressed_vip_context_menu_action >= 0) {
                const auto pressed = static_cast<
                    tibia::client::VipContextAction>(
                        host->pressed_vip_context_menu_action);
                const auto* entry = vip_context_menu_entry_at(*host,
                    host->mouse);
                host->pressed_vip_context_menu_action = -1;
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                if (entry && entry->action == pressed)
                    apply_vip_context_menu_action(*host, window, pressed);
                else
                    host->vip_context_menu.reset();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                host->pressed_battle_context_menu_action >= 0) {
                const auto pressed = host->pressed_battle_context_menu_action;
                const auto creature_id = host->battle_context_menu
                    ? host->battle_context_menu->creature_id : 0;
                const auto* entry = battle_menu_entry_at(*host, host->mouse);
                if (entry && static_cast<int>(entry->action) == pressed)
                    apply_battle_target_command(*host, entry->action, creature_id);
                host->pressed_battle_context_menu_action = -1;
                host->battle_context_menu.reset();
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                host->pressed_battle_creature_id != 0) {
                const auto pressed = host->pressed_battle_creature_id;
                host->pressed_battle_creature_id = 0;
                if (battle_panel_creature_at(*host, window, host->mouse) == pressed) {
                    const auto action = host->battle_targets.attack_target == pressed
                        ? tibia::client::BattleTargetAction::stop_attack
                        : tibia::client::BattleTargetAction::attack;
                    apply_battle_target_command(*host, action, pressed);
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                host->pressed_container_header_id >= 0) {
                const auto id = static_cast<std::uint8_t>(
                    host->pressed_container_header_id);
                const auto button = host->pressed_container_header_button;
                host->pressed_container_header_id = -1;
                host->pressed_container_header_button = -1;
                RECT client{};
                GetClientRect(window, &client);
                const auto snapshot = host->game_world_mailbox->read();
                const auto game_layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height,
                    tibia::client::map_window_minimum_height,
                    host->no_stretching});
                if (snapshot && game_layout) {
                    const auto panels = container_panel_layouts(*host, window,
                        *snapshot, *game_layout);
                    const auto panel = std::find_if(panels.begin(), panels.end(),
                        [id](const auto& item) { return item.id == id; });
                    const auto state = std::find_if(
                        host->container_panel_states.begin(),
                        host->container_panel_states.end(), [id](const auto& item) {
                            return item.id == id;
                        });
                    if (panel != panels.end() && state !=
                            host->container_panel_states.end() && button == 1) {
                        const auto& bounds = panel->minimize_button;
                        if (contains_point(RECT{bounds.x, bounds.y,
                                bounds.x + bounds.width, bounds.y + bounds.height},
                                host->mouse)) {
                            if (!state->minimized) {
                                state->restore_height = panel->window.height;
                                state->height = panel->window.height;
                                state->minimized = true;
                            } else {
                                state->minimized = false;
                                state->height = std::max(
                                    tibia::client::container_panel_minimum_height,
                                    state->restore_height);
                            }
                        }
                    }
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->moving_container_id >= 0) {
                host->moving_container_id = -1;
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->resizing_container_id >= 0) {
                host->resizing_container_id = -1;
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->pressed_skills_header_button >= 0) {
                const auto pressed = host->pressed_skills_header_button;
                host->pressed_skills_header_button = -1;
                if (const auto plan = skills_panel_window_plan(*host, window)) {
                    const auto header = tibia::client::layout_skills_panel_header(
                        plan->window);
                    if (pressed == 0 && contains_point(
                            skills_rect(header.close_button), host->mouse)) {
                        const auto height = host->skills_window_minimized &&
                                host->skills_window_restore_height > 0
                            ? host->skills_window_restore_height
                            : plan->window.height;
                        save_skills_window_height(*host, height);
                        host->skills_window_open = false;
                        host->skills_window_minimized = false;
                        host->skills_window_restore_height = 0;
                        host->skills_window_resized_height = 0;
                        host->skills_context_menu.reset();
                    } else if (pressed == 1 && contains_point(
                            skills_rect(header.minimize_button), host->mouse)) {
                        if (!host->skills_window_minimized) {
                            host->skills_window_restore_height = plan->window.height;
                            host->skills_window_minimized = true;
                        } else {
                            host->skills_window_minimized = false;
                        }
                    }
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->skills_window_resize_dragging) {
                host->skills_window_resize_dragging = false;
                if (const auto plan = skills_panel_window_plan(*host, window))
                    save_skills_window_height(*host, plan->window.height);
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->battle_window_resize_dragging) {
                host->battle_window_resize_dragging = false;
                if (host->battle_window_resized_height > 0) {
                    host->battle_window_restore_height =
                        host->battle_window_resized_height;
                    save_battle_window_height(*host,
                        host->battle_window_resized_height);
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->vip_window_resize_dragging) {
                host->vip_window_resize_dragging = false;
                if (host->vip_window_resized_height > 0) {
                    host->vip_window_restore_height = host->vip_window_resized_height;
                    save_vip_window_height(*host, host->vip_window_resized_height);
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->pressed_battle_header_button >= 0) {
                const auto pressed = host->pressed_battle_header_button;
                host->pressed_battle_header_button = -1;
                if (const auto plan = battle_panel_window_plan(*host, window)) {
                    const auto header = tibia::client::layout_skills_panel_header(
                        plan->window);
                    if (pressed == 0 && contains_point(
                            skills_rect(header.close_button), host->mouse)) {
                        const auto height = host->battle_window_minimized &&
                                host->battle_window_restore_height > 0
                            ? host->battle_window_restore_height
                            : plan->window.height;
                        save_battle_window_height(*host, height);
                        host->battle_window_open = false;
                        host->battle_window_minimized = false;
                        host->battle_window_restore_height = 0;
                        host->battle_window_natural_height = 0;
                        host->battle_window_resized_height = 0;
                    } else if (pressed == 1 && contains_point(
                            skills_rect(header.minimize_button), host->mouse)) {
                        if (!host->battle_window_minimized) {
                            host->battle_window_restore_height = plan->window.height;
                            host->battle_window_minimized = true;
                        } else {
                            host->battle_window_minimized = false;
                            host->battle_window_resized_height =
                                host->battle_window_restore_height;
                        }
                    }
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->pressed_vip_header_button >= 0) {
                const auto pressed = host->pressed_vip_header_button;
                host->pressed_vip_header_button = -1;
                if (const auto plan = vip_panel_window_plan(*host, window)) {
                    const auto header = tibia::client::layout_skills_panel_header(
                        plan->window);
                    if (pressed == 0 && contains_point(
                            skills_rect(header.close_button), host->mouse)) {
                        const auto height = host->vip_window_minimized &&
                                host->vip_window_restore_height > 0
                            ? host->vip_window_restore_height
                            : plan->window.height;
                        save_vip_window_height(*host, height);
                        host->vip_window_open = false;
                        host->vip_window_minimized = false;
                        host->vip_window_restore_height = 0;
                        host->vip_window_natural_height = 0;
                        host->vip_window_resized_height = 0;
                    } else if (pressed == 1 && contains_point(
                            skills_rect(header.minimize_button), host->mouse)) {
                        if (!host->vip_window_minimized) {
                            host->vip_window_restore_height = plan->window.height;
                            host->vip_window_minimized = true;
                        } else {
                            host->vip_window_minimized = false;
                            host->vip_window_resized_height =
                                host->vip_window_restore_height;
                        }
                    }
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                (host->pressed_battle_scrollbar_part >= 0 ||
                    host->battle_scrollbar_dragging)) {
                host->pressed_battle_scrollbar_part = -1;
                host->battle_scrollbar_dragging = false;
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                (host->pressed_vip_scrollbar_part >= 0 ||
                    host->vip_scrollbar_dragging)) {
                host->pressed_vip_scrollbar_part = -1;
                host->vip_scrollbar_dragging = false;
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP &&
                host->pressed_skills_context_menu_action >= 0) {
                const auto action = host->pressed_skills_context_menu_action;
                const auto* entry = skills_menu_entry_at(*host, host->mouse);
                if (entry && entry->action == action)
                    apply_skills_context_menu_action(*host,
                        static_cast<std::uint16_t>(action));
                host->pressed_skills_context_menu_action = -1;
                host->skills_context_menu.reset();
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            const bool notice_open = !host->notice_text.empty();
            const bool modal_open = !host->connection_status_text.empty() ||
                notice_open || host->motd_open ||
                host->account_data_warning_open || host->help_open ||
                host->hints_open || host->hint_content_open ||
                host->rule_violation_request_form.open ||
                host->report_violation_dialog.open ||
                host->quest_log_open || host->quest_line_open;
            if (message == WM_LBUTTONUP)
                host->dragged_dialog = ClientHost::DraggedDialog::none;
            if (message == WM_LBUTTONUP && host->new_channel_dialog_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_new_channel_button;
                if (pressed == 0 && contains_point(new_channel_button_rect(*host, 0),
                    host->mouse))
                    submit_new_channel_dialog(*host);
                else if (pressed == 1 && contains_point(new_channel_button_rect(*host, 1),
                    host->mouse)) {
                    host->new_channel_dialog_open = false;
                    host->private_channel_name.clear();
                    host->private_channel_focused = false;
                    host->new_channel_dialog_offset = {};
                    host->new_channel_scrollbar_dragging = false;
                    host->pressed_new_channel_scroll_button = -1;
                }
                host->pressed_new_channel_button = -1;
                host->pressed_new_channel_scroll_button = -1;
                host->new_channel_scrollbar_dragging = false;
                ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && notice_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                if (host->pressed_notice_button == 0 && contains_point(
                        notice_button_rect(*host), host->mouse)) {
                    host->notice_text.clear();
                    host->notice_title = L"Tibia";
                    host->notice_dialog_offset = {};
                }
                host->pressed_notice_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !notice_open && host->motd_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_motd_button;
                if (pressed == 0 && contains_point(
                    motd_button_rect(*host, pressed), host->mouse))
                    host->motd_open = false;
                host->pressed_motd_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !notice_open &&
                host->account_data_warning_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                if (host->pressed_account_data_warning_button == 0 && contains_point(
                    account_data_warning_button_rect(*host), host->mouse)) {
                    host->account_data_warning_open = false;
                    host->account_data_warning_dialog_offset = {};
                }
                host->pressed_account_data_warning_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !notice_open &&
                host->rule_violation_request_form.open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_rule_violation_request_button;
                if (host->pressed_rule_violation_request_checkbox &&
                    host->rule_violation_request_form.ip_banishment_enabled &&
                    contains_point(rule_violation_request_checkbox_rect(window,
                        host->rule_violation_request_dialog_offset), host->mouse))
                    host->rule_violation_request_form.set_ip_banishment(
                        !host->rule_violation_request_form.ip_banishment);
                if (pressed >= 0 && pressed < 2 && contains_point(
                        rule_violation_request_button_rect(window,
                            host->rule_violation_request_dialog_offset, pressed),
                        host->mouse)) {
                    if (pressed == 0) {
                        submit_rule_violation_request(*host);
                    } else {
                        host->rule_violation_request_form.close();
                        KillTimer(window, login_caret_timer_id);
                        host->rule_violation_request_focus_field = -1;
                        host->rule_violation_request_dialog_offset = {};
                        KillTimer(window, login_caret_timer_id);
                    }
                }
                host->pressed_rule_violation_request_button = -1;
                host->pressed_rule_violation_request_checkbox = false;
                finish_options_list_scrollbar_interaction(*host);
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && !notice_open && host->report_violation_dialog.open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->report_violation_dialog.pressed_button;
                if (pressed >= 0 && pressed < 2 && contains_point(
                    report_violation_button_rect(window,
                        host->report_rule_violation_dialog_offset, pressed), host->mouse)) {
                    if (pressed == 0 && !host->report_violation_dialog.text.empty()) {
                        if (host->report_violation_buffer &&
                            host->report_violation_buffer->is_open()) {
                            host->notice_text =
                                L"You have already submitted a request. Please be patient.";
                        } else if (host->submit_rule_violation) {
                            try {
                                host->console_transcript_scroll_from_bottom = 0;
                                host->submit_rule_violation(host->report_violation_dialog.text);
                            } catch (const std::exception& error) {
                                host->notice_text = error_to_wide(error.what());
                            }
                        }
                    }
                    host->report_violation_dialog.close();
                    KillTimer(window, login_caret_timer_id);
                }
                host->report_violation_dialog.pressed_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !notice_open && host->hint_content_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                if (host->pressed_hint_content_button == 0 && contains_point(
                    hint_content_ok_rect(window, host->hint_content_dialog_offset),
                    host->mouse)) {
                    host->hint_content_open = false;
                    host->active_hint = nullptr;
                }
                host->pressed_hint_content_button = -1;
                host->pressed_hint_content_scroll_part = -1;
                host->hint_content_scroll_dragging = false;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !notice_open &&
                !host->hint_content_open && host->hints_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_hints_button;
                if (pressed >= 0 && pressed < 2 && contains_point(
                    hints_button_rect(window, host->hints_dialog_offset, pressed),
                    host->mouse)) {
                    if (pressed == 0 && host->hints_selected >= 0 &&
                        host->hints_selected < static_cast<int>(host->hints.size())) {
                        host->active_hint = tibia::client::find_hint(
                            host->hints[static_cast<std::size_t>(host->hints_selected)].id);
                        if (host->active_hint) {
                            const auto view = hint_content_view_rect(window);
                            host->hint_content_lines = hint_document_lines(*host,
                                host->active_hint->content,
                                view.right - view.left - ui_units(window, 21));
                            host->hint_content_dialog_offset = {};
                            host->hint_content_scroll = 0;
                            host->hint_content_open = true;
                        }
                    } else if (pressed == 1) {
                        host->hints_open = false;
                    }
                }
                host->pressed_hints_button = -1;
                host->pressed_hints_scroll_part = -1;
                host->hints_scroll_dragging = false;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !notice_open && !host->hints_open &&
                !host->report_violation_dialog.open && host->help_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                if (host->pressed_help_button == 6 && contains_point(
                    help_ok_rect(window, host->help_dialog_offset), host->mouse))
                    host->help_open = false;
                else if (host->pressed_help_button == 1 && contains_point(
                    help_action_button_rect(window, host->help_dialog_offset, 1),
                    host->mouse)) {
                    const auto preferences =
                        tibia::client::read_communication_preferences(
                            host->communication_config_path);
                    host->hints = tibia::client::available_hints(
                        preferences.known_hints);
                    host->hints_selected = -1;
                    host->hints_scroll = 0;
                    host->hints_dialog_offset = {};
                    host->pressed_hints_button = -1;
                    host->hints_open = true;
                }
                else if (host->pressed_help_button == 2) {
                    const auto dialog = help_dialog_rect(window, host->help_dialog_offset);
                    const int content_x = dialog.left + ui_units(window, 27);
                    const int top = dialog.top + ui_units(window, 41) + ui_units(window, 66);
                    const RECT button{content_x, top, content_x + ui_units(window, 86),
                        top + ui_units(window, 20)};
                    if (contains_point(button, host->mouse) && host->request_tutor_help) {
                        try {
                            host->request_tutor_help();
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    }
                }
                else if (host->pressed_help_button == 3) {
                    if (contains_point(help_action_button_rect(window,
                        host->help_dialog_offset, 3), host->mouse)) {
                        if (host->report_violation_buffer &&
                            host->report_violation_buffer->is_open()) {
                            host->notice_text =
                                L"You have already submitted a request. Please close the current request.";
                        } else {
                            host->report_rule_violation_dialog_offset = {};
                            host->report_violation_dialog.show();
                            SetTimer(window, login_caret_timer_id, 100, nullptr);
                        }
                    }
                }
                else if (host->pressed_help_button == 4 && contains_point(
                    help_action_button_rect(window, host->help_dialog_offset, 4), host->mouse)) {
                    ShellExecuteW(window, L"open",
                        L"http://www.tibia.com/gameguides/?subtopic=manual", nullptr, nullptr,
                        SW_SHOWNORMAL);
                }
                else if (host->pressed_help_button == 5 && contains_point(
                    help_action_button_rect(window, host->help_dialog_offset, 5), host->mouse)) {
                    ShellExecuteW(window, L"open",
                        L"http://www.tibia.com/support/?subtopic=faq", nullptr, nullptr,
                        SW_SHOWNORMAL);
                }
                host->pressed_help_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !notice_open && host->quest_line_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                finish_quest_scroll_interaction(*host, host->mouse);
                if (host->pressed_quest_line_button == 0 && contains_point(
                    quest_line_close_rect(window, host->quest_line_dialog_offset), host->mouse))
                    host->quest_line_open = false;
                host->pressed_quest_line_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            } else if (message == WM_LBUTTONUP && !notice_open && host->quest_log_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                finish_quest_scroll_interaction(*host, host->mouse);
                const int pressed = host->pressed_quest_log_button;
                if (pressed >= 0 && pressed < 2 && contains_point(
                    quest_log_button_rect(window, host->quest_log_dialog_offset, pressed),
                    host->mouse)) {
                    if (pressed == 1) {
                        host->quest_log_open = false;
                    } else if (host->quest_log_selected >= 0) {
                        const auto snapshot = host->game_world_mailbox->read();
                        if (snapshot && static_cast<std::size_t>(host->quest_log_selected) <
                            snapshot->quest_log_entries.size()) {
                            host->quest_line_id = snapshot->quest_log_entries[
                                static_cast<std::size_t>(host->quest_log_selected)].id;
                            host->quest_flag_selected = -1;
                            host->quest_flag_scroll = 0;
                            if (!host->last_quest_flags_request_id ||
                                *host->last_quest_flags_request_id != host->quest_line_id) {
                                host->quest_line_request_revision =
                                    snapshot->quest_flags_revision;
                                host->quest_line_flags_pending =
                                    static_cast<bool>(host->request_quest_flags);
                                if (host->request_quest_flags) {
                                    try {
                                        host->request_quest_flags(host->quest_line_id);
                                        host->last_quest_flags_request_id = host->quest_line_id;
                                    } catch (const std::exception& error) {
                                        host->notice_text = error_to_wide(error.what());
                                    }
                                }
                            }
                            host->quest_line_dialog_offset = {};
                            host->quest_line_open = true;
                        }
                    }
                }
                host->pressed_quest_log_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open && host->info_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_info_button;
                const bool clicked = (pressed == 0 && contains_point(
                    info_ok_rect(window, host->info_dialog_offset), host->mouse));
                const auto website_button = info_website_rect(window, host->info_dialog_offset);
                const bool visit_site = pressed == 2 && contains_point(website_button, host->mouse);
                if (clicked)
                    host->info_open = false;
                host->pressed_info_button = -1;
                if (visit_site) ShellExecuteW(window, L"open", L"http://www.tibia.com/",
                    nullptr, nullptr, SW_SHOWNORMAL);
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open &&
                host->advanced_graphics_options_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_advanced_graphics_control;
                if (pressed >= 0 && pressed < 3 && !host->graphics_fullscreen_value &&
                    contains_point(advanced_graphics_engine_button_rect(window,
                        host->advanced_graphics_options_dialog_offset,
                        static_cast<std::size_t>(pressed)), host->mouse)) {
                    constexpr std::array<int, 3> engine_values{0, 2, 1};
                    try {
                        select_graphics_backend(*host,
                            static_cast<tibia::render::GraphicsBackend>(
                                engine_values[static_cast<std::size_t>(pressed)]));
                    } catch (const std::exception& error) {
                        host->notice_text = error_to_wide(error.what());
                    }
                } else if (pressed == 4 && contains_point(advanced_graphics_checkbox_rect(
                    window, host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_light_effects_y), host->mouse)) {
                    host->advanced_graphics_light_effects =
                        !host->advanced_graphics_light_effects;
                } else if (pressed == 5 && contains_point(advanced_graphics_checkbox_rect(
                    window, host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_stretch_y), host->mouse)) {
                    host->advanced_graphics_no_stretching =
                        !host->advanced_graphics_no_stretching;
                } else if (pressed == 6 && contains_point(advanced_graphics_checkbox_rect(
                    window, host->advanced_graphics_options_dialog_offset,
                    tibia::client::advanced_graphics_mouse_pointer_y), host->mouse)) {
                    host->advanced_graphics_windows_mouse_pointer =
                        !host->advanced_graphics_windows_mouse_pointer;
                } else if ((pressed == 13 || pressed == 14 || pressed == 17 || pressed == 18)) {
                    const bool ambient = pressed == 13 || pressed == 14;
                    const auto slider = advanced_graphics_slider_rect(window,
                        host->advanced_graphics_options_dialog_offset,
                        ambient ? tibia::client::advanced_graphics_ambient_slider_y :
                            tibia::client::advanced_graphics_framerate_slider_y);
                    if (contains_point(slider, host->mouse)) {
                        const int delta = (pressed == 13 || pressed == 17) ? -1 : 1;
                        if (ambient) host->advanced_graphics_brightness = std::clamp(
                            host->advanced_graphics_brightness + delta, 0, 128);
                        else {
                            host->advanced_graphics_framerate = std::clamp(
                                host->advanced_graphics_framerate + delta,
                                tibia::client::advanced_graphics_slider_minimum,
                                tibia::client::advanced_graphics_slider_maximum);
                            host->framerate_limit = host->advanced_graphics_framerate;
                        }
                    }
                } else if (pressed >= 8 && pressed <= 10 && contains_point(
                    advanced_graphics_button_rect(window,
                        host->advanced_graphics_options_dialog_offset,
                        static_cast<std::size_t>(pressed - 8)), host->mouse)) {
                    if (pressed == 8) {
                        ShellExecuteW(window, L"open",
                            L"http://www.tibia.com/gameguides/?subtopic=manual&section=options#graphicsoptions",
                            nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (pressed == 9) {
                        const auto previous_backend = host->renderer->backend();
                        try {
                            auto preferences = tibia::client::read_communication_preferences(
                                host->communication_config_path);
                            activate_graphics_backend(*host,
                                static_cast<tibia::render::GraphicsBackend>(
                                    host->advanced_graphics_engine));
                            preferences.basic_brightness = host->advanced_graphics_brightness;
                            preferences.confine_framerate = host->advanced_graphics_framerate;
                            preferences.engine = host->advanced_graphics_engine;
                            preferences.light_effects = host->advanced_graphics_light_effects;
                            preferences.no_stretching = host->advanced_graphics_no_stretching;
                            preferences.windows_mouse_pointer =
                                host->advanced_graphics_windows_mouse_pointer;
                            tibia::client::write_communication_preferences(
                                host->communication_config_path, preferences);
                            host->ambient_light = host->advanced_graphics_brightness;
                            host->show_light_effects = host->advanced_graphics_light_effects;
                            host->no_stretching = host->advanced_graphics_no_stretching;
                            host->windows_mouse_pointer =
                                host->advanced_graphics_windows_mouse_pointer;
                            host->framerate_limit = host->advanced_graphics_framerate;
                            host->advanced_graphics_options_open = false;
                        } catch (const std::exception& error) {
                            if (host->renderer->backend() != previous_backend) {
                                try { activate_graphics_backend(*host, previous_backend); }
                                catch (...) {}
                            }
                            host->framerate_limit = host->advanced_graphics_original_framerate;
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else {
                        host->framerate_limit = host->advanced_graphics_original_framerate;
                        host->advanced_graphics_options_open = false;
                    }
                }
                host->pressed_advanced_graphics_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open && host->general_options_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_general_options_control;
                if (pressed >= 0 && pressed < 5 && contains_point(
                    general_options_row_rect(window, host->general_options_dialog_offset,
                        static_cast<std::size_t>(pressed)), host->mouse)) {
                    host->general_options_values[static_cast<std::size_t>(pressed)] =
                        !host->general_options_values[static_cast<std::size_t>(pressed)];
                } else if (pressed >= 5 && pressed <= 6 && contains_point(
                    general_options_button_rect(window, host->general_options_dialog_offset,
                        static_cast<std::size_t>(pressed - 5)), host->mouse)) {
                    if (pressed == 5) {
                        try {
                            auto preferences = tibia::client::read_communication_preferences(
                                host->communication_config_path);
                            preferences.classic_control = host->general_options_values[0];
                            preferences.auto_chase_off = host->general_options_values[1];
                            preferences.show_new_hints = host->general_options_values[2];
                            preferences.creature_info = host->general_options_values[3] ? 2 : 0;
                            host->creature_info_mode = preferences.creature_info;
                            preferences.textual_effects = host->general_options_values[4];
                            tibia::client::write_communication_preferences(
                                host->communication_config_path, preferences);
                            host->show_textual_effects = preferences.textual_effects;
                            host->general_options_open = false;
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else {
                        host->general_options_open = false;
                    }
                }
                host->pressed_general_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open && host->network_options_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_network_options_control;
                if (pressed == 0 && contains_point(network_options_row_rect(window,
                    host->network_options_dialog_offset, 0), host->mouse)) {
                    host->network_options_value = !host->network_options_value;
                } else if (pressed >= 1 && pressed <= 3 && contains_point(
                    network_options_button_rect(window, host->network_options_dialog_offset,
                        static_cast<std::size_t>(pressed - 1)), host->mouse)) {
                    if (pressed == 1) {
                        ShellExecuteW(window, L"open",
                            L"http://www.tibia.com/gameguides/?subtopic=manual&section=options#networkoptions",
                            nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (pressed == 2) {
                        try {
                            auto preferences = tibia::client::read_communication_preferences(
                                host->communication_config_path);
                            preferences.test_server_login = host->network_options_value;
                            tibia::client::write_communication_preferences(
                                host->communication_config_path, preferences);
                            host->test_server_login = host->network_options_value;
                            host->network_options_open = false;
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else {
                        host->network_options_open = false;
                    }
                }
                host->pressed_network_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open && host->graphics_options_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_graphics_options_control;
                if (pressed == 0 && contains_point(graphics_options_fullscreen_rect(window,
                    host->graphics_options_dialog_offset), host->mouse)) {
                    host->graphics_fullscreen_value = !host->graphics_fullscreen_value;
                } else if (pressed == 4 && contains_point(graphics_options_advanced_rect(window,
                    host->graphics_options_dialog_offset), host->mouse)) {
                    try {
                        load_advanced_graphics_options(*host);
                    } catch (const std::exception& error) {
                        host->notice_text = error_to_wide(error.what());
                    }
                } else if (pressed >= 1 && pressed <= 3 && contains_point(
                    graphics_options_button_rect(window, host->graphics_options_dialog_offset,
                        static_cast<std::size_t>(pressed - 1)), host->mouse)) {
                    if (pressed == 1) {
                        ShellExecuteW(window, L"open",
                            L"http://www.tibia.com/gameguides/?subtopic=manual&section=options#graphicsoptions",
                            nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (pressed == 2) {
                        const bool previous_fullscreen = host->fullscreen_window;
                        const int previous_mode = host->graphics_initial_selected_mode;
                        try {
                            const tibia::client::GraphicsDisplayMode* selected_mode = nullptr;
                            if (host->graphics_selected_mode >= 0 &&
                                static_cast<std::size_t>(host->graphics_selected_mode) <
                                    host->graphics_display_modes.size()) {
                                selected_mode = &host->graphics_display_modes[
                                    static_cast<std::size_t>(host->graphics_selected_mode)];
                            }
                            if (host->graphics_fullscreen_value && !selected_mode)
                                throw std::runtime_error("no display mode is selected");
                            set_window_fullscreen(*host, host->graphics_fullscreen_value,
                                selected_mode);
                            if (selected_mode) {
                                auto preferences = tibia::client::read_communication_preferences(
                                    host->communication_config_path);
                                preferences.fullscreen_mode = {selected_mode->width,
                                    selected_mode->height, selected_mode->bits_per_pixel};
                                preferences.fullscreen_mode_configured = true;
                                tibia::client::write_communication_preferences(
                                    host->communication_config_path, preferences);
                            }
                            host->graphics_options_open = false;
                        } catch (const std::exception& error) {
                            try {
                                const tibia::client::GraphicsDisplayMode* previous = nullptr;
                                if (previous_mode >= 0 && static_cast<std::size_t>(previous_mode) <
                                    host->graphics_display_modes.size())
                                    previous = &host->graphics_display_modes[
                                        static_cast<std::size_t>(previous_mode)];
                                set_window_fullscreen(*host, previous_fullscreen, previous);
                            } catch (...) {
                                ChangeDisplaySettingsW(nullptr, 0);
                            }
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else {
                        host->graphics_options_open = false;
                    }
                }
                host->pressed_graphics_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open && host->console_options_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_console_options_control;
                if (pressed >= 0 && pressed < 6 && contains_point(
                    console_options_row_rect(window, host->console_options_dialog_offset,
                        static_cast<std::size_t>(pressed)), host->mouse)) {
                    host->console_options_values[static_cast<std::size_t>(pressed)] =
                        !host->console_options_values[static_cast<std::size_t>(pressed)];
                } else if (pressed >= 6 && pressed <= 8 && contains_point(
                    console_options_button_rect(window, host->console_options_dialog_offset,
                        static_cast<std::size_t>(pressed - 6)), host->mouse)) {
                    if (pressed == 6) {
                        ShellExecuteW(window, L"open",
                            L"http://www.tibia.com/gameguides/?subtopic=manual&section=options#consoleoptions",
                            nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (pressed == 7) {
                        try {
                            auto preferences = tibia::client::read_communication_preferences(
                                host->communication_config_path);
                            preferences.routing.mode_16_enabled = host->console_options_values[0];
                            preferences.routing.mode_13_and_14_enabled = host->console_options_values[1];
                            preferences.routing.mode_15_enabled = host->console_options_values[2];
                            preferences.display.timestamps_enabled = host->console_options_values[3];
                            preferences.display.levels_enabled = host->console_options_values[4];
                            preferences.display.private_messages_enabled = host->console_options_values[5];
                            tibia::client::write_communication_preferences(
                                host->communication_config_path, preferences);
                            host->console_options_open = false;
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else {
                        host->console_options_open = false;
                    }
                }
                host->pressed_console_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open && host->hotkey_options_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_hotkey_options_control;
                if (pressed == 7 && contains_point(hotkey_options_control_rect(window,
                    host->hotkey_options_dialog_offset, 0,
                    tibia::client::hotkey_automatic_y, 0,
                    tibia::client::hotkey_automatic_height), host->mouse)) {
                    host->hotkey_send_automatically = !host->hotkey_send_automatically;
                    commit_hotkey_editor(*host);
                } else if (pressed == 1 && contains_point(hotkey_options_control_rect(window,
                    host->hotkey_options_dialog_offset, tibia::client::hotkey_clear_x,
                    tibia::client::hotkey_clear_y, tibia::client::hotkey_button_width,
                    tibia::client::hotkey_button_height), host->mouse)) {
                    tibia::client::clear_hotkey(host->hotkey_options_values[host->selected_hotkey]);
                    host->hotkey_text.clear();
                    host->hotkey_send_automatically = false;
                } else if (pressed >= 2 && pressed <= 4 && contains_point(
                    hotkey_options_control_rect(window, host->hotkey_options_dialog_offset,
                        tibia::client::hotkey_mode_x,
                        tibia::client::hotkey_mode_y[static_cast<std::size_t>(pressed - 2)],
                        tibia::client::hotkey_button_width,
                        tibia::client::hotkey_button_height), host->mouse)) {
                    tibia::client::set_hotkey_object_mode(
                        host->hotkey_options_values[host->selected_hotkey],
                        pressed == 2 ? 2 : pressed == 3 ? 1 : 0);
                } else if (pressed == 5 && contains_point(hotkey_options_footer_button_rect(
                    window, host->hotkey_options_dialog_offset, 0), host->mouse)) {
                    commit_hotkey_editor(*host);
                    try {
                        auto preferences = tibia::client::read_communication_preferences(
                            host->communication_config_path);
                        preferences.hotkeys = host->hotkey_options_values;
                        tibia::client::write_communication_preferences(
                            host->communication_config_path, preferences);
                        host->hotkey_options_open = false;
                        host->hotkey_text_focused = false;
                        host->pending_hotkey_object_selection.reset();
                        KillTimer(window, login_caret_timer_id);
                    } catch (const std::exception& error) {
                        host->notice_text = error_to_wide(error.what());
                    }
                } else if (pressed == 6 && contains_point(hotkey_options_footer_button_rect(
                    window, host->hotkey_options_dialog_offset, 1), host->mouse)) {
                    host->hotkey_options_open = false;
                    host->hotkey_text_focused = false;
                    KillTimer(window, login_caret_timer_id);
                } else if (pressed == 0 && contains_point(hotkey_options_control_rect(window,
                    host->hotkey_options_dialog_offset, tibia::client::hotkey_select_x,
                    tibia::client::hotkey_select_y, tibia::client::hotkey_button_width,
                    tibia::client::hotkey_button_height), host->mouse)) {
                    commit_hotkey_editor(*host);
                    host->pending_hotkey_object_selection = host->selected_hotkey;
                    host->interaction_mode = tibia::client::hotkey_object_selection_mode;
                    host->hotkey_options_open = false;
                    host->hotkey_text_focused = false;
                    KillTimer(window, login_caret_timer_id);
                }
                host->pressed_hotkey_options_control = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open && host->screen == UiScreen::login) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_login_button;
                if (pressed == 0 && contains_point(login_ok_rect(window, host->login_dialog_offset), host->mouse))
                    PostMessageW(window, WM_COMMAND, MAKEWPARAM(login_button_id, BN_CLICKED), 0);
                else if (pressed == 1 && contains_point(login_cancel_rect(window, host->login_dialog_offset), host->mouse))
                    show_screen(*host, UiScreen::main_menu);
                host->pressed_login_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open &&
                host->screen == UiScreen::character_selection) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_login_button;
                if (pressed == 0 && contains_point(character_ok_rect(window,
                    host->character_selection_dialog_offset), host->mouse))
                    PostMessageW(window, WM_COMMAND, MAKEWPARAM(1006, BN_CLICKED), 0);
                else if (pressed == 1 && contains_point(character_cancel_rect(window,
                    host->character_selection_dialog_offset), host->mouse))
                    PostMessageW(window, WM_COMMAND, MAKEWPARAM(back_button_id, BN_CLICKED), 0);
                host->pressed_login_button = -1;
                host->pressed_character_scroll_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open &&
                host->options_open &&
                !host->console_options_open && !host->general_options_open &&
                !host->network_options_open && !host->graphics_options_open &&
                !host->hotkey_options_open) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const auto dialog = options_rect(window, host->options_dialog_offset);
                const float scale = ui_scale(window);
                const auto s = [scale](int value) {
                    return std::max(1, static_cast<int>(std::lround(static_cast<float>(value) * scale)));
                };
                const int pressed = host->pressed_options_button;
                constexpr int row_y[]{32, 65, 98, 131, 164, 214};
                if (pressed >= 0 && pressed < 6) {
                    RECT button{dialog.left + s(18), dialog.top + s(row_y[pressed]),
                        dialog.left + s(104), dialog.top + s(row_y[pressed] + 20)};
                    if (contains_point(button, host->mouse) && pressed == 0) {
                        try {
                            const auto preferences = tibia::client::read_communication_preferences(
                                host->communication_config_path);
                            host->general_options_values = {
                                preferences.classic_control,
                                preferences.auto_chase_off,
                                preferences.show_new_hints,
                                preferences.creature_info != 0,
                                preferences.textual_effects};
                            host->general_options_dialog_offset = {};
                            host->general_options_open = true;
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else if (contains_point(button, host->mouse) && pressed == 1) {
                        try {
                            load_graphics_options(*host);
                            host->graphics_options_dialog_offset = {};
                            host->graphics_options_open = true;
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else if (contains_point(button, host->mouse) && pressed == 3) {
                        try {
                            const auto preferences = tibia::client::read_communication_preferences(
                                host->communication_config_path);
                            host->network_options_value = preferences.test_server_login;
                            host->network_options_dialog_offset = {};
                            host->network_options_open = true;
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else if (contains_point(button, host->mouse) && pressed == 2) {
                        try {
                            const auto preferences = tibia::client::read_communication_preferences(
                                host->communication_config_path);
                            host->console_options_values = {
                                preferences.routing.mode_16_enabled,
                                preferences.routing.mode_13_and_14_enabled,
                                preferences.routing.mode_15_enabled,
                                preferences.display.timestamps_enabled,
                                preferences.display.levels_enabled,
                                preferences.display.private_messages_enabled};
                            host->console_options_dialog_offset = {};
                            host->console_options_open = true;
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else if (contains_point(button, host->mouse) && pressed == 4) {
                        try {
                            if (!host->pending_hotkey_object_selection) {
                                const auto preferences = tibia::client::read_communication_preferences(
                                    host->communication_config_path);
                                host->hotkey_options_values = preferences.hotkeys;
                                host->selected_hotkey = 0;
                                host->hotkey_list_scroll = 0;
                                host->hotkey_text.clear();
                                host->hotkey_send_automatically = false;
                            }
                            host->hotkey_text_focused = false;
                            select_hotkey(*host, host->pending_hotkey_object_selection.value_or(
                                host->selected_hotkey));
                            host->hotkey_options_dialog_offset = {};
                            host->pressed_hotkey_options_control = -1;
                            host->hotkey_options_open = true;
                            host->login_caret_phase = 0;
                            SetTimer(window, login_caret_timer_id, 100, nullptr);
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else if (contains_point(button, host->mouse) && pressed == 5) {
                        const auto status = host->status_line_state.snapshot();
                        host->motd_text = status.has_text
                            ? decode_ui_string(status.text)
                            : L"No current information.";
                        host->motd_dialog_offset = {};
                        host->pressed_motd_button = -1;
                        host->motd_open = true;
                    }
                } else if (pressed == 6) {
                    RECT ok{dialog.left + s(230), dialog.top + s(264),
                        dialog.left + s(273), dialog.top + s(284)};
                    if (contains_point(ok, host->mouse)) {
                        host->options_open = false;
                        if (host->options_from_game) host->options_from_game = false;
                        else show_screen(*host, UiScreen::main_menu);
                    }
                }
                host->pressed_options_button = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open &&
                host->screen == UiScreen::main_menu && !host->options_open &&
                !host->info_open &&
                host->pressed_menu_index >= 0) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const auto menu = menu_rect(window);
                const auto scale = ui_scale(window);
                const int index = host->pressed_menu_index;
                RECT button{menu.left + static_cast<int>(std::lround(15.0F * scale)),
                    menu.top + static_cast<int>(std::lround((15.0F + 30.0F * index) * scale)), 0, 0};
                button.right = button.left + static_cast<int>(std::lround(86.0F * scale));
                button.bottom = button.top + static_cast<int>(std::lround(20.0F * scale));
                if (PtInRect(&button, host->mouse))
                    PostMessageW(window, WM_COMMAND, MAKEWPARAM(menu_button_ids[index], BN_CLICKED), 0);
                host->pressed_menu_index = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP &&
                host->pressed_game_screen_control >= 0) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const auto pressed = host->pressed_game_screen_control;
                host->pressed_game_screen_control = -1;
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height,
                    tibia::client::map_window_minimum_height, host->no_stretching});
                if (!modal_open && layout && host->screen == UiScreen::game_world) {
                    const int automap_y = host->inventory_minimized
                        ? tibia::client::automap_panel_minimized_inventory_y
                        : tibia::client::automap_panel_full_inventory_y;
                    const auto buttons =
                        tibia::client::layout_game_screen_control_buttons(
                            layout->sidebar.x,
                            automap_y + tibia::client::automap_control_row_offset);
                    const auto& bounds = buttons[static_cast<std::size_t>(pressed)].bounds;
                    const RECT button{bounds.x, bounds.y,
                        bounds.x + bounds.width, bounds.y + bounds.height};
                    if (contains_point(button, host->mouse) && pressed < 3) {
                        auto& open = pressed == 0
                            ? host->skills_window_open
                            : pressed == 1 ? host->battle_window_open
                                          : host->vip_window_open;
                        const auto type = pressed == 0
                            ? tibia::client::ContentWindowType::skills
                            : pressed == 1 ? tibia::client::ContentWindowType::battle
                                           : tibia::client::ContentWindowType::vip;
                        const bool opening = !open;
                        if (opening) remember_content_window_heights(*host, window);
                        open = !open;
                        if (open && std::find_if(
                                host->restored_content_windows.begin(),
                                host->restored_content_windows.end(),
                                [type](const auto& item) {
                                    return item.type == type;
                                }) == host->restored_content_windows.end())
                            host->restored_content_windows.push_back({type, 0});
                        if (open) remember_content_window_height(*host, window, type);
                    }
                }
                host->mouse_buttons &= ~1U;
                if (!host->mouse_buttons) ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (message == WM_LBUTTONUP && host->pressed_inventory_control >= 0) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_inventory_control;
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height, tibia::client::map_window_minimum_height,
                    host->no_stretching});
                if (!modal_open && layout && host->screen == UiScreen::game_world) {
                    const bool restoring = pressed == 1;
                    const int button_x = layout->sidebar.x + (restoring
                        ? tibia::client::inventory_maximize_button_x
                        : tibia::client::inventory_minimize_button_x);
                    const int button_y = restoring
                        ? tibia::client::inventory_maximize_button_y
                        : tibia::client::inventory_minimize_button_y;
                    const RECT button{button_x, button_y,
                        button_x + tibia::client::inventory_window_button_size,
                        button_y + tibia::client::inventory_window_button_size};
                    if (contains_point(button, host->mouse)) {
                        host->inventory_minimized = !restoring;
                        InvalidateRect(window, nullptr, FALSE);
                    }
                }
                host->pressed_inventory_control = -1;
            }
            if (message == WM_LBUTTONUP && host->pressed_combat_control >= 0) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_combat_control;
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height, tibia::client::map_window_minimum_height,
                    host->no_stretching});
                const bool compact = host->inventory_minimized;
                const auto control_count = compact
                    ? tibia::client::compact_combat_icons.size()
                    : tibia::client::expanded_combat_icons.size();
                if (!modal_open && layout && static_cast<std::size_t>(pressed) < control_count &&
                    host->screen == UiScreen::game_world &&
                    contains_point(combat_control_rect(layout->sidebar.x, compact,
                        static_cast<std::size_t>(pressed)), host->mouse)) {
                    auto preferences = tibia::client::read_communication_preferences(
                        host->communication_config_path);
                    if (pressed < 3) {
                        preferences.attack_mode = pressed + 1;
                        host->combat_attack_mode = pressed + 1;
                    } else if (pressed == 3 || pressed == 4) {
                        preferences.chase_mode = pressed - 3;
                        host->combat_chase_mode = pressed - 3;
                    } else {
                        preferences.secure_mode = !preferences.secure_mode;
                        host->combat_secure_mode = preferences.secure_mode;
                    }
                    tibia::client::write_communication_preferences(
                        host->communication_config_path, preferences);
                    InvalidateRect(window, nullptr, FALSE);
                }
                host->pressed_combat_control = -1;
            }
            if (message == WM_LBUTTONUP && host->pressed_combat_action >= 0) {
                host->mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const int pressed = host->pressed_combat_action;
                RECT client{};
                GetClientRect(window, &client);
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height, tibia::client::map_window_minimum_height,
                    host->no_stretching});
                const bool compact = host->inventory_minimized;
                const auto action_count = compact
                    ? tibia::client::compact_combat_actions.size() +
                        tibia::client::compact_combat_utility_icons.size()
                    : tibia::client::expanded_combat_actions.size();
                if (!modal_open && layout && static_cast<std::size_t>(pressed) < action_count &&
                    host->screen == UiScreen::game_world &&
                    contains_point(combat_action_rect(layout->sidebar.x, compact,
                        static_cast<std::size_t>(pressed)), host->mouse)) {
                    const int options_action = compact ? 1 : 2;
                    if (pressed == 0 && host->send_game_command) {
                        try {
                            if (host->stop_action_reset_pending)
                                host->stop_action_reset_pending->store(true);
                            host->send_game_command(0xbe);
                        } catch (const std::exception& error) {
                            host->notice_text = error_to_wide(error.what());
                        }
                    } else if (!compact && pressed == 1) {
                        host->quest_log_open = true;
                        host->quest_log_dialog_offset = {};
                        if (!host->quest_request_sent && host->send_game_command) {
                            try {
                                host->send_game_command(0xf0);
                                host->quest_request_sent = true;
                            } catch (const std::exception& error) {
                                host->notice_text = error_to_wide(error.what());
                            }
                        }
                    } else if (pressed == options_action) {
                        host->options_open = true;
                        host->options_from_game = true;
                        host->options_dialog_offset = {};
                    } else if (pressed == (compact ? 2 : 3)) {
                        host->help_open = true;
                        host->help_dialog_offset = {};
                    }
                }
                host->pressed_combat_action = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_LBUTTONUP && !modal_open &&
                host->screen == UiScreen::game_world &&
                !host->inventory_minimized &&
                host->interaction_mode == tibia::client::hotkey_object_selection_mode &&
                host->pending_hotkey_object_selection) {
                RECT client{};
                GetClientRect(window, &client);
                const auto snapshot = host->game_world_mailbox->read();
                const auto layout = tibia::client::resolve_game_window_layout({
                    client.right - client.left, client.bottom - client.top,
                    host->console_height, tibia::client::map_panel_height, tibia::client::map_window_minimum_height,
                    host->no_stretching});
                if (snapshot && layout) {
                    const auto slot = tibia::client::inventory_slot_at(
                        layout->sidebar.x + tibia::client::inventory_panel_offset_x,
                        tibia::client::inventory_panel_offset_y,
                        GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                    const POINT click{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                    const RECT map_rect{layout->map.x, layout->map.y,
                        layout->map.x + layout->map.width,
                        layout->map.y + layout->map.height};
                    if (slot) {
                        const auto item = std::find_if(snapshot->inventory.begin(),
                            snapshot->inventory.end(), [slot](const auto& entry) {
                                return entry.slot == *slot;
                            });
                        if (item != snapshot->inventory.end() && item->item.type != 0 &&
                            *host->pending_hotkey_object_selection <
                                host->hotkey_options_values.size()) {
                            auto& preference = host->hotkey_options_values[
                                *host->pending_hotkey_object_selection];
                            preference.data[0] = item->item.type;
                            preference.data[1] = item->item.has_extra ? item->item.extra : 0;
                            host->interaction_mode = 0;
                            host->selected_hotkey = *host->pending_hotkey_object_selection;
                            host->hotkey_text_focused = false;
                            select_hotkey(*host, host->selected_hotkey);
                            host->pending_hotkey_object_selection.reset();
                            host->hotkey_options_open = true;
                            host->hotkey_options_dialog_offset = {};
                            host->pressed_hotkey_options_control = -1;
                            SetFocus(window);
                            InvalidateRect(window, nullptr, FALSE);
                        }
                    } else if (contains_point(map_rect, click)) {
                        host->communication_session_mailbox->enqueue_status_message(
                            "Please select an object in your inventory.");
                    }
                }
            }
            host->mouse_buttons &= ~(message == WM_LBUTTONUP ? 1U : message == WM_RBUTTONUP ? 2U : 4U);
            if (message == WM_LBUTTONUP) {
                host->console_transcript_selecting = false;
                host->pressed_skills_scrollbar_part = -1;
                host->skills_scrollbar_dragging = false;
                host->pressed_battle_scrollbar_part = -1;
                host->battle_scrollbar_dragging = false;
            }
            if (!host->mouse_buttons) {
                finish_options_list_scrollbar_interaction(*host);
                host->pressed_console_transcript_scrollbar_part = -1;
                host->console_transcript_scrollbar_dragging = false;
                ReleaseCapture();
            }
        }
        break;
    case WM_MOUSEWHEEL:
        if (host) {
            host->wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
            const int accumulated_wheel = host->wheel_remainder + host->wheel_delta;
            const int wheel_steps = accumulated_wheel / WHEEL_DELTA;
            host->wheel_remainder = accumulated_wheel % WHEEL_DELTA;
            if (host->screen == UiScreen::game_world && host->wheel_delta != 0) {
                POINT mouse{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ScreenToClient(window, &mouse);
                const auto& transcript = host->console_transcript_clip;
                const auto& scrollbar = host->console_transcript_scrollbar_viewport;
                if (host->console_transcript_scrollbar_geometry_valid &&
                    (contains_point(transcript, mouse) ||
                        contains_point(scrollbar, mouse))) {
                    if (wheel_steps != 0) {
                        const int maximum = std::max(0,
                            host->console_transcript_item_count -
                                host->console_transcript_visible_rows);
                        host->console_transcript_scroll_from_bottom = std::clamp(
                            host->console_transcript_scroll_from_bottom + wheel_steps,
                            0, maximum);
                        InvalidateRect(window, nullptr, FALSE);
                    }
                    return 0;
                }
                if (const auto plan = battle_panel_window_plan(*host, window)) {
                    const auto& viewport = plan->panel.viewport;
                    const RECT viewport_rect{viewport.x, viewport.y,
                        viewport.x + viewport.width,
                        viewport.y + viewport.height};
                    if (contains_point(viewport_rect, mouse)) {
                        host->battle_scroll_value = std::clamp(
                            plan->panel.scroll_offset - wheel_steps *
                                tibia::client::battle_panel_scroll_step,
                            0, plan->panel.scroll_range);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
                if (const auto plan = vip_panel_window_plan(*host, window)) {
                    const auto& viewport = plan->viewport;
                    const RECT viewport_rect{viewport.x, viewport.y,
                        viewport.x + viewport.width,
                        viewport.y + viewport.height};
                    if (contains_point(viewport_rect, mouse)) {
                        host->vip_scroll_value =
                            tibia::client::skills_panel_scroll_by_lines(
                                plan->scroll, -wheel_steps);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
                if (const auto plan = skills_panel_window_plan(*host, window)) {
                    const auto& panel = plan->skills.panel;
                    const RECT panel_rect{panel.x, panel.y,
                        panel.x + panel.width, panel.y + panel.height};
                    if (contains_point(panel_rect, mouse)) {
                        host->skills_scroll_value =
                            tibia::client::skills_panel_scroll_by_lines(
                                plan->skills.scroll_range, -wheel_steps);
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (host->hint_content_open && wheel_steps != 0) {
                POINT mouse{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ScreenToClient(window, &mouse);
                const auto view = hint_content_view_rect(window,
                    host->hint_content_dialog_offset);
                if (contains_point(view, mouse)) {
                    const int visible = hint_content_visible_lines(*host, view);
                    const int maximum = std::max(0,
                        static_cast<int>(host->hint_content_lines.size()) - visible);
                    host->hint_content_scroll = std::clamp(host->hint_content_scroll -
                        wheel_steps, 0, maximum);
                    InvalidateRect(window, nullptr, FALSE);
                }
            } else if (host->hints_open && wheel_steps != 0) {
                POINT mouse{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ScreenToClient(window, &mouse);
                const auto list = hints_list_rect(window, host->hints_dialog_offset);
                if (contains_point(list, mouse)) {
                    const int line_height = ui_units(window,
                        std::max(1, host->ui_font_heights[2]));
                    const int visible = std::max(1,
                        static_cast<int>(list.bottom - list.top) / line_height);
                    const int maximum = std::max(0,
                        static_cast<int>(host->hints.size()) - visible);
                    host->hints_scroll = std::clamp(host->hints_scroll -
                        wheel_steps, 0, maximum);
                    InvalidateRect(window, nullptr, FALSE);
                }
            } else if (host->hotkey_options_open && wheel_steps != 0) {
                const auto maximum = tibia::client::hotkey_count -
                    tibia::client::hotkey_list_visible_rows;
                const int scroll = static_cast<int>(host->hotkey_list_scroll) - wheel_steps;
                host->hotkey_list_scroll = static_cast<std::size_t>(std::clamp(
                    scroll, 0, static_cast<int>(maximum)));
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->graphics_options_open && wheel_steps != 0) {
                const int maximum = std::max(0, static_cast<int>(
                    host->graphics_display_modes.size()) -
                    tibia::client::graphics_options_visible_rows);
                host->graphics_mode_scroll = std::clamp(host->graphics_mode_scroll -
                    wheel_steps, 0, maximum);
                InvalidateRect(window, nullptr, FALSE);
            } else if (host->quest_line_open && wheel_steps != 0) {
                POINT mouse{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ScreenToClient(window, &mouse);
                const auto flags = quest_line_flags_rect(window,
                    host->quest_line_dialog_offset);
                if (contains_point(flags, mouse)) {
                    const int height = ui_units(window, 118);
                    const int row_height = ui_units(window,
                        tibia::client::quest_log_row_height);
                    const int margin = ui_units(window,
                        tibia::client::quest_log_content_margin);
                    const int count = static_cast<int>(quest_scroll_entry_count(*host, true));
                    const int maximum = tibia::client::quest_log_max_scroll(
                        static_cast<std::size_t>(count), height, row_height, margin);
                    host->quest_flag_scroll = std::clamp(host->quest_flag_scroll -
                        wheel_steps, 0, maximum);
                    InvalidateRect(window, nullptr, FALSE);
                }
            } else if (host->quest_log_open && wheel_steps != 0) {
                POINT mouse{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ScreenToClient(window, &mouse);
                if (contains_point(quest_log_list_rect(window,
                    host->quest_log_dialog_offset), mouse)) {
                    const auto snapshot = host->game_world_mailbox->read();
                    const int maximum = snapshot ? tibia::client::quest_log_max_scroll(
                        snapshot->quest_log_entries.size(), ui_units(window, 268),
                        ui_units(window, tibia::client::quest_log_row_height),
                        ui_units(window, tibia::client::quest_log_content_margin)) : 0;
                    host->quest_log_scroll = std::clamp(host->quest_log_scroll -
                        wheel_steps, 0, maximum);
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
        }
        break;
    case WM_DESTROY:
        if (host && host->fullscreen_window) {
            try { set_window_fullscreen(*host, false, nullptr); }
            catch (...) { ChangeDisplaySettingsW(nullptr, 0); }
        }
        if (host && host->login_thread.joinable()) {
            host->login_thread.request_stop();
            if (host->login_connection) host->login_connection->close();
        }
        if (host) {
            if (host->ui_font) DeleteObject(host->ui_font);
            if (host->ui_title_font) DeleteObject(host->ui_title_font);
            if (host->ui_menu_font) DeleteObject(host->ui_menu_font);
            if (host->ui_edit_brush) DeleteObject(host->ui_edit_brush);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return window_proc_impl(window, message, wparam, lparam);
}

int run(HINSTANCE instance, const std::filesystem::path& pic_path) {
    const auto bytes = read_file(pic_path);
    const auto pic = tibia::resources::parse_tibia_pic(bytes);
    if (pic.pictures.empty()) throw std::runtime_error("Tibia.pic has no pictures");
    const auto root = pic_path.parent_path();
    const auto dat_bytes = read_file(root / L"Tibia.dat");
    const auto spr_bytes = read_file(root / L"Tibia.spr");
    const auto dat = tibia::resources::parse_tibia_dat(dat_bytes);
    const auto spr = tibia::resources::parse_tibia_spr(spr_bytes);
    auto host_storage = std::make_unique<ClientHost>();
    auto& host = *host_storage;
    host.dat = &dat;
    host.tibia_spr = &spr;
    host.tibia_pic = &pic;
    host.tibia_spr_bytes = &spr_bytes;
    host.tibia_pic_bytes = &bytes;
    host.map_tile_cache = std::make_unique<tibia::map::MapWindowTileCache>();
    host.map_surface_renderer =
        std::make_unique<tibia::client::MapWindowTileSurfaceRenderer>();
    host.communication_config_path = root / L"Tibia.cfg";
    const auto client_preferences = tibia::client::read_communication_preferences(
        host.communication_config_path);
    host.game_world_mailbox->set_automap_zoom(client_preferences.automap_zoom);
    host.restored_content_windows = tibia::client::restore_content_windows(
        client_preferences.content_windows);
    const auto has_content_window = [&host](
        tibia::client::ContentWindowType type) {
        return std::any_of(host.restored_content_windows.begin(),
            host.restored_content_windows.end(), [type](const auto& window) {
                return window.type == type;
            });
    };
    host.skills_window_open = has_content_window(
        tibia::client::ContentWindowType::skills);
    host.battle_window_open = has_content_window(
        tibia::client::ContentWindowType::battle);
    host.vip_window_open = has_content_window(
        tibia::client::ContentWindowType::vip);
    host.skills_bar_visibility = client_preferences.skills_bar_visibility;
    host.console_height = client_preferences.console_height;
    host.combat_attack_mode = client_preferences.attack_mode;
    host.combat_chase_mode = client_preferences.chase_mode;
    host.combat_secure_mode = client_preferences.secure_mode;
    host.test_server_login = client_preferences.test_server_login;
    host.ambient_light = std::clamp(client_preferences.basic_brightness, 0, 128);
    host.show_light_effects = client_preferences.light_effects;
    host.show_textual_effects = client_preferences.textual_effects;
    host.creature_info_mode = client_preferences.creature_info;
    host.no_stretching = client_preferences.no_stretching;
    host.windows_mouse_pointer = client_preferences.windows_mouse_pointer;
    host.framerate_limit = client_preferences.confine_framerate <
        tibia::client::advanced_graphics_slider_minimum
        ? tibia::client::advanced_graphics_slider_maximum
        : std::clamp(client_preferences.confine_framerate,
            tibia::client::advanced_graphics_slider_minimum,
            tibia::client::advanced_graphics_slider_maximum);
    if (dat.items.objects.empty() || spr.sprite_offsets.empty())
        throw std::runtime_error("Tibia.dat or Tibia.spr contains no indexed client resources");
    for (std::size_t i = 0; i < dat.items.objects.size(); ++i) {
        const auto& object = dat.items.objects[i];
        const bool has_network_extra = std::any_of(object.attributes.begin(), object.attributes.end(),
            [](const auto& attribute) {
                return attribute.id == 0x0B || attribute.id == 0x0C ||
                    attribute.id == 0x05 || attribute.id == 0x08;
            });
        if (has_network_extra) {
            host.map_item_ids_with_extra_byte.push_back(
                static_cast<std::uint16_t>(dat.items.first_id + i));
        }
    }
    const tibia::resources::TibiaDatCategory* categories[] = {
        &dat.items, &dat.outfits, &dat.effects, &dat.missiles
    };
    for (const auto* category : categories)
        for (const auto& object : category->objects)
            for (const auto sprite_id : object.sprite_ids)
                if (sprite_id != 0 && sprite_id > spr.sprite_offsets.size())
                    throw std::runtime_error("Tibia.dat references a sprite outside Tibia.spr index");
    const auto& first = pic.pictures.front();
    if (client_preferences.engine >= 0 && client_preferences.engine <= 2)
        host.backend = static_cast<tibia::render::GraphicsBackend>(client_preferences.engine);
    else
        host.backend = tibia::render::GraphicsBackend::direct3d9;
    host.resource_summary = L"Validated: items " + std::to_wstring(dat.items.objects.size()) +
        L", outfits " + std::to_wstring(dat.outfits.objects.size()) +
        L", sprites " + std::to_wstring(spr.sprite_offsets.size()) +
        L", pictures " + std::to_wstring(pic.pictures.size());
    INITCOMMONCONTROLSEX common_controls{sizeof(common_controls), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common_controls))
        throw std::runtime_error("InitCommonControlsEx failed");
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"TibiaClient";
    if (!RegisterClassExW(&wc)) throw std::runtime_error("RegisterClassExW failed");
    // Original FUN_004bdce0 uses a 600x450 client area when no valid saved
    // window rectangle exists, then centers the outer window on the screen.
    constexpr int client_width = 600;
    constexpr int client_height = 450;
    RECT rect{0, 0, client_width, client_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    const int window_width = rect.right - rect.left;
    const int window_height = rect.bottom - rect.top;
    const int window_x = (GetSystemMetrics(SM_CXSCREEN) - window_width) / 2;
    const int window_y = (GetSystemMetrics(SM_CYSCREEN) - window_height) / 2;
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"Tibia",
        WS_OVERLAPPEDWINDOW, window_x, window_y, window_width, window_height,
        nullptr, nullptr, instance, &host);
    if (!window) throw std::runtime_error("CreateWindowExW failed");

    host.renderer = std::make_unique<tibia::render::GraphicsRenderer>();
    RECT client{};
    GetClientRect(window, &client);
    host.renderer->initialize(host.backend, window,
        static_cast<std::uint32_t>(client.right - client.left),
        static_cast<std::uint32_t>(client.bottom - client.top));
    host.renderer_initialized = true;
    upload_solid_ui_texture(host, login_field_background_texture, 0x36);
    upload_solid_ui_texture(host, login_caret_texture, 0xff);
    upload_solid_ui_texture(host, console_input_focused_texture, 0x47);
    upload_solid_ui_texture(host, console_input_selection_texture, 0x80);
    upload_solid_ui_texture(host, ui_separator_shadow_texture, 0x28);
    upload_solid_ui_texture(host, ui_separator_highlight_texture, 0x77);
    upload_solid_ui_texture(host, graphics_list_background_texture, 0x40);
    upload_solid_ui_texture(host, graphics_list_selection_texture, 0x70);
    upload_solid_ui_color(host, skills_bar_track_texture, 0, 0, 0);
    upload_solid_ui_color(host, skills_bar_red_texture, 192, 0, 0);
    upload_solid_ui_color(host, skills_bar_green_texture, 0, 192, 0);
    upload_solid_ui_color(host, skills_bar_yellow_texture, 192, 192, 0);
    upload_solid_ui_color(host, skills_bar_orange_texture, 192, 96, 0);
    upload_solid_ui_color(host, skills_separator_texture, 176, 176, 176);
    upload_solid_ui_color(host, skills_menu_selection_texture, 128, 128, 128);
    upload_solid_ui_color(host, battle_hover_frame_texture, 248, 248, 248);
    upload_solid_ui_color(host, battle_attack_frame_texture, 224, 64, 64);
    upload_solid_ui_color(host, battle_follow_frame_texture, 64, 224, 64);
    upload_solid_ui_color(host, battle_attack_hover_frame_texture, 248, 164, 164);
    upload_solid_ui_color(host, battle_follow_hover_frame_texture, 180, 248, 180);
    upload_solid_ui_color(host, automap_empty_viewport_texture, 0, 0, 0);
    upload_solid_ui_color(host, battle_health_track_texture, 0, 0, 0);
    upload_solid_ui_color(host, battle_health_color_texture_base, 96, 0, 0);
    upload_solid_ui_color(host, battle_health_color_texture_base + 1, 192, 0, 0);
    upload_solid_ui_color(host, battle_health_color_texture_base + 2, 192, 48, 48);
    upload_solid_ui_color(host, battle_health_color_texture_base + 3, 192, 192, 0);
    upload_solid_ui_color(host, battle_health_color_texture_base + 4, 96, 192, 96);
    upload_solid_ui_color(host, battle_health_color_texture_base + 5, 0, 192, 0);
    upload_ui_skins(host, bytes, pic);
    tibia::client::upload_game_mouse_cursor(*host.renderer, bytes, pic);
    upload_login_field_bevel(host, bytes, pic);
    upload_bitmap_fonts(host, bytes, pic);
    for (std::uint16_t y = 0; y < first.tiles_y; ++y) {
        for (std::uint16_t x = 0; x < first.tiles_x; ++x) {
            auto image = tibia::resources::decode_tibia_pic_tile(bytes, pic, 1, x, y);
            const auto id = static_cast<std::uint32_t>(y) * first.tiles_x + x + 1U;
            host.renderer->upload(id, image);
            host.tiles.push_back({id, static_cast<int>(x) * 32, static_cast<int>(y) * 32});
        }
    }
    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) &&
            msg.wParam == VK_ESCAPE && (msg.hwnd == window || IsChild(window, msg.hwnd))) {
            SendMessageW(window, escape_ui_message, 0, 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (host.login_thread.joinable()) {
        host.login_thread.request_stop();
        if (host.login_connection) host.login_connection->close();
        host.login_thread.join();
    }
    host.renderer.reset();
    UnregisterClassW(wc.lpszClassName, instance);
    return static_cast<int>(msg.wParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        std::wstring executable(32768, L'\0');
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0 || length >= executable.size()) throw std::runtime_error("cannot resolve executable path");
        executable.resize(length);
        const auto exe_dir = std::filesystem::path(executable).parent_path();
        auto pic = exe_dir / L"Tibia.pic";
        if (!std::filesystem::exists(pic)) pic = std::filesystem::current_path() / L"Tibia.pic";
        return run(instance, pic);
    } catch (const std::exception& error) {
        const auto message = std::wstring(error.what(), error.what() + std::char_traits<char>::length(error.what()));
        MessageBoxW(nullptr, message.c_str(), L"Tibia reconstruction", MB_OK | MB_ICONERROR);
        return 1;
    }
}
