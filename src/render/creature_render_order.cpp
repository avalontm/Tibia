#include "render/creature_render_order.hpp"

#include <utility>

namespace tibia::render {

void sort_creature_render_order(std::span<CreatureRenderOrderEntry> entries) noexcept {
    if (entries.size() < 2) return;
    for (std::size_t first = 0; first < entries.size() - 1; ++first) {
        for (auto next = first + 1; next < entries.size(); ++next) {
            const auto& left = entries[first];
            const auto& right = entries[next];
            if (right.screen_y <= left.screen_y &&
                (left.screen_y != right.screen_y || right.screen_x <= left.screen_x))
                std::swap(entries[first], entries[next]);
        }
    }
}

}
