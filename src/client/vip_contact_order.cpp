#include "client/vip_contact_order.hpp"

#include <algorithm>

namespace tibia::client {
namespace {

std::int32_t buddy_type(std::uint32_t id,
    std::span<const BuddyPreference> preferences) {
    const auto preference = std::find_if(preferences.begin(), preferences.end(),
        [id](const BuddyPreference& entry) { return entry.id == id; });
    return preference == preferences.end() ? 0 : preference->type;
}

bool should_swap(const network::State6BuddyEntry& left,
    const network::State6BuddyEntry& right,
    std::int32_t left_type,
    std::int32_t right_type,
    const std::array<std::int32_t, 3>& sort_methods) {
    for (const auto method : sort_methods) {
        if (method == 0) {
            if (left.name != right.name)
                return std::lexicographical_compare(right.name.begin(), right.name.end(),
                    left.name.begin(), left.name.end(), [](unsigned char a, unsigned char b) {
                        return a < b;
                    });
        } else if (method == 1) {
            if (left_type != right_type) return left_type > right_type;
        } else if (method == 2 && left.online != right.online) {
            return !left.online;
        }
    }
    return false;
}

}

bool promote_vip_sort_method(std::array<std::int32_t, 3>& sort_methods,
    std::int32_t selected_method) noexcept {
    if (selected_method == sort_methods[0]) return false;
    if (selected_method == sort_methods[2]) sort_methods[2] = sort_methods[1];
    sort_methods[1] = sort_methods[0];
    sort_methods[0] = selected_method;
    return true;
}

std::vector<std::size_t> make_vip_contact_order(
    std::span<const network::State6BuddyEntry> contacts,
    std::span<const BuddyPreference> buddy_preferences,
    const std::array<std::int32_t, 3>& sort_methods,
    bool hide_offline) {
    std::vector<std::size_t> order;
    order.reserve(contacts.size());
    for (std::size_t index = 0; index < contacts.size(); ++index)
        if (!hide_offline || contacts[index].online) order.push_back(index);
    std::vector<std::int32_t> types;
    types.reserve(contacts.size());
    for (const auto& contact : contacts)
        types.push_back(buddy_type(contact.id, buddy_preferences));
    for (std::size_t first = 0; first + 1 < order.size(); ++first) {
        for (std::size_t second = first + 1; second < order.size(); ++second) {
            if (should_swap(contacts[order[first]], contacts[order[second]],
                    types[order[first]], types[order[second]], sort_methods))
                std::swap(order[first], order[second]);
        }
    }
    return order;
}

}
