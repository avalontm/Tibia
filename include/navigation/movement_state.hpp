#pragma once

#include <cstdint>

namespace tibia::navigation {

struct LocalPlayerMovementState {
    std::uint32_t at_0x00605A14{}, at_0x00605A10{}, at_0x00605A0C{};
    std::uint32_t at_0x006059DC{}, at_0x006059D8{}, at_0x006059D4{};
    std::uint32_t at_0x00605A18{};
    std::int32_t at_0x00605A20{}, at_0x00605A1C{};
    std::uint8_t at_0x00605A09{}, at_0x00605A08{};
    std::uint32_t at_0x0060599C{}, at_0x00605998{};
    std::uint32_t at_0x00605A24{};
};

}
