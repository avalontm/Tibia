#pragma once

#include <cstdint>

namespace tibia::client {

enum class NativeInteractionEvent : std::uint8_t {
    use_with_target = 0x2f,
    trade_with_player = 0x30
};

enum class GameInteractionMode : std::uint8_t {
    none = 0,
    move_item = 6,
    use_with_target = 7,
    hotkey_object_selection = 8,
    trade_with_player = 9
};

[[nodiscard]] constexpr GameInteractionMode interaction_mode_for_event(
    NativeInteractionEvent event) noexcept {
    switch (event) {
    case NativeInteractionEvent::use_with_target:
        return GameInteractionMode::use_with_target;
    case NativeInteractionEvent::trade_with_player:
        return GameInteractionMode::trade_with_player;
    }
    return GameInteractionMode::none;
}

}
