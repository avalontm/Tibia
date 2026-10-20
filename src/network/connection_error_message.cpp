#include "network/connection_error_message.hpp"

#include <stdexcept>
#include <string>

namespace tibia::network {
namespace {

struct ErrorDescription {
    int code;
    const char* text;
    bool servers_offline;
};

constexpr ErrorDescription descriptions[]{
    {0x2726, "Invalid server address.", false},
    {0x2738, "Connection error.", false},
    {0x2741, "Invalid server address.", false},
    {0x2742, "Network is unreachable.", false},
    {0x2743, "Network is unreachable.", false},
    {0x274C, "Connection timeout.", false},
    {0x274D, "Connection refused.", true},
    {0x2751, "Server is unreachable.", false},
    {0x2AF9, "Server name does not exist.", false},
    {0x2AFA, "Could not resolve server name.", false},
    {0x2AFC, "Error while resolving the server name.", false},
    {0x30D41, "Login server is overloaded. Please try again later.", false},
    {0x30D42, "Disconnected from server.", false}
};

ErrorDescription describe_error(int error_code) {
    for (const auto& description : descriptions) {
        if (description.code == error_code) {
            return description;
        }
    }
    return {error_code, nullptr, false};
}

}

std::string format_connection_error(
    int error_code,
    ConnectionTarget target,
    bool test_server) {
    if (!test_server && target != ConnectionTarget::login_server &&
        target != ConnectionTarget::game_server) {
        throw std::invalid_argument("unsupported connection target");
    }

    const auto description = describe_error(error_code);
    const auto detail = description.text != nullptr
        ? std::string(description.text)
        : std::to_string(error_code);

    std::string result;
    if (test_server) {
        result = "Cannot connect to the test server.\n\nError: " + detail;
        if (description.servers_offline) {
            result += "\n\nThe test server is offline. Check www.tibia.com";
        }
        result += "\nfor more information on any current test.";
        result += "\nIf you don't want to use the test server go to\nthe network option and uncheck \"Test Server Login\".";
    } else if (target == ConnectionTarget::login_server) {
        result = "Cannot connect to a login server.\n\nError: " + detail;
        if (description.servers_offline) {
            result += "\n\nAll login server are offline. Check www.tibia.com";
        }
        result += "\nfor more information.";
    } else {
        result = "Cannot connect to the game server.\n\nError: " + detail;
        if (description.servers_offline) {
            result += "\n\nThe game server is offline. Check www.tibia.com";
        }
        result += "\nfor more information.";
    }

    result += "\n\nFor more information take a look at the FAQs in the \nguide section at www.tibia.com.";
    return result;
}

}
