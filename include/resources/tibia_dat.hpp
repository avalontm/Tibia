#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tibia::resources {

struct TibiaDatHeader {
    std::uint32_t signature{};
    std::uint16_t last_item_id{};
    std::uint16_t last_outfit_id{};
    std::uint16_t last_effect_id{};
    std::uint16_t last_missile_id{};
};

struct TibiaDatAttribute {
    std::uint8_t id{};
    // The 7.92 reader consumes payload words only for a small observed set of
    // attribute IDs. Raw IDs are retained; names/semantics are not inferred.
    std::vector<std::uint16_t> payload_words;
};

struct TibiaDatObject {
    std::vector<TibiaDatAttribute> attributes;
    // Four native flag bytes populated from each attribute ID by FUN_004EA100.
    // Bits retain their numeric IDs; semantic labels are intentionally omitted.
    std::array<std::uint8_t, 4> native_property_bits{};
    std::uint8_t width{};
    std::uint8_t height{};
    std::uint8_t exact_size{};
    std::uint8_t layers{};
    std::uint8_t pattern_x{};
    std::uint8_t pattern_y{};
    std::uint8_t pattern_z{};
    std::uint8_t animation_frames{};
    std::vector<std::uint16_t> sprite_ids;

    // Selects the sprite using the client's observed one-based coordinate API.
    [[nodiscard]] std::uint16_t sprite_id_at(std::uint16_t x, std::uint16_t y,
        std::uint16_t layer, std::uint16_t pattern_x, std::uint16_t pattern_y,
        std::uint16_t pattern_z, std::uint16_t phase) const;
    [[nodiscard]] bool has_native_property(std::uint8_t property_id) const noexcept;
    [[nodiscard]] const TibiaDatAttribute* find_attribute(
        std::uint8_t attribute_id) const noexcept;
};

struct TibiaDatCategory {
    std::uint16_t first_id{};
    std::vector<TibiaDatObject> objects;
    [[nodiscard]] const TibiaDatObject* find(std::uint16_t object_id) const noexcept;
};

struct TibiaDatFile {
    TibiaDatHeader header;
    TibiaDatCategory items;
    TibiaDatCategory outfits;
    TibiaDatCategory effects;
    TibiaDatCategory missiles;
    std::size_t parsed_bytes{};
    std::vector<std::uint8_t> trailing_bytes;
    [[nodiscard]] const TibiaDatObject* find_item(std::uint16_t object_id) const noexcept;
};

// Parses the format observed in the Tibia 7.92 resource loader at 0x004F0500.
[[nodiscard]] TibiaDatFile parse_tibia_dat(std::span<const std::uint8_t> bytes);

} // namespace tibia::resources
