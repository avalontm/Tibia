#pragma once

#include "client/content_window_stack_layout.hpp"

namespace tibia::client {

struct SkillsPanelHeaderLayout {
    ContentWindowRect title;
    ContentWindowRect icon;
    ContentWindowRect minimize_button;
    ContentWindowRect close_button;
};

[[nodiscard]] SkillsPanelHeaderLayout layout_skills_panel_header(
    const ContentWindowRect& window) noexcept;

}
