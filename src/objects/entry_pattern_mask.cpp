#include "objects/entry_pattern_mask.hpp"

namespace tibia::objects {

std::uint32_t pattern_height_mask(std::uint32_t height) noexcept {
    switch (height) {
    case 2:
        return 1;
    case 3:
        return 3;
    case 4:
        return 7;
    default:
        return 0;
    }
}

}
