#include "resources/tibia_pic.hpp"

#include <cstddef>
#include <stdexcept>

namespace tibia::resources {
namespace {

class Reader {
public:
    Reader(std::span<const std::uint8_t> bytes, std::size_t offset = 0)
        : bytes_(bytes), offset_(offset) {
        if (offset > bytes.size()) throw std::runtime_error("Tibia.pic offset is outside the file");
    }
    std::uint8_t u8() { require(1); return bytes_[offset_++]; }
    std::uint16_t u16() {
        const auto lo = u8(); const auto hi = u8();
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
    void require(std::size_t n) const {
        if (n > bytes_.size() - offset_) throw std::runtime_error("truncated Tibia.pic data");
    }
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};

TibiaSprImage decode(std::span<const std::uint8_t> bytes, std::uint32_t offset, TibiaSprColor key) {
    TibiaSprImage image;
    image.file_offset = offset;
    image.color_key = key;
    if (offset == 0) return image;
    Reader reader(bytes, offset);
    const auto payload_size = reader.u16();
    if (payload_size > reader.remaining()) throw std::runtime_error("Tibia.pic tile payload is truncated");
    for (std::size_t pixel = 0; pixel < TibiaSprImage::width * TibiaSprImage::height; ++pixel) {
        const auto base = pixel * 4;
        image.rgba[base] = key.red; image.rgba[base + 1] = key.green;
        image.rgba[base + 2] = key.blue; image.rgba[base + 3] = 0;
    }
    std::size_t consumed = 0;
    std::size_t pixel = 0;
    while (consumed < payload_size) {
        if (payload_size - consumed < 4) throw std::runtime_error("incomplete Tibia.pic run header");
        const auto transparent = reader.u16();
        const auto colored = reader.u16();
        consumed += 4;
        if (transparent > 1024 - pixel) throw std::runtime_error("Tibia.pic transparent run exceeds tile");
        pixel += transparent;
        if (colored > 1024 - pixel) throw std::runtime_error("Tibia.pic colored run exceeds tile");
        if (static_cast<std::size_t>(colored) > (payload_size - consumed) / 3)
            throw std::runtime_error("Tibia.pic colored run exceeds payload");
        for (std::uint16_t i = 0; i < colored; ++i, ++pixel) {
            const auto base = pixel * 4;
            image.rgba[base] = reader.u8(); image.rgba[base + 1] = reader.u8();
            image.rgba[base + 2] = reader.u8(); image.rgba[base + 3] = 255;
        }
        consumed += static_cast<std::size_t>(colored) * 3;
    }
    return image;
}

} // namespace

TibiaPicFile parse_tibia_pic(std::span<const std::uint8_t> bytes) {
    Reader reader(bytes);
    TibiaPicFile result;
    result.signature = reader.u32();
    const auto count = reader.u16();
    result.pictures.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        TibiaPicPicture picture;
        picture.tiles_x = reader.u8();
        picture.tiles_y = reader.u8();
        picture.color_key = {reader.u8(), reader.u8(), reader.u8()};
        const auto tiles = static_cast<std::size_t>(picture.tiles_x) * picture.tiles_y;
        if (tiles > reader.remaining() / 4) throw std::runtime_error("Tibia.pic picture tile index is truncated");
        picture.tile_offsets.reserve(tiles);
        for (std::size_t tile = 0; tile < tiles; ++tile) picture.tile_offsets.push_back(reader.u32());
        result.pictures.push_back(std::move(picture));
    }
    result.parsed_index_bytes = reader.offset();
    return result;
}

TibiaSprImage decode_tibia_pic_tile(std::span<const std::uint8_t> bytes, const TibiaPicFile& file,
    std::uint16_t picture_id, std::uint16_t tile_x, std::uint16_t tile_y) {
    if (picture_id == 0 || picture_id > file.pictures.size())
        throw std::out_of_range("Tibia.pic picture ID is outside the index");
    const auto& picture = file.pictures[picture_id - 1];
    if (tile_x >= picture.tiles_x || tile_y >= picture.tiles_y)
        throw std::out_of_range("Tibia.pic tile coordinates are outside the picture");
    const auto index = static_cast<std::size_t>(tile_y) * picture.tiles_x + tile_x;
    return decode(bytes, picture.tile_offsets[index], picture.color_key);
}

} // namespace tibia::resources
