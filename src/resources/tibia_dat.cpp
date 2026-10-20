#include "resources/tibia_dat.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace tibia::resources {

bool TibiaDatObject::has_native_property(std::uint8_t property_id) const noexcept {
    if (property_id >= native_property_bits.size() * 8U) return false;
    return (native_property_bits[property_id / 8U] &
        static_cast<std::uint8_t>(1U << (property_id % 8U))) != 0;
}

const TibiaDatAttribute* TibiaDatObject::find_attribute(std::uint8_t attribute_id) const noexcept {
    const auto found = std::find_if(attributes.begin(), attributes.end(),
        [attribute_id](const TibiaDatAttribute& attribute) { return attribute.id == attribute_id; });
    return found == attributes.end() ? nullptr : &*found;
}

const TibiaDatObject* TibiaDatCategory::find(std::uint16_t object_id) const noexcept {
    if (object_id < first_id) return nullptr;
    const auto index = static_cast<std::size_t>(object_id - first_id);
    return index < objects.size() ? &objects[index] : nullptr;
}

const TibiaDatObject* TibiaDatFile::find_item(std::uint16_t object_id) const noexcept {
    return items.find(object_id);
}

std::uint16_t TibiaDatObject::sprite_id_at(std::uint16_t x, std::uint16_t y,
    std::uint16_t layer, std::uint16_t pattern_x, std::uint16_t pattern_y,
    std::uint16_t pattern_z, std::uint16_t phase) const {
    if (x == 0 || x > width || y == 0 || y > height || layer == 0 || layer > layers ||
        pattern_x == 0 || pattern_x > this->pattern_x || pattern_y == 0 || pattern_y > this->pattern_y ||
        pattern_z == 0 || pattern_z > this->pattern_z || phase == 0 || phase > animation_frames)
        throw std::out_of_range("Tibia.dat sprite selector is outside the object dimensions");

    // Recovered from FUN_004e9a00: phase, pattern-Z, pattern-Y, pattern-X,
    // layer, tile-Y, tile-X; all selector coordinates are one-based.
    std::size_t index = phase - 1;
    index = index * this->pattern_z + (pattern_z - 1);
    index = index * this->pattern_y + (pattern_y - 1);
    index = index * this->pattern_x + (pattern_x - 1);
    index = index * layers + (layer - 1);
    index = index * height + (y - 1);
    index = index * width + (x - 1);
    if (index >= sprite_ids.size()) throw std::runtime_error("Tibia.dat sprite index exceeds the parsed object data");
    return sprite_ids[index];
}

namespace {

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

    [[nodiscard]] std::uint8_t read_u8() {
        require(1);
        return bytes_[offset_++];
    }

    [[nodiscard]] std::uint16_t read_u16() {
        const auto low = read_u8();
        const auto high = read_u8();
        return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8U));
    }

    [[nodiscard]] std::uint32_t read_u32() {
        const auto b0 = read_u8();
        const auto b1 = read_u8();
        const auto b2 = read_u8();
        const auto b3 = read_u8();
        return static_cast<std::uint32_t>(b0) |
            (static_cast<std::uint32_t>(b1) << 8U) |
            (static_cast<std::uint32_t>(b2) << 16U) |
            (static_cast<std::uint32_t>(b3) << 24U);
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

    [[nodiscard]] std::vector<std::uint8_t> take_remaining() {
        const auto first = bytes_.begin() + static_cast<std::ptrdiff_t>(offset_);
        std::vector<std::uint8_t> result(first, bytes_.end());
        offset_ = bytes_.size();
        return result;
    }

private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - offset_) {
            throw std::runtime_error("Tibia.dat is truncated at byte offset " +
                std::to_string(offset_));
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};

[[nodiscard]] std::size_t payload_word_count(std::uint8_t attribute_id) noexcept {
    switch (attribute_id) {
    case 0x00:
    case 0x09:
    case 0x0A:
    case 0x1A:
    case 0x1D:
    case 0x1E:
        return 1;
    case 0x16:
    case 0x19:
        return 2;
    default:
        return 0;
    }
}

[[nodiscard]] TibiaDatObject read_object(Reader& reader) {
    TibiaDatObject object;
    while (true) {
        const auto id = reader.read_u8();
        if (id == 0xFF) {
            break;
        }
        TibiaDatAttribute attribute;
        attribute.id = id;
        if (id < 32) {
            object.native_property_bits[id / 8U] |=
                static_cast<std::uint8_t>(1U << (id % 8U));
        }
        const auto words = payload_word_count(id);
        attribute.payload_words.reserve(words);
        for (std::size_t index = 0; index < words; ++index) {
            attribute.payload_words.push_back(reader.read_u16());
        }
        object.attributes.push_back(std::move(attribute));
    }

    object.width = reader.read_u8();
    object.height = reader.read_u8();
    if (object.width < 2 && object.height < 2) {
        object.exact_size = 0x20;
    } else {
        object.exact_size = reader.read_u8();
    }
    object.layers = reader.read_u8();
    object.pattern_x = reader.read_u8();
    object.pattern_y = reader.read_u8();
    object.pattern_z = reader.read_u8();
    object.animation_frames = reader.read_u8();

    const std::uint64_t sprite_count =
        static_cast<std::uint64_t>(object.width) * object.height * object.layers *
        object.pattern_x * object.pattern_y * object.pattern_z * object.animation_frames;
    // The binary rejects counts >= 0x4001 before allocating its sprite-ID array.
    if (sprite_count >= 0x4001U ||
        sprite_count > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Tibia.dat object sprite count exceeds the client limit");
    }
    if (sprite_count > reader.remaining() / sizeof(std::uint16_t)) {
        throw std::runtime_error("Tibia.dat sprite-ID list is truncated at byte offset " +
            std::to_string(reader.offset()));
    }
    object.sprite_ids.reserve(static_cast<std::size_t>(sprite_count));
    for (std::uint64_t index = 0; index < sprite_count; ++index) {
        object.sprite_ids.push_back(reader.read_u16());
    }
    return object;
}

[[nodiscard]] TibiaDatCategory read_category(
    Reader& reader,
    std::uint16_t first_id,
    std::uint32_t count) {
    if (count > reader.remaining() / 8U) {
        throw std::runtime_error("Tibia.dat category count is inconsistent with remaining bytes");
    }
    TibiaDatCategory category;
    category.first_id = first_id;
    category.objects.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        category.objects.push_back(read_object(reader));
    }
    return category;
}

} // namespace

TibiaDatFile parse_tibia_dat(std::span<const std::uint8_t> bytes) {
    Reader reader(bytes);
    TibiaDatFile result;
    result.header.signature = reader.read_u32();
    result.header.last_item_id = reader.read_u16();
    result.header.last_outfit_id = reader.read_u16();
    result.header.last_effect_id = reader.read_u16();
    result.header.last_missile_id = reader.read_u16();

    if (result.header.last_item_id < 100) {
        throw std::runtime_error("Tibia.dat item-ID range ends before its first observed ID");
    }
    result.items = read_category(reader, 100,
        static_cast<std::uint32_t>(result.header.last_item_id) - 100U + 1U);
    result.outfits = read_category(reader, 1, result.header.last_outfit_id);
    result.effects = read_category(reader, 1, result.header.last_effect_id);
    result.missiles = read_category(reader, 1, result.header.last_missile_id);
    result.parsed_bytes = reader.offset();
    result.trailing_bytes = reader.take_remaining();
    return result;
}

} // namespace tibia::resources
