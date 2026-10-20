#include "client/talkbox_renderer.hpp"

#include <algorithm>
#include <string_view>

namespace tibia::client {
namespace {

constexpr std::size_t glyph_count = 224;

std::string_view visible_text(const std::string& text) noexcept {
    const auto terminator = text.find('\0');
    return text.substr(0, terminator);
}

std::int32_t glyph_index(unsigned char character) noexcept {
    if (character > 32) return static_cast<std::int32_t>(character) - 32;
    if (character == ' ') return -1;
    return 0x5f;
}

std::int32_t advance_for(std::int32_t index, const TalkboxFontView& font) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= font.glyphs.size())
        return font.missing_glyph_advance;
    return font.glyphs[static_cast<std::size_t>(index)].advance;
}

std::int32_t text_width(std::string_view text, const TalkboxFontView& font) noexcept {
    std::int32_t width{};
    std::size_t count{};
    for (const unsigned char character : text) {
        width += advance_for(glyph_index(character), font);
        ++count;
    }
    if (count > 1)
        width += static_cast<std::int32_t>(count - 1) * font.letter_spacing;
    return width;
}

std::array<TalkboxTextExtent, TalkSystem::talkbox_count> measure_talkboxes(
    const std::array<Talkbox, TalkSystem::talkbox_count>& talkboxes,
    const TalkboxFontView& font) {
    std::array<TalkboxTextExtent, TalkSystem::talkbox_count> extents{};
    for (std::size_t index = 0; index < talkboxes.size(); ++index) {
        const auto& talkbox = talkboxes[index];
        if (talkbox.state != 1) continue;
        for (const auto& line : talkbox.lines)
            extents[index].width = std::max(extents[index].width,
                text_width(visible_text(line.text), font));
        extents[index].height = static_cast<std::int32_t>(talkbox.lines.size()) *
            font.line_height;
    }
    return extents;
}

}

void TalkboxRenderer::draw(render::GraphicsRenderer& renderer,
    const std::array<Talkbox, TalkSystem::talkbox_count>& talkboxes,
    const network::State6DecodedMap& map,
    const TalkboxLayoutContext& context,
    const TalkboxFontView& font,
    const MapWindowLayout& clip) {
    if (font.images.size() < glyph_count || font.glyphs.size() < glyph_count ||
        font.line_height <= 0 || clip.width <= 0 || clip.height <= 0)
        return;

    const auto extents = measure_talkboxes(talkboxes, font);
    const auto placements = layout_talkboxes(talkboxes, extents, map, context);
    const auto commands = build_talkbox_text_commands(talkboxes, placements,
        font.line_height, clip.x, clip.y);
    for (const auto& command : commands) {
        const auto line = visible_text(command.text);
        auto x = command.anchor_x;
        const auto width = text_width(line, font);
        if (command.alignment == TalkboxTextAlignment::center) x -= width / 2;
        if (command.alignment == TalkboxTextAlignment::right) x -= width;

        for (const unsigned char character : line) {
            const auto index = glyph_index(character);
            if (index >= 0 && static_cast<std::size_t>(index) < glyph_count) {
                const auto glyph_index_value = static_cast<std::size_t>(index);
                const auto& glyph = font.glyphs[glyph_index_value];
                if (glyph.width > 0 && glyph.height > 0)
                    renderer.draw_clipped(glyph_texture(renderer,
                        font.images[glyph_index_value], command.color, glyph_index_value),
                        x, command.top_y, glyph.width, glyph.height,
                        clip.x, clip.y, clip.width, clip.height);
            }
            x += advance_for(index, font) + font.letter_spacing;
        }
    }
}

std::uint32_t TalkboxRenderer::glyph_texture(render::GraphicsRenderer& renderer,
    const resources::TibiaSprImage& source, TalkboxColor color,
    std::size_t glyph_index_value) {
    const auto rgb = (static_cast<std::uint32_t>(color.red) << 16U) |
        (static_cast<std::uint32_t>(color.green) << 8U) | color.blue;
    const auto key = (static_cast<std::uint64_t>(rgb) << 8U) |
        static_cast<std::uint64_t>(glyph_index_value);
    if (const auto found = glyph_textures_.find(key); found != glyph_textures_.end())
        return found->second;

    auto tinted = source;
    for (std::size_t offset = 0; offset < tinted.rgba.size(); offset += 4U) {
        if (tinted.rgba[offset + 3] == 0) continue;
        tinted.rgba[offset] = static_cast<std::uint8_t>(
            static_cast<unsigned>(tinted.rgba[offset]) * color.red / 255U);
        tinted.rgba[offset + 1] = static_cast<std::uint8_t>(
            static_cast<unsigned>(tinted.rgba[offset + 1]) * color.green / 255U);
        tinted.rgba[offset + 2] = static_cast<std::uint8_t>(
            static_cast<unsigned>(tinted.rgba[offset + 2]) * color.blue / 255U);
    }
    const auto texture = next_texture_++;
    renderer.upload(texture, tinted);
    glyph_textures_.emplace(key, texture);
    return texture;
}

}
