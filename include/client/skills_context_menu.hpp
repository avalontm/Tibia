#pragma once

#include "client/content_window_stack_layout.hpp"
#include "client/skills_panel_model.hpp"

#include <cstdint>
#include <array>
#include <span>
#include <string>
#include <vector>

namespace tibia::client {

struct SkillsContextMenuEntry {
    std::uint16_t action{};
    std::wstring label;
    ContentWindowRect rect;
    bool separator{};
};

struct SkillsContextMenuPlan {
    ContentWindowRect window;
    std::vector<SkillsContextMenuEntry> entries;
};

inline constexpr std::uint16_t skills_context_menu_first_action = 0x2904;
inline constexpr std::uint16_t skills_context_menu_last_action = 0x290f;
inline constexpr int skills_context_menu_border = 4;
inline constexpr int skills_context_menu_text_padding = 2;
inline constexpr int skills_context_menu_action_width_padding = 44;
inline constexpr int skills_context_menu_separator_width = 50;
inline constexpr int skills_context_menu_separator_height = 8;

[[nodiscard]] std::array<std::wstring, 12> skills_context_menu_labels(
    const SkillsPanelBarVisibility& visibility);

[[nodiscard]] SkillsContextMenuPlan make_skills_context_menu_plan(
    const SkillsPanelBarVisibility& visibility,
    std::span<const int> text_widths,
    int font_height,
    int anchor_x,
    int anchor_y,
    int client_width,
    int client_height);

[[nodiscard]] bool apply_skills_context_menu_action(
    SkillsPanelBarVisibility& visibility,
    std::uint16_t action) noexcept;

}
