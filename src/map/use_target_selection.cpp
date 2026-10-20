#include "map/use_target_selection.hpp"

#include "resources/tibia_dat.hpp"

#include <array>

namespace tibia::map {
namespace {

std::optional<bool> has_native_property(const Object& object,
    const resources::TibiaDatFile& dat, std::uint8_t property) {
    if (object.type == 0x63) return property == 13;
    const auto* item = dat.find_item(object.type);
    if (item == nullptr) return std::nullopt;
    return item->has_native_property(property);
}

}

std::optional<std::size_t> select_native_use_target_stack_position(
    const std::vector<const Object*>& objects, const resources::TibiaDatFile& dat) {
    if (objects.empty()) return std::nullopt;

    std::size_t stack_position{};
    while (stack_position + 1 < objects.size()) {
        const auto& object = *objects[stack_position];
        const auto is_usable = has_native_property(object, dat, 6);
        if (!is_usable) return std::nullopt;
        if (*is_usable) return stack_position;

        constexpr std::array<std::uint8_t, 4> blocking_properties{0, 1, 2, 3};
        bool skip_object = false;
        for (const auto property : blocking_properties) {
            const auto has_property = has_native_property(object, dat, property);
            if (!has_property) return std::nullopt;
            if (*has_property) {
                skip_object = true;
                break;
            }
        }
        if (!skip_object) break;
        ++stack_position;
    }

    if (stack_position > 0) {
        const auto is_hangable = has_native_property(
            *objects[stack_position - 1], dat, 12);
        if (!is_hangable) return std::nullopt;
        if (*is_hangable) --stack_position;
    }
    return stack_position;
}

}
