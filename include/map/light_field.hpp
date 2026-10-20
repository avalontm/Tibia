#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tibia::map {

inline constexpr std::size_t light_field_width = 18;
inline constexpr std::size_t light_field_height = 14;
inline constexpr std::size_t light_field_size = light_field_width * light_field_height;
inline constexpr std::size_t light_field_floor_count = 8;
using LightCellMask = std::array<std::uint8_t, light_field_size>;

struct Vector3i {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
};

struct RgbLight {
    std::array<std::uint8_t, 3> channels{};
};

struct LightSource {
    std::int32_t pixel_x{};
    std::int32_t pixel_y{};
    std::int32_t radius{};
    std::uint8_t floor{};
    RgbLight color;
};

[[nodiscard]] Vector3i scale(Vector3i value, std::int32_t factor) noexcept;
[[nodiscard]] Vector3i divide(Vector3i value, std::int32_t divisor);
[[nodiscard]] Vector3i component_max(Vector3i value, Vector3i other) noexcept;
[[nodiscard]] float round_down(float value) noexcept;

class LightField {
public:
    LightField(std::array<RgbLight, light_field_size> colors,
        std::array<std::int32_t, light_field_size> field_values) noexcept;

    [[nodiscard]] RgbLight clamped_color(std::int32_t x, std::int32_t y) const noexcept;
    [[nodiscard]] std::uint8_t average_color(std::int32_t x, std::int32_t y) const noexcept;
    [[nodiscard]] std::optional<bool> field_at_most(
        std::int32_t x, std::int32_t y, std::int32_t value) const noexcept;
    void blend(std::int32_t x, std::int32_t y, std::uint8_t brightness,
        RgbLight source, std::uint8_t ambient_brightness, bool floor_below_eight) noexcept;
    void add_source(const LightSource& source, const LightCellMask& cells_with_property_00) noexcept;

private:
    [[nodiscard]] static std::size_t index(std::int32_t x, std::int32_t y) noexcept;
    std::array<RgbLight, light_field_size> colors_;
    std::array<std::int32_t, light_field_size> field_values_;
};

}
