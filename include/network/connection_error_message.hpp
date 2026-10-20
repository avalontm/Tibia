#pragma once

#include <string>

namespace tibia::network {

enum class ConnectionTarget {
    login_server,
    game_server
};

[[nodiscard]] std::string format_connection_error(
    int error_code,
    ConnectionTarget target,
    bool test_server);

}
