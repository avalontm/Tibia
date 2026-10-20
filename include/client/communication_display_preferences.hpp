#pragma once

namespace tibia::client {

struct CommunicationDisplayPreferences {
    bool timestamps_enabled{};
    bool levels_enabled{};
    bool private_messages_enabled{true};
};

}
