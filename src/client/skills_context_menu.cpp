#include "client/skills_context_menu.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>

namespace tibia::client {
namespace {

std::array<std::wstring, 12> labels_for(
    const SkillsPanelBarVisibility& visibility) {
    constexpr std::array<std::wstring_view, 10> names{
        L"Level", L"Stamina", L"Magic Level", L"Fist Fighting",
        L"Club Fighting", L"Sword Fighting", L"Axe Fighting",
        L"Distance Fighting", L"Shielding", L"Fishing"};
    std::array<bool, 10> visible{
        visibility.level, visibility.stamina, visibility.magic_level,
        visibility.skills[0], visibility.skills[1], visibility.skills[2],
        visibility.skills[3], visibility.skills[4], visibility.skills[5],
        visibility.skills[6]};
    std::array<std::wstring, 12> labels;
    for (std::size_t index = 0; index < names.size(); ++index) {
        labels[index] = visible[index] ? L"Hide " : L"Show ";
        labels[index] += names[index];
    }
    labels[10] = L"Show all Skill Bars";
    labels[11] = L"Hide all Skill Bars";
    return labels;
}

}

std::array<std::wstring, 12> skills_context_menu_labels(
    const SkillsPanelBarVisibility& visibility) {
    return labels_for(visibility);
}

SkillsContextMenuPlan make_skills_context_menu_plan(
    const SkillsPanelBarVisibility& visibility,
    std::span<const int> text_widths,
    int font_height,
    int anchor_x,
    int anchor_y,
    int client_width,
    int client_height) {
    const auto labels = labels_for(visibility);
    const auto row_height = std::max(0, font_height) +
        skills_context_menu_text_padding * 2;
    auto width = skills_context_menu_separator_width;
    for (const auto text_width : text_widths)
        width = std::max(width, std::max(0, text_width) +
            skills_context_menu_action_width_padding);
    width += skills_context_menu_border * 2;

    auto height = skills_context_menu_border * 2 +
        row_height * static_cast<int>(labels.size()) +
        skills_context_menu_separator_height;
    int x = anchor_x;
    int y = anchor_y;
    if (x + width > client_width) x = client_width - width;
    if (y + height > client_height) y = client_height - height;
    x = std::max(0, x);
    y = std::max(0, y);

    SkillsContextMenuPlan plan;
    plan.window = {x, y, width, height};
    plan.entries.reserve(labels.size() + 1);
    auto row_y = y + skills_context_menu_border;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (index == 10) {
            plan.entries.push_back({0, {},
                {x + skills_context_menu_border, row_y,
                    width - skills_context_menu_border * 2,
                    skills_context_menu_separator_height}, true});
            row_y += skills_context_menu_separator_height;
        }
        plan.entries.push_back({static_cast<std::uint16_t>(
                skills_context_menu_first_action + index), labels[index],
            {x + skills_context_menu_border, row_y,
                width - skills_context_menu_border * 2, row_height}, false});
        row_y += row_height;
    }
    return plan;
}

bool apply_skills_context_menu_action(
    SkillsPanelBarVisibility& visibility,
    std::uint16_t action) noexcept {
    if (action >= 0x2904 && action <= 0x2906) {
        auto* flag = action == 0x2904 ? &visibility.level :
            action == 0x2905 ? &visibility.stamina : &visibility.magic_level;
        *flag = !*flag;
        return true;
    }
    if (action >= 0x2907 && action <= 0x290d) {
        auto& flag = visibility.skills[action - 0x2907];
        flag = !flag;
        return true;
    }
    if (action == 0x290e || action == 0x290f) {
        const auto visible = action == 0x290e;
        visibility.level = visible;
        visibility.stamina = visible;
        visibility.magic_level = visible;
        visibility.skills.fill(visible);
        return true;
    }
    return false;
}

}
