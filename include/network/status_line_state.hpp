#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace tibia::network {

struct StatusLineSnapshot {
    std::int32_t value{};
    bool has_text{};
    std::string text;
};

class StatusLineState {
public:
    [[nodiscard]] bool consume(std::string_view message);
    [[nodiscard]] std::int32_t value() const;
    [[nodiscard]] StatusLineSnapshot snapshot() const;

private:
    mutable std::mutex mutex_;
    std::int32_t value_{};
    bool has_text_{};
    std::string text_;
};

}
