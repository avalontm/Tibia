#pragma once

#include "client/client_communication_preferences.hpp"
#include "network/message_dispatcher.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace tibia::client {

[[nodiscard]] bool promote_vip_sort_method(
    std::array<std::int32_t, 3>& sort_methods,
    std::int32_t selected_method) noexcept;

[[nodiscard]] std::vector<std::size_t> make_vip_contact_order(
    std::span<const network::State6BuddyEntry> contacts,
    std::span<const BuddyPreference> buddy_preferences,
    const std::array<std::int32_t, 3>& sort_methods,
    bool hide_offline);

}
