#include "client/inventory_panel_renderer.hpp"

#include "client/inventory_item_visual.hpp"
#include "render/object_compositor.hpp"
#include "resources/tibia_spr.hpp"

#include <algorithm>
#include <cstddef>

namespace tibia::client {
namespace {

constexpr std::uint32_t inventory_texture_base = 0xe6000000U;

resources::TibiaSprImage compose_inventory_icon(
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

}

void InventoryPanelRenderer::draw(render::GraphicsRenderer& renderer,
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const GameWindowLayout& layout,
    std::uint32_t animation_clock) {
    const auto origin_x = layout.sidebar.x + inventory_panel_offset_x;
    const auto origin_y = inventory_panel_offset_y;
    for (std::size_t index = 0; index < inventory_slot_layouts.size(); ++index) {
        const auto& slot_layout = inventory_slot_layouts[index];
        const auto item = std::find_if(snapshot.inventory.begin(), snapshot.inventory.end(),
            [&slot_layout](const auto& entry) { return entry.slot == slot_layout.slot; });
        if (item == snapshot.inventory.end() || item->item.type == 0) continue;
        const auto object_id = static_cast<std::int32_t>(item->item.type);
        const auto* object = dat.find_item(static_cast<std::uint16_t>(object_id));
        if (!object) continue;
        const auto variant = inventory_item_variant(*object,
            item->item.has_extra ? item->item.extra : 0, animation_clock);
        auto& uploaded = uploaded_icons_[index];
        if (uploaded.object_id != object_id || uploaded.pattern_x != variant.pattern_x ||
            uploaded.pattern_z != variant.pattern_z || uploaded.phase != variant.phase) {
            try {
                renderer.upload(inventory_texture_base + static_cast<std::uint32_t>(index),
                    compose_inventory_icon(tibia_spr_bytes, tibia_spr, *object, variant));
                uploaded = {object_id, variant.pattern_x, variant.pattern_z, variant.phase};
            } catch (...) {
                uploaded = {};
                continue;
            }
        }
        renderer.draw(inventory_texture_base + static_cast<std::uint32_t>(index),
            origin_x + slot_layout.x,
            origin_y + slot_layout.y,
            inventory_item_slot_size, inventory_item_slot_size);
    }
}

}
