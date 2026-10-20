#include "network/server_catalog.hpp"

#include <cstring>

namespace tibia::network {

ServerCatalog initialize_legacy_server_catalog(std::int32_t random_value) {
    ServerCatalog catalog;
    constexpr std::array<const char*, legacy_server_count> addresses{{
        "server.tibia.com",
        "server2.tibia.com",
        "tibia1.cipsoft.com",
        "tibia2.cipsoft.com"
    }};
    for (std::size_t i = 0; i < catalog.endpoints.size(); ++i) {
        auto& endpoint = catalog.endpoints[i];
        std::memcpy(endpoint.address.data(), addresses[i], std::strlen(addresses[i]) + 1);
        endpoint.port = 7171;
        endpoint.enabled = 1;
    }
    catalog.selected_index = static_cast<std::uint32_t>(random_value) & 3u;
    catalog.initialized = true;
    return catalog;
}

ServerEndpoint* select_next_enabled_server(ServerCatalog& catalog) noexcept {
    if (!catalog.initialized || catalog.selected_index >= catalog.endpoints.size())
        return nullptr;
    for (std::size_t checked = 0; checked < catalog.endpoints.size(); ++checked) {
        auto& endpoint = catalog.endpoints[catalog.selected_index];
        if (endpoint.enabled != 0) return &endpoint;
        catalog.selected_index = (catalog.selected_index + 1U) % catalog.endpoints.size();
    }
    return nullptr;
}

void disable_current_server(ServerCatalog& catalog, std::uint32_t retry_delay) noexcept {
    if (!catalog.initialized || catalog.selected_index >= catalog.endpoints.size()) return;
    auto& endpoint = catalog.endpoints[catalog.selected_index];
    endpoint.enabled = 0;
    endpoint.retry_delay = retry_delay;
}

std::optional<std::uint32_t> next_server_retry_delay(ServerCatalog& catalog) noexcept {
    for (const auto& endpoint : catalog.endpoints)
        if (endpoint.retry_delay == legacy_server_retry_sentinel)
            return legacy_server_retry_sentinel;
    const auto* endpoint = select_next_enabled_server(catalog);
    if (!endpoint) return std::nullopt;
    return endpoint->retry_delay;
}

void enable_legacy_servers(ServerCatalog& catalog) noexcept {
    for (auto& endpoint : catalog.endpoints) endpoint.enabled = 1;
    catalog.initialized = true;
}

}
