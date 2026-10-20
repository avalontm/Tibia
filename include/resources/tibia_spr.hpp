#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tibia::resources {

struct TibiaSprColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

struct TibiaSprImage {
    static constexpr std::size_t width = 32;
    static constexpr std::size_t height = 32;
    std::uint32_t file_offset{};
    TibiaSprColor color_key{};
    std::uint8_t content_width{static_cast<std::uint8_t>(width)};
    std::uint8_t content_height{static_cast<std::uint8_t>(height)};
    // Row-major RGBA pixels. The legacy color-key color is emitted with alpha 0.
    std::array<std::uint8_t, width * height * 4> rgba{};
};

struct TibiaSprFile {
    std::uint32_t signature{};
    std::vector<std::uint32_t> sprite_offsets;
    std::size_t parsed_index_bytes{};
};

// Parses the 7.92 sprite index and decodes one indexed 32x32 sprite on demand.
[[nodiscard]] TibiaSprFile parse_tibia_spr(std::span<const std::uint8_t> bytes);
[[nodiscard]] TibiaSprImage decode_tibia_spr_image(
    std::span<const std::uint8_t> bytes, const TibiaSprFile& file, std::uint16_t sprite_id);

} // namespace tibia::resources
