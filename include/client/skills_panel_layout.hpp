#pragma once

#include "client/skills_panel_model.hpp"
#include "client/skills_panel_scroll.hpp"

#include <array>
#include <cstdint>

namespace tibia::client {

struct SkillsPanelRect {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct SkillsPanelMetrics {
    int font_height{};
    SkillsPanelScrollRange scroll;
};

struct SkillsPanelRgb {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

struct SkillsPanelRowLayout {
    SkillsPanelRowKind kind{SkillsPanelRowKind::value};
    SkillsPanelRect clip;
    int label_x{};
    int label_y{};
    int value_right{};
    int value_y{};
    SkillsPanelRect progress_track;
    SkillsPanelRect progress_fill;
    int separator_y{};
    SkillsPanelRgb text_color{176, 176, 176};
    SkillsPanelRgb progress_color{};
    SkillsPanelRgb separator_color{176, 176, 176};
};

inline constexpr int skills_panel_font_index = 3;
inline constexpr int skills_panel_padding_x = 10;
inline constexpr int skills_panel_padding_y = 10;
inline constexpr int skills_panel_scrollbar_width = 12;

[[nodiscard]] std::array<SkillsPanelRowLayout, 16> layout_skills_panel_rows(
    const std::array<SkillsPanelRow, 16>& rows,
    const SkillsPanelRect& panel,
    const SkillsPanelMetrics& metrics);

}
