#include "network/packet_stream.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace tibia::network {

void PacketWriter::write_u8(std::uint8_t value) {
    bytes_.push_back(value);
}

void PacketWriter::write_u16(std::uint16_t value) {
    bytes_.push_back(static_cast<std::uint8_t>(value));
    bytes_.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void PacketWriter::write_u32(std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void PacketWriter::write_bytes(std::span<const std::uint8_t> value) {
    bytes_.insert(bytes_.end(), value.begin(), value.end());
}

void PacketWriter::write_string(std::string_view value) {
    if (value.size() > 0xFFFFU) {
        throw std::invalid_argument("packet string exceeds its 16-bit length field");
    }
    write_u16(static_cast<std::uint16_t>(value.size()));
    std::transform(value.begin(), value.end(), std::back_inserter(bytes_),
                   [](char ch) { return static_cast<std::uint8_t>(ch); });
}

const std::vector<std::uint8_t>& PacketWriter::bytes() const noexcept {
    return bytes_;
}

std::vector<std::uint8_t> PacketWriter::take() && noexcept {
    return std::move(bytes_);
}

PacketReader::PacketReader(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

void PacketReader::require(std::size_t count) const {
    if (count > bytes_.size() - position_) {
        throw std::invalid_argument("packet ended before the requested field");
    }
}

std::uint8_t PacketReader::read_u8() {
    require(1);
    return bytes_[position_++];
}

std::uint16_t PacketReader::read_u16() {
    require(2);
    const auto value = static_cast<std::uint16_t>(bytes_[position_]) |
                       (static_cast<std::uint16_t>(bytes_[position_ + 1]) << 8U);
    position_ += 2;
    return value;
}

std::uint32_t PacketReader::read_u32() {
    require(4);
    const auto value = static_cast<std::uint32_t>(bytes_[position_]) |
                       (static_cast<std::uint32_t>(bytes_[position_ + 1]) << 8U) |
                       (static_cast<std::uint32_t>(bytes_[position_ + 2]) << 16U) |
                       (static_cast<std::uint32_t>(bytes_[position_ + 3]) << 24U);
    position_ += 4;
    return value;
}

std::span<const std::uint8_t> PacketReader::read_bytes(std::size_t count) {
    require(count);
    const auto result = bytes_.subspan(position_, count);
    position_ += count;
    return result;
}

std::string PacketReader::read_string() {
    const auto length = static_cast<std::size_t>(read_u16());
    const auto field = read_bytes(length);
    if (field.empty()) {
        return {};
    }
    return {reinterpret_cast<const char*>(field.data()), field.size()};
}

std::size_t PacketReader::position() const noexcept {
    return position_;
}

std::size_t PacketReader::remaining() const noexcept {
    return bytes_.size() - position_;
}

std::span<const std::uint8_t> PacketReader::remaining_bytes() const noexcept {
    return bytes_.subspan(position_);
}

} // namespace tibia::network
