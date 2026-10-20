#include "objects/object_property_table.hpp"

#include "objects/entry_attribute_flags.hpp"

namespace tibia::objects {

ObjectPropertyTable::ObjectPropertyTable(
    int first_type,
    std::span<const std::uint8_t> records)
    : entries_(first_type, records) {}

std::span<const std::uint8_t> ObjectPropertyTable::entry(int type) const {
    return entries_.at(type);
}

bool ObjectPropertyTable::has_flag(int type, std::size_t flag_index) const {
    if (type == 99) {
        return flag_index == 13;
    }
    const auto record = entry(type);
    const auto flags = record.subspan(0x24, 4);
    return EntryAttributeFlags(flags).contains(flag_index);
}

}
