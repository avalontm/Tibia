#include "objects/entry_collection.hpp"

#include <limits>
#include <stdexcept>

namespace tibia::objects {

EntryCollectionView::EntryCollectionView(
    int first_index,
    std::span<const std::uint8_t> records)
    : first_index_(first_index), records_(records) {
    if (records_.empty() || records_.size() % record_size != 0) {
        throw std::invalid_argument("entry collection requires complete records");
    }
    if (size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("entry collection count exceeds the native range");
    }
    const auto last = static_cast<std::int64_t>(first_index_) +
                      static_cast<std::int64_t>(size()) - 1;
    if (last > std::numeric_limits<int>::max()) {
        throw std::length_error("entry collection index range overflows");
    }
}

std::span<const std::uint8_t> EntryCollectionView::at(int index) const {
    const auto offset = static_cast<std::int64_t>(index) - first_index_;
    if (offset < 0 || static_cast<std::uint64_t>(offset) >= size()) {
        throw std::out_of_range("entry index is outside the collection");
    }
    return records_.subspan(static_cast<std::size_t>(offset) * record_size, record_size);
}

std::uint32_t EntryCollectionView::read_u32(int index, std::size_t field_offset) const {
    if (field_offset > record_size - sizeof(std::uint32_t)) {
        throw std::out_of_range("entry field exceeds the record");
    }
    const auto record = at(index);
    return static_cast<std::uint32_t>(record[field_offset]) |
           (static_cast<std::uint32_t>(record[field_offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(record[field_offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(record[field_offset + 3]) << 24U);
}

int EntryCollectionView::first_index() const noexcept {
    return first_index_;
}

int EntryCollectionView::last_index() const noexcept {
    const auto last = static_cast<std::int64_t>(first_index_) +
                      static_cast<std::int64_t>(size()) - 1;
    return static_cast<int>(last);
}

std::size_t EntryCollectionView::size() const noexcept {
    return records_.size() / record_size;
}

}
