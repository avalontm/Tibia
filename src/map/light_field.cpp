#include "map/light_field.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace tibia::map {

Vector3i scale(Vector3i value, std::int32_t factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

Vector3i divide(Vector3i value, std::int32_t divisor) {
    if (divisor == 0) throw std::invalid_argument("vector divisor must be nonzero");
    return {value.x / divisor, value.y / divisor, value.z / divisor};
}

Vector3i component_max(Vector3i value, Vector3i other) noexcept {
    return {std::max(value.x, other.x), std::max(value.y, other.y),
        std::max(value.z, other.z)};
}

float round_down(float value) noexcept {
    return std::floor(value);
}

LightField::LightField(std::array<RgbLight, light_field_size> colors,
    std::array<std::int32_t, light_field_size> field_values) noexcept
    : colors_(std::move(colors)), field_values_(std::move(field_values)) {}

std::size_t LightField::index(std::int32_t x, std::int32_t y) noexcept {
    return static_cast<std::size_t>(x * static_cast<std::int32_t>(light_field_height) + y);
}

RgbLight LightField::clamped_color(std::int32_t x, std::int32_t y) const noexcept {
    x = std::clamp(x, 0, static_cast<std::int32_t>(light_field_width - 1));
    y = std::clamp(y, 0, static_cast<std::int32_t>(light_field_height - 1));
    return colors_[index(x, y)];
}

std::uint8_t LightField::average_color(std::int32_t x, std::int32_t y) const noexcept {
    const auto color = colors_[index(x, y)];
    const auto sum = static_cast<std::uint32_t>(color.channels[0]) +
        color.channels[1] + color.channels[2];
    return static_cast<std::uint8_t>(sum / 3);
}

std::optional<bool> LightField::field_at_most(
    std::int32_t x, std::int32_t y, std::int32_t value) const noexcept {
    if (x < 0 || x >= static_cast<std::int32_t>(light_field_width) ||
        y < 0 || y >= static_cast<std::int32_t>(light_field_height))
        return std::nullopt;
    return field_values_[index(x, y)] <= value;
}

void LightField::blend(std::int32_t x, std::int32_t y, std::uint8_t brightness,
    RgbLight source, std::uint8_t ambient_brightness, bool floor_below_eight) noexcept {
    ambient_brightness = std::min<std::uint8_t>(ambient_brightness, 128);
    const auto ambient_limit = floor_below_eight ? 200u : 255u;
    const auto ambient_rg = ambient_limit * ambient_brightness / 255u;
    const auto red_source = static_cast<std::uint32_t>(source.channels[0]) * brightness / 255u;
    const auto inverse_brightness = 255u - red_source;
    auto& color = colors_[index(x, y)];
    for (std::size_t channel = 0; channel < color.channels.size(); ++channel) {
        const auto ambient = channel == 2 ? ambient_brightness : ambient_rg;
        const auto source_value = channel == 0 ? red_source :
            static_cast<std::uint32_t>(source.channels[channel]) * brightness / 255u;
        const auto value = source_value + ambient * inverse_brightness / 255u;
        color.channels[channel] = static_cast<std::uint8_t>(std::min(value, 255u));
    }
}

void LightField::add_source(const LightSource& source,
    const LightCellMask& cells_with_property_00) noexcept {
    if (source.radius < 1) return;
    constexpr std::int64_t cell_pixels = 32;
    const auto radius_pixels = static_cast<std::int64_t>(source.radius) * cell_pixels;
    const auto floor_tile = [](std::int32_t pixel) {
        const auto value = static_cast<std::int64_t>(pixel);
        const auto quotient = value / cell_pixels;
        return quotient - (value < 0 && value % cell_pixels != 0 ? 1 : 0);
    };
    const auto source_tile_x = floor_tile(source.pixel_x);
    const auto source_tile_y = floor_tile(source.pixel_y);
    const auto first_x = std::max<std::int64_t>(0,
        source_tile_x - source.radius + 1);
    const auto end_x = std::min<std::int64_t>(light_field_width,
        source_tile_x + source.radius);
    const auto first_y = std::max<std::int64_t>(0,
        source_tile_y - source.radius + 1);
    const auto end_y = std::min<std::int64_t>(light_field_height,
        source_tile_y + source.radius);
    for (auto x = first_x; x < end_x; ++x) {
        for (auto y = first_y; y < end_y; ++y) {
            const auto offset_x = static_cast<std::int64_t>(x) * cell_pixels -
                source.pixel_x + cell_pixels / 2;
            const auto offset_y = static_cast<std::int64_t>(y) * cell_pixels -
                source.pixel_y + cell_pixels / 2;
            const auto squared_distance = offset_x * offset_x + offset_y * offset_y;
            const auto distance = static_cast<std::int64_t>(std::nearbyint(
                std::sqrt(static_cast<long double>(squared_distance))));
            const auto remaining = radius_pixels - distance;
            if (remaining <= 0) continue;

            auto& color = colors_[index(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y))];
            for (std::size_t channel = 0; channel < color.channels.size(); ++channel) {
                auto contribution = static_cast<std::uint32_t>(
                    source.color.channels[channel]) * static_cast<std::uint32_t>(remaining) / 160U;
                if (cells_with_property_00[index(static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y))] == 0)
                    contribution = contribution * 4U / 5U;
                color.channels[channel] = static_cast<std::uint8_t>(std::min<std::uint32_t>(
                    std::max<std::uint32_t>(color.channels[channel], contribution), 255U));
            }
        }
    }
}

}
