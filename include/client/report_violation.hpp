#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tibia::client {

struct ReportReason {
    std::uint8_t id{};
    std::string_view label;
};

[[nodiscard]] std::vector<ReportReason> available_report_reasons(
    const std::array<std::uint8_t, 32>& right_values);

}
