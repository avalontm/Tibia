#include "client/skills_panel_header_layout.hpp"

#include <algorithm>

namespace tibia::client {

SkillsPanelHeaderLayout layout_skills_panel_header(
    const ContentWindowRect& window) noexcept {
    constexpr int button_size = 12;
    return {
        {window.x + 20, window.y + 4, std::max(0, window.width - 61), 10},
        {window.x + 4, window.y + 2, button_size, button_size},
        {window.x + window.width - 28, window.y + 2, button_size, button_size},
        {window.x + window.width - 15, window.y + 2, button_size, button_size}
    };
}

}
