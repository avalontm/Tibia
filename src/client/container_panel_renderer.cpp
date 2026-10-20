#include "client/container_panel_renderer.hpp"

#include "client/inventory_item_visual.hpp"
#include "render/object_compositor.hpp"

#include <algorithm>
#include <cstddef>

namespace tibia::client {
namespace {

constexpr std::uint32_t container_texture_base = 0xe8000000U;

resources::TibiaSprImage compose_container_icon(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const resources::TibiaDatObject& object,
    const InventoryItemVariant& variant) {
    const auto surface = render::compose_tibia_dat_object(
        tibia_spr_bytes, tibia_spr, object, variant.pattern_x,
        variant.pattern_y, variant.pattern_z, variant.phase);
    resources::TibiaSprImage image;
    const auto& pixels = surface.pixels();
    for (std::uint32_t y = 0; y < resources::TibiaSprImage::height; ++y) {
        const auto source_y = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(y) * surface.height() /
            resources::TibiaSprImage::height);
        for (std::uint32_t x = 0; x < resources::TibiaSprImage::width; ++x) {
            const auto source_x = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(x) * surface.width() /
                resources::TibiaSprImage::width);
            const auto source = (static_cast<std::size_t>(source_y) * surface.width() +
                source_x) * 4U;
            const auto destination = (static_cast<std::size_t>(y) *
                resources::TibiaSprImage::width + x) * 4U;
            std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(source), 4,
                image.rgba.begin() + static_cast<std::ptrdiff_t>(destination));
        }
    }
    return image;
}

std::uint32_t container_texture_id(std::uint8_t container_id, std::size_t index) {
    return container_texture_base + static_cast<std::uint32_t>(container_id) * 256U +
        static_cast<std::uint32_t>(index);
}

}

void ContainerPanelRenderer::draw(render::GraphicsRenderer& renderer,
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const std::vector<ContainerPanelLayout>& panels,
    std::uint32_t animation_clock) {
    for (const auto& panel : panels) {
        const auto container = std::find_if(snapshot.containers.begin(),
            snapshot.containers.end(), [&panel](const auto& current) {
                return current.id == panel.id;
            });
        if (container == snapshot.containers.end()) continue;
        for (const auto& cell : panel.item_cells) {
            const auto& item = container->items[cell.item_index];
            if (item.type == 0) continue;
            const auto* object = dat.find_item(item.type);
            if (!object) continue;
            const auto variant = inventory_item_variant(*object,
                item.has_extra ? item.extra : 0, animation_clock);
            const auto texture_id = container_texture_id(panel.id, cell.item_index);
            auto& uploaded = uploaded_icons_[texture_id];
            if (uploaded.object_id != item.type || uploaded.pattern_x != variant.pattern_x ||
                uploaded.pattern_z != variant.pattern_z || uploaded.phase != variant.phase) {
                try {
                    renderer.upload(texture_id, compose_container_icon(tibia_spr_bytes,
                        tibia_spr, *object, variant));
                    uploaded = {item.type, variant.pattern_x, variant.pattern_z,
                        variant.phase};
                } catch (...) {
                    uploaded = {};
                    continue;
                }
            }
            renderer.draw(texture_id, cell.bounds.x, cell.bounds.y,
                cell.bounds.width, cell.bounds.height);
        }
    }
}

}
