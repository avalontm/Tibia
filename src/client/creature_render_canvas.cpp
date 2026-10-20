#include "client/creature_render_canvas.hpp"

#include <limits>

namespace tibia::client {

std::size_t draw_creature_render_batch(
    render::SoftwareSurface& canvas,
    const CreatureRenderBatch& batch,
    std::int32_t origin_x,
    std::int32_t origin_y) {
    std::size_t drawn{};
    for (const auto& creature : batch.entries) {
        const auto x = static_cast<std::int64_t>(creature.sort_x) -
            (static_cast<std::int64_t>(creature.surface.width()) - 32) + origin_x;
        const auto y = static_cast<std::int64_t>(creature.sort_y) -
            (static_cast<std::int64_t>(creature.surface.height()) - 32) + origin_y;
        if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max() ||
            y < std::numeric_limits<int>::min() || y > std::numeric_limits<int>::max())
            continue;
        canvas.draw(creature.surface, static_cast<int>(x), static_cast<int>(y));
        ++drawn;
    }
    return drawn;
}

std::size_t draw_creature_render_batch(
    render::SoftwareSurface& canvas,
    const CreatureRenderBatch& batch,
    const MapWindowLayout& layout) {
    return draw_creature_render_batch(canvas, batch, layout.x, layout.y);
}

}
