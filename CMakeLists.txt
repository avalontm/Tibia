cmake_minimum_required(VERSION 3.20)
project(tibia_reconstruction LANGUAGES CXX)

add_library(tibia_core STATIC
    src/objects/entry_collection.cpp
    src/objects/entry_attribute_flags.cpp
    src/objects/entry_pattern_coordinates.cpp
    src/objects/entry_pattern_mask.cpp
    src/objects/entry_sprite_table.cpp
    src/objects/object_property_table.cpp
    src/crypto/msvc_rand.cpp
    src/crypto/xtea.cpp
    src/client/talk_system.cpp
    src/client/talkbox_layout.cpp
    src/client/talkbox_renderer.cpp
    src/client/battle_panel_model.cpp
    src/client/battle_panel_layout.cpp
    src/client/battle_panel_window_layout.cpp
    src/client/vip_panel_window_layout.cpp
    src/client/vip_context_menu.cpp
    src/client/battle_panel_renderer.cpp
    src/client/battle_panel_interaction.cpp
    src/client/console_context_menu.cpp
    src/client/console_panel_layout.cpp
    src/client/console_panel_renderer.cpp
    src/client/console_chat_input.cpp
    src/client/message_dialog_layout.cpp
    src/client/new_channel_dialog_layout.cpp
    src/client/game_mouse_cursor.cpp
    src/client/inventory_layout.cpp
    src/client/player_status_bars_layout.cpp
    src/client/inventory_panel_renderer.cpp
    src/client/container_panel_layout.cpp
    src/client/container_panel_renderer.cpp
    src/client/inventory_item_visual.cpp
    src/client/skills_panel_model.cpp
    src/client/skills_panel_layout.cpp
    src/client/skills_panel_scroll.cpp
    src/client/skills_panel_render_plan.cpp
    src/client/skills_panel_window_layout.cpp
    src/client/skills_panel_header_layout.cpp
    src/client/skills_context_menu.cpp
    src/client/content_window_state.cpp
    src/client/content_window_stack_layout.cpp
    src/client/content_window_frame_layout.cpp
    src/client/game_world_snapshot.cpp
    src/client/quest_log_view.cpp
    src/client/quest_log_layout.cpp
    src/client/ui_scrollbar_layout.cpp
    src/client/report_violation.cpp
    src/client/report_violation_buffer.cpp
    src/client/report_violation_dialog.cpp
    src/client/rule_violation_request_form.cpp
    src/client/creature_outfit_appearance.cpp
    src/client/creature_render_plan.cpp
    src/client/creature_render_batch.cpp
    src/client/creature_render_canvas.cpp
    src/client/creature_render_frame.cpp
    src/client/map_window_layout.cpp
    src/client/map_window_frame_layout.cpp
    src/client/game_screen_control_layout.cpp
    src/client/automap_panel_layout.cpp
    src/client/map_window_camera.cpp
    src/client/map_window_item_frame.cpp
    src/client/map_window_light_sources.cpp
    src/client/map_window_creature_renderer.cpp
    src/client/map_window_effect_renderer.cpp
    src/client/map_window_lighting_renderer.cpp
    src/client/map_window_direct_sprite_renderer.cpp
    src/client/map_window_tile_surface_renderer.cpp
    src/client/message_line_list.cpp
    src/client/communication_router.cpp
    src/client/communication_session.cpp
    src/client/client_communication_preferences.cpp
    src/client/hints_catalog.cpp
    src/client/hint_markup.cpp
    src/client/hotkey_preferences.cpp
    src/client/hotkey_options_page.cpp
    src/network/encrypted_frame.cpp
    src/network/rule_violation_request.cpp
    src/network/encrypted_frame_stream.cpp
    src/network/bounded_byte_writer.cpp
    src/network/connection_error_message.cpp
    src/network/delta_patch.cpp
    src/network/login_request.cpp
    src/network/login_response.cpp
    src/network/login_session.cpp
    src/network/speech_request.cpp
    src/network/channel_request.cpp
    src/network/console_filter_request.cpp
    src/network/message_dispatcher.cpp
    src/navigation/navigation.cpp
    src/map/cache.cpp
    src/map/automap_store.cpp
    src/map/collision.cpp
    src/map/floor_window.cpp
    src/map/item_render_traversal.cpp
    src/map/tile_object_order.cpp
    src/map/light_field.cpp
    src/map/render_queue.cpp
    src/map/render_cell_filter.cpp
    src/map/window_tile_cache.cpp
    src/map/model.cpp
    src/network/patch_transfer.cpp
    src/network/patch_file_output.cpp
    src/network/server_address.cpp
    src/network/server_catalog.cpp
    src/network/status_line_state.cpp
    src/network/patch_finalizer.cpp
    src/network/patch_process.cpp
    src/network/packet_stream.cpp
    src/network/tcp_frame_buffer.cpp
    src/network/tcp_connection.cpp
    src/network/rsa_login_block.cpp
    src/network/rsa_public.cpp
    src/resources/tibia_dat.cpp
    src/resources/tibia_spr.cpp
    src/resources/tibia_pic.cpp
    src/render/software_surface.cpp
    src/render/graphics_renderer.cpp
    src/render/object_compositor.cpp
    src/render/object_sprite_layout.cpp
    src/render/map_item_layout.cpp
    src/render/map_item_router.cpp
    src/render/object_sprite_router.cpp
    src/render/creature_render_order.cpp
    src/render/creature_projection.cpp
    src/render/outfit_compositor.cpp
    src/render/pic_compositor.cpp
    src/render/animation_compositor.cpp
)

target_include_directories(tibia_core PUBLIC include)
target_compile_features(tibia_core PUBLIC cxx_std_20)

add_executable(tibia_asset_tool tools/asset_tool.cpp)
target_link_libraries(tibia_asset_tool PRIVATE tibia_core)

if(WIN32)
    target_sources(tibia_core PRIVATE src/render/d3d9_renderer.cpp src/render/opengl_renderer.cpp src/render/directdraw_renderer.cpp)
target_link_libraries(tibia_core PRIVATE bcrypt ws2_32 opengl32 shell32)

    add_executable(tibia_client WIN32
        src/client/account_management.cpp
        src/client/win32_client.cpp
    )
    target_link_libraries(tibia_client PRIVATE tibia_core user32 gdi32 comctl32 shell32 winmm)
    add_custom_command(TARGET tibia_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/Tibia.dat
            ${CMAKE_CURRENT_SOURCE_DIR}/Tibia.pic
            ${CMAKE_CURRENT_SOURCE_DIR}/Tibia.spr
            ${CMAKE_CURRENT_SOURCE_DIR}/Tibia.cfg
            $<TARGET_FILE_DIR:tibia_client>
        VERBATIM
    )
endif()

