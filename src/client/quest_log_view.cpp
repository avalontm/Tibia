#include "client/quest_log_view.hpp"

#include <utility>

namespace tibia::client {

std::vector<QuestLogEntry> make_quest_log_entries(
    const network::State6QuestState& quests) {
    std::vector<QuestLogEntry> entries;
    entries.reserve(quests.lines().size());
    for (const auto& line : quests.lines()) {
        if (line.name.empty()) continue;
        auto text = line.name;
        if (line.completed) text += " (completed)";
        entries.push_back({line.id, std::move(text), line.completed});
    }
    return entries;
}

}
