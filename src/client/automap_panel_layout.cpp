#include "client/automap_panel_layout.hpp"

namespace tibia::client {

AutomapPanelLayout layout_automap_panel(
    std::int32_t sidebar_x, std::int32_t panel_y) noexcept {
    const MapWindowLayout panel{sidebar_x, panel_y, 176, automap_panel_height};
    return {
        panel,
        {sidebar_x + 9, panel_y + 5, 106, 106},
        {sidebar_x + 124, panel_y + 5, 43, 43},
        {{
            {{sidebar_x + 124, panel_y + 71, 20, 20}, 66, 67},
            {{sidebar_x + 124, panel_y + 50, 20, 20}, 68, 69},
            {{sidebar_x + 147, panel_y + 50, 20, 20}, 70, 71},
            {{sidebar_x + 147, panel_y + 71, 20, 20}, 72, 73}
        }},
        {{sidebar_x + 124, panel_y + 92, 43, 20}, 12, 13}
    };
}

}
