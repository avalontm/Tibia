#pragma once

#include "client/game_world_snapshot.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace tibia::client {

enum class SkillsPanelRowKind : std::uint8_t {
    value,
    separator
};

struct SkillsPanelRow {
    SkillsPanelRowKind kind{SkillsPanelRowKind::value};
    std::wstring label;
    std::wstring value;
    std::optional<std::uint32_t> progress;
};

struct SkillsPanelBarVisibility {
    bool level{true};
    bool stamina{true};
    bool magic_level{true};
    std::array<bool, 7> skills{true, true, true, true, true, true, true};
};

[[nodiscard]] std::array<SkillsPanelRow, 16> make_skills_panel_rows(
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility);

}
