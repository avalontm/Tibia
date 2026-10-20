#include "client/report_violation.hpp"

namespace tibia::client {
namespace {

constexpr std::array<std::string_view, 32> reason_labels{
    "1a) Offensive name",
    "1b) Name containing part of sentence",
    "1b) Name with nonsensical letter combination",
    "1b) Invalid name format",
    "1c) Name not describing person",
    "1c) Name of celebrity",
    "1c) Name referring to country",
    "1c) Name to fake player identity",
    "1c) Name to fake official position",
    "2a) Offensive statement",
    "2b) Spamming",
    "2c) Advertisement not related to game",
    "2c) Real money advertisement",
    "2d) Non-English public statement",
    "2d) Off-topic public statement",
    "2e) Inciting rule violation",
    "3a) Bug abuse",
    "3b) Game weakness abuse",
    "3c) Macro use",
    "3d) Using unofficial software to play",
    "3e) Hacking",
    "3f) Multi-clienting",
    "3g) Account trading",
    "3g) Account sharing",
    "4a) Threatening gamemaster",
    "4b) Pretending to have official position",
    "4b) Pretending to have influence on gamemaster",
    "4c) False report to gamemaster",
    "Excessive unjustified player killing",
    "Destructive behaviour",
    "Spoiling auction",
    "Invalid payment"};

}

std::vector<ReportReason> available_report_reasons(
    const std::array<std::uint8_t, 32>& right_values) {
    std::vector<ReportReason> reasons;
    reasons.reserve(reason_labels.size());
    for (std::size_t index = 0; index < reason_labels.size(); ++index) {
        if (right_values[index] == 0) continue;
        reasons.push_back({static_cast<std::uint8_t>(index), reason_labels[index]});
    }
    return reasons;
}

}
