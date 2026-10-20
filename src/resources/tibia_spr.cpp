#include "resources/tibia_spr.hpp"

#include <stdexcept>
#include <string>

namespace tibia::resources {
namespace {

class Reader {
public:
    Reader(std::span<const std::uint8_t> bytes, std::size_t offset = 0)
        : bytes_(bytes), offset_(offset) {
        if (offset > bytes.size()) throw std::runtime_error("Tibia.spr offset is outside the file");
    }

    std::uint8_t u8() { require(1); return bytes_[offset_++]; }
    std::uint16_t u16() {
        const auto lo = u8();
        const auto hi = u8();
        return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8U));
    }
    std::uint32_t u32() {
        const auto b0 = u8(); const auto b1 = u8();
        const auto b2 = u8(); const auto b3 = u8();
        return static_cast<std::uint32_t>(b0) | (static_cast<std::uint32_t>(b1) << 8U) |
            (static_cast<std::uint32_t>(b2) << 16U) | (static_cast<std::uint32_t>(b3) << 24U);
    }
    std::size_t offset() const noexcept { return offset_; }
    std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - offset_) throw std::runtime_error("truncated Tibia.spr data");
    }
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};

} // namespace

TibiaSprFile parse_tibia_spr(std::span<const std::uint8_t> bytes) {
    Reader reader(bytes);
    TibiaSprFile result;
    result.signature = reader.u32();
    const auto count = reader.u16();
    if (count > reader.remaining() / 4) throw std::runtime_error("Tibia.spr sprite index is truncated");
    result.sprite_offsets.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) result.sprite_offsets.push_back(reader.u32());
    result.parsed_index_bytes = reader.offset();
    return result;
}

TibiaSprImage decode_tibia_spr_image(
    std::span<const std::uint8_t> bytes, const TibiaSprFile& file, std::uint16_t sprite_id) {
    if (sprite_id == 0 || sprite_id > file.sprite_offsets.size())
        throw std::out_of_range("Tibia.spr sprite ID " + std::to_string(sprite_id) +
            " is outside the 1.." + std::to_string(file.sprite_offsets.size()) + " index");

    TibiaSprImage image;
    image.file_offset = file.sprite_offsets[sprite_id - 1];
    if (image.file_offset == 0) return image;
    Reader reader(bytes, image.file_offset);
    image.color_key = {reader.u8(), reader.u8(), reader.u8()};
    const auto payload_size = reader.u16();
    if (payload_size > reader.remaining()) throw std::runtime_error("Tibia.spr sprite payload is truncated");

    for (std::size_t pixel = 0; pixel < TibiaSprImage::width * TibiaSprImage::height; ++pixel) {
        const auto base = pixel * 4;
        image.rgba[base] = image.color_key.red;
        image.rgba[base + 1] = image.color_key.green;
        image.rgba[base + 2] = image.color_key.blue;
        image.rgba[base + 3] = 0;
    }

    std::size_t consumed = 0;
    std::size_t pixel = 0;
    while (consumed < payload_size) {
        if (payload_size - consumed < 4) throw std::runtime_error("incomplete Tibia.spr run header");
        const auto transparent = reader.u16();
        const auto colored = reader.u16();
        consumed += 4;
        if (transparent > TibiaSprImage::width * TibiaSprImage::height - pixel)
            throw std::runtime_error("Tibia.spr transparent run exceeds the sprite canvas");
        pixel += transparent;
        if (colored > TibiaSprImage::width * TibiaSprImage::height - pixel)
            throw std::runtime_error("Tibia.spr colored run exceeds the sprite canvas");
        if (static_cast<std::size_t>(colored) > (payload_size - consumed) / 3)
            throw std::runtime_error("Tibia.spr colored run exceeds its payload");
        for (std::uint16_t i = 0; i < colored; ++i, ++pixel) {
            const auto red = reader.u8();
            const auto green = reader.u8();
            const auto blue = reader.u8();
            const auto base = pixel * 4;
            image.rgba[base] = red;
            image.rgba[base + 1] = green;
            image.rgba[base + 2] = blue;
            image.rgba[base + 3] = 255;
        }
        consumed += static_cast<std::size_t>(colored) * 3;
    }
    return image;
}

} // namespace tibia::resources
