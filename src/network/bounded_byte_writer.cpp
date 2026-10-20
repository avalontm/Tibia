#include "network/bounded_byte_writer.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace tibia::network {

BoundedByteWriter::BoundedByteWriter(std::span<std::uint8_t> storage) noexcept
    : storage_(storage) {}

void BoundedByteWriter::reset() noexcept {
    position_ = 0;
}

void BoundedByteWriter::require(std::size_t count) const {
    if (count > storage_.size() - position_) {
        throw std::out_of_range("bounded byte writer capacity exceeded");
    }
}

void BoundedByteWriter::write_u8(std::uint8_t value) {
    require(1);
    storage_[position_++] = value;
}

void BoundedByteWriter::write_u16(std::uint16_t value) {
    require(2);
    storage_[position_] = static_cast<std::uint8_t>(value);
    storage_[position_ + 1] = static_cast<std::uint8_t>(value >> 8U);
    position_ += 2;
}

void BoundedByteWriter::write_u32(std::uint32_t value) {
    require(4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        storage_[position_++] = static_cast<std::uint8_t>(value >> shift);
    }
}

void BoundedByteWriter::write_c_string(const char* value) {
    if (value == nullptr) {
        throw std::invalid_argument("bounded byte writer string is null");
    }
    const auto length = std::strlen(value);
    write_u16(static_cast<std::uint16_t>(length));
    require(length);
    std::transform(value, value + length, storage_.begin() + static_cast<std::ptrdiff_t>(position_),
                   [](char byte) { return static_cast<std::uint8_t>(byte); });
    position_ += length;
}

void BoundedByteWriter::write_bytes(std::span<const std::uint8_t> value) {
    require(value.size());
    std::copy(value.begin(), value.end(), storage_.begin() + static_cast<std::ptrdiff_t>(position_));
    position_ += value.size();
}

std::size_t BoundedByteWriter::position() const noexcept {
    return position_;
}

std::size_t BoundedByteWriter::capacity() const noexcept {
    return storage_.size();
}

std::size_t BoundedByteWriter::remaining() const noexcept {
    return storage_.size() - position_;
}

void write_command_prefix(BoundedByteWriter& writer, int command_type) {
    if (command_type < 0 || command_type > 0xFF) {
        throw std::out_of_range("command type is outside the native byte range");
    }
    writer.reset();
    writer.write_u16(0);
    writer.write_u16(0);
    writer.write_u8(static_cast<std::uint8_t>(command_type));
}

}
