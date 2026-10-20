#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "objects/entry_collection.hpp"

namespace tibia::objects {

class ObjectPropertyTable {
public:
    static constexpr std::size_t record_size = EntryCollectionView::record_size;

    ObjectPropertyTable(int first_type, std::span<const std::uint8_t> records);

    [[nodiscard]] std::span<const std::uint8_t> entry(int type) const;
    [[nodiscard]] bool has_flag(int type, std::size_t flag_index) const;

private:
    EntryCollectionView entries_;
};

}
