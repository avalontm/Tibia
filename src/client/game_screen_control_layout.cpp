#include "client/game_screen_control_layout.hpp"

#include "client/ui_skins_792.hpp"

namespace tibia::client {

std::array<GameScreenControlButton, 4>
layout_game_screen_control_buttons(std::int32_t sidebar_x,
    std::int32_t row_y) noexcept {
    const auto make_button = [sidebar_x, row_y](GameScreenControl control,
        std::int32_t offset_x, std::uint16_t normal_skin,
        std::uint16_t pressed_skin) {
        const auto& skin = ui_skins[normal_skin];
        return GameScreenControlButton{control,
            {sidebar_x + offset_x, row_y, skin.width, skin.height},
            normal_skin, pressed_skin};
    };
    return {{
        make_button(GameScreenControl::skills, 8, 14, 15),
        make_button(GameScreenControl::battle, 45, 14, 15),
        make_button(GameScreenControl::vip, 82, 14, 15),
        make_button(GameScreenControl::logout, 124, 12, 13)
    }};
}

}
