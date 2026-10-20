#pragma once

#include "client/talkbox_layout.hpp"
#include "client/map_window_layout.hpp"
#include "render/graphics_renderer.hpp"
#include "resources/tibia_spr.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>

namespace tibia::client {

struct TalkboxGlyphMetrics {
    std::int32_t width{};
    std::int32_t height{};
    std::int32_t advance{};
};

struct TalkboxFontView {
    std::span<const resources::TibiaSprImage> images;
    std::span<const TalkboxGlyphMetrics> glyphs;
    std::int32_t line_height{};
    std::int32_t letter_spacing{};
    std::int32_t missing_glyph_advance{};
};

class TalkboxRenderer {
public:
    void draw(render::GraphicsRenderer& renderer,
        const std::array<Talkbox, TalkSystem::talkbox_count>& talkboxes,
        const network::State6DecodedMap& map,
        const TalkboxLayoutContext& context,
        const TalkboxFontView& font,
        const MapWindowLayout& clip);

private:
    [[nodiscard]] std::uint32_t glyph_texture(render::GraphicsRenderer& renderer,
        const resources::TibiaSprImage& source, TalkboxColor color,
        std::size_t glyph_index);

    std::unordered_map<std::uint64_t, std::uint32_t> glyph_textures_;
    std::uint32_t next_texture_{0xfa000000U};
};

}
