#pragma once

#include <array>
#include <string_view>

namespace tibia::client {

struct CombatIconLayout {
    int normal_skin{};
    int pressed_skin{};
    int x{};
    int y{};
};

struct CombatTextLayout {
    std::wstring_view label;
    int x{};
    int y{};
    int normal_skin{12};
    int pressed_skin{13};
};

inline constexpr int expanded_combat_controls_x = 124;
inline constexpr int expanded_combat_controls_y = 164;
inline constexpr int combat_icon_size = 20;

inline constexpr std::array<CombatIconLayout, 6> expanded_combat_icons{{
    {103, 104, 0, 0},
    {105, 106, 0, 20},
    {107, 108, 0, 40},
    {109, 110, 23, 0},
    {111, 112, 23, 20},
    {113, 114, 23, 40}
}};

inline constexpr std::array<CombatTextLayout, 4> expanded_combat_actions{{
    {L"Stop", 0, 62},
    {L"Quests", 0, 86},
    {L"Options", 0, 108},
    {L"Help", 0, 130}
}};

inline constexpr int compact_combat_controls_x = 56;
inline constexpr int compact_combat_controls_y = 149;

inline constexpr std::array<CombatIconLayout, 6> compact_combat_icons{{
    {103, 104, 0, 0},
    {105, 106, 20, 0},
    {107, 108, 40, 0},
    {109, 110, 0, 22},
    {111, 112, 20, 22},
    {113, 114, 40, 22}
}};

inline constexpr std::array<CombatTextLayout, 1> compact_combat_actions{{
    {L"Stop", 68, 0}
}};

struct CombatUtilityIconLayout {
    int normal_skin{};
    int pressed_skin{};
    int x{};
    int y{};
};

inline constexpr std::array<CombatUtilityIconLayout, 2> compact_combat_utility_icons{{
    {115, 116, 68, 22},
    {117, 118, 90, 22}
}};

}
