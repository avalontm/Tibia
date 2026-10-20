#include "client/skills_panel_model.hpp"

#include <array>
#include <string_view>

namespace tibia::client {
namespace {

std::wstring format_grouped_number(std::uint32_t value) {
    const auto digits = std::to_string(value);
    std::wstring formatted;
    formatted.reserve(digits.size() + digits.size() / 3);
    for (std::size_t index = 0; index < digits.size(); ++index) {
        if (index != 0 && (digits.size() - index) % 3 == 0)
            formatted.push_back(L',');
        formatted.push_back(static_cast<wchar_t>(digits[index]));
    }
    return formatted;
}

std::wstring format_stamina(std::uint32_t minutes) {
    const auto hours = minutes / 60;
    const auto remaining_minutes = minutes % 60;
    auto formatted = std::to_wstring(hours);
    if (hours < 10) formatted.insert(formatted.begin(), L' ');
    formatted.push_back(L':');
    if (remaining_minutes < 10) formatted.push_back(L'0');
    formatted += std::to_wstring(remaining_minutes);
    return formatted;
}

}

std::array<SkillsPanelRow, 16> make_skills_panel_rows(
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility) {
    constexpr std::array<std::wstring_view, 7> skill_names{
        L"Fist Fighting", L"Club Fighting", L"Sword Fighting", L"Axe Fighting",
        L"Distance Fighting", L"Shielding", L"Fishing"};
    std::array<SkillsPanelRow, 16> rows{};
    rows[0] = {SkillsPanelRowKind::value, L"Experience",
        format_grouped_number(statistics.experience), std::nullopt};
    rows[1] = {SkillsPanelRowKind::value, L"Level",
        format_grouped_number(statistics.level), bar_visibility.level
            ? std::optional(statistics.level_progress) : std::nullopt};
    rows[2] = {SkillsPanelRowKind::value, L"Hit Points",
        format_grouped_number(statistics.hit_points), std::nullopt};
    rows[3] = {SkillsPanelRowKind::value, L"Mana",
        format_grouped_number(statistics.mana), std::nullopt};
    rows[4] = {SkillsPanelRowKind::value, L"Soul Points",
        format_grouped_number(statistics.soul_points), std::nullopt};
    rows[5] = {SkillsPanelRowKind::value, L"Capacity",
        format_grouped_number(statistics.capacity), std::nullopt};
    rows[6] = {SkillsPanelRowKind::value, L"Stamina",
        format_stamina(statistics.stamina_minutes), bar_visibility.stamina
            ? std::optional(statistics.stamina_progress) : std::nullopt};
    rows[7] = {SkillsPanelRowKind::value, L"Magic Level",
        format_grouped_number(statistics.magic_level), bar_visibility.magic_level
            ? std::optional(statistics.magic_level_progress) : std::nullopt};
    rows[8] = {SkillsPanelRowKind::separator, {}, {}, std::nullopt};
    for (std::size_t index = 0; index < skill_names.size(); ++index) {
        rows[index + 9] = {SkillsPanelRowKind::value, std::wstring(skill_names[index]),
            format_grouped_number(statistics.skills[index]), bar_visibility.skills[index]
                ? std::optional(statistics.skill_progress[index]) : std::nullopt};
    }
    return rows;
}

}
