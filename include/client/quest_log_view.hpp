#pragma once

#include "network/message_dispatcher.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tibia::client {

struct QuestLogEntry {
    std::uint16_t id{};
    std::string text;
    bool completed{};
};

[[nodiscard]] std::vector<QuestLogEntry> make_quest_log_entries(
    const network::State6QuestState& quests);

}
