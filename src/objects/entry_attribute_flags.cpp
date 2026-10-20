#include "objects/entry_attribute_flags.hpp"

#include <stdexcept>

namespace tibia::objects {

EntryAttributeFlags::EntryAttributeFlags(std::span<const std::uint8_t> bytes) noexcept
    : bytes_(bytes) {}

bool EntryAttributeFlags::contains(std::size_t index) const {
    const auto byte_index = index / 8U;
    if (byte_index >= bytes_.size()) {
        throw std::out_of_range("entry attribute index exceeds available flag bytes");
    }
    const auto bit_mask = static_cast<std::uint8_t>(1U << (index % 8U));
    return (bytes_[byte_index] & bit_mask) != 0;
}

}
