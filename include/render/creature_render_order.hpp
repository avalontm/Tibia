#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tibia::render {

struct CreatureRenderOrderEntry {
    std::int32_t screen_x{};
    std::int32_t screen_y{};
    std::size_t source_index{};
};

void sort_creature_render_order(std::span<CreatureRenderOrderEntry> entries) noexcept;

}
