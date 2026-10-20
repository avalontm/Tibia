#include "render/outfit_compositor.hpp"

#include <stdexcept>
#include <cmath>

namespace tibia::render {
namespace {

bool mask_label(const resources::TibiaSprImage& mask, std::size_t i, std::size_t& label) {
    const auto r = mask.rgba[i];
    const auto g = mask.rgba[i + 1];
    const auto b = mask.rgba[i + 2];
    if (r == 255 && g == 255 && b == 0) label = 0;
    else if (r == 255 && g == 0 && b == 0) label = 1;
    else if (r == 0 && g == 255 && b == 0) label = 2;
    else if (r == 0 && g == 0 && b == 255) label = 3;
    else return false;
    return true;
}

std::uint8_t shade(std::uint8_t color, std::uint32_t luminance) {
    return static_cast<std::uint8_t>((static_cast<std::uint32_t>(color) * luminance) >> 8U);
}

std::uint8_t channel(double value) {
    const auto rounded = std::nearbyint(value * 255.0);
    return static_cast<std::uint8_t>(rounded < 0.0 ? 0.0 : rounded > 255.0 ? 255.0 : rounded);
}

} // namespace

resources::TibiaSprColor tibia_outfit_color(std::uint16_t selector) {
    if (selector > 132) selector = 0;
    const auto group = static_cast<unsigned>(selector / 19);
    const auto step = static_cast<unsigned>(selector % 19);
    if (step == 0) {
        const auto value = 1.0 - static_cast<double>(group) / 7.0;
        const auto gray = channel(value);
        return {gray, gray, gray};
    }
    const double hue = static_cast<double>(step) / 18.0;
    double saturation = 1.0;
    double value = 1.0;
    switch (group) {
    case 0: saturation = 0.25; break;
    case 1: saturation = 0.25; value = 0.75; break;
    case 2: saturation = 0.5; value = 0.75; break;
    case 3: saturation = 2.0 / 3.0; value = 0.75; break;
    case 5: value = 0.75; break;
    case 6: value = 0.5; break;
    default: break;
    }
    const double scaled_hue = hue * 6.0;
    const auto sector = static_cast<unsigned>(scaled_hue);
    const double fraction = scaled_hue - sector;
    const double minimum = value * (1.0 - saturation);
    const double falling = value * (1.0 - saturation * fraction);
    const double rising = value * (1.0 - saturation * (1.0 - fraction));
    double red{}, green{}, blue{};
    switch (sector) {
    case 0: red = value; green = rising; blue = minimum; break;
    case 1: red = falling; green = value; blue = minimum; break;
    case 2: red = minimum; green = value; blue = rising; break;
    case 3: red = minimum; green = falling; blue = value; break;
    case 4: red = rising; green = minimum; blue = value; break;
    default: red = value; green = minimum; blue = falling; break;
    }
    return {channel(red), channel(green), channel(blue)};
}

resources::TibiaSprImage colorize_tibia_outfit_tile(const resources::TibiaSprImage& base,
    const resources::TibiaSprImage& mask, const OutfitPalette& palette) {
    resources::TibiaSprImage result = base;
    for (std::size_t i = 0; i < result.rgba.size(); i += 4) {
        std::size_t label{};
        if (!mask_label(mask, i, label)) continue;
        const auto luminance = (static_cast<std::uint32_t>(base.rgba[i + 1]) * 59U +
            static_cast<std::uint32_t>(base.rgba[i + 2]) * 11U +
            static_cast<std::uint32_t>(base.rgba[i]) * 30U) / 100U;
        result.rgba[i] = shade(palette[label].red, luminance);
        result.rgba[i + 1] = shade(palette[label].green, luminance);
        result.rgba[i + 2] = shade(palette[label].blue, luminance);
        result.rgba[i + 3] = 255;
    }
    return result;
}

SoftwareSurface compose_tibia_outfit(std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr, const resources::TibiaDatObject& outfit,
    const OutfitRenderOptions& options, const OutfitPalette& palette) {
    if (outfit.layers != 2)
        throw std::invalid_argument("outfit rendering requires the observed base and mask layers");
    if (outfit.width == 0 || outfit.height == 0)
        throw std::invalid_argument("Tibia.dat outfit has no drawable tile grid");
    SoftwareSurface result(static_cast<std::uint32_t>(outfit.width) * 32U,
        static_cast<std::uint32_t>(outfit.height) * 32U, {0, 0, 0, 0});
    if (options.direction == 0 || options.direction > outfit.pattern_x ||
        options.pattern_z == 0 || options.pattern_z > outfit.pattern_z ||
        options.phase == 0 || options.phase > outfit.animation_frames)
        throw std::out_of_range("outfit direction, pattern, or phase is outside the DAT object");
    std::uint32_t permitted_addons = 0;
    if (outfit.pattern_y == 2) permitted_addons = 1;
    else if (outfit.pattern_y == 3) permitted_addons = 3;
    else if (outfit.pattern_y == 4) permitted_addons = 7;
    const auto available_patterns_y = permitted_addons == 0 ? 1 : outfit.pattern_y;
    for (std::uint16_t pattern_y = 1; pattern_y <= available_patterns_y; ++pattern_y) {
        if (pattern_y > 1) {
            const auto bit = 1U << (pattern_y - 2);
            if ((options.addon_mask & bit) == 0) continue;
        }
        for (std::uint16_t y = 1; y <= outfit.height; ++y) {
            for (std::uint16_t x = 1; x <= outfit.width; ++x) {
                const auto base_id = outfit.sprite_id_at(x, y, 1, options.direction,
                    pattern_y, options.pattern_z, options.phase);
                const auto mask_id = outfit.sprite_id_at(x, y, 2, options.direction,
                    pattern_y, options.pattern_z, options.phase);
                if (base_id == 0) continue;
                const auto base = resources::decode_tibia_spr_image(tibia_spr_bytes, tibia_spr, base_id);
                const auto mask = mask_id == 0 ? resources::TibiaSprImage{} :
                    resources::decode_tibia_spr_image(tibia_spr_bytes, tibia_spr, mask_id);
                result.draw(colorize_tibia_outfit_tile(base, mask, palette),
                    static_cast<int>((outfit.width - x) * 32),
                    static_cast<int>((outfit.height - y) * 32));
            }
        }
    }
    return result;
}

} // namespace tibia::render
