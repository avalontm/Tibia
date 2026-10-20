#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tibia::network {

inline constexpr std::size_t legacy_server_count = 4;
inline constexpr std::size_t legacy_server_address_capacity = 100;
inline constexpr std::uint32_t legacy_server_retry_sentinel = 200001;

struct ServerEndpoint {
    std::array<char, legacy_server_address_capacity> address{};
    std::uint32_t port{};
    std::uint8_t enabled{};
    std::array<std::uint8_t, 3> unmodeled_bytes{};
    std::uint32_t retry_delay{};
};

static_assert(sizeof(ServerEndpoint) == 0x70);
static_assert(offsetof(ServerEndpoint, port) == 0x64);
static_assert(offsetof(ServerEndpoint, enabled) == 0x68);
static_assert(offsetof(ServerEndpoint, retry_delay) == 0x6c);

struct ServerCatalog {
    std::array<ServerEndpoint, legacy_server_count> endpoints{};
    std::uint32_t selected_index{};
    bool initialized{};
};

[[nodiscard]] ServerCatalog initialize_legacy_server_catalog(std::int32_t random_value);
[[nodiscard]] ServerEndpoint* select_next_enabled_server(ServerCatalog& catalog) noexcept;
void disable_current_server(ServerCatalog& catalog, std::uint32_t retry_delay) noexcept;
[[nodiscard]] std::optional<std::uint32_t> next_server_retry_delay(ServerCatalog& catalog) noexcept;
void enable_legacy_servers(ServerCatalog& catalog) noexcept;

}
