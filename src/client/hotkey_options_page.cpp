#include "client/hotkey_options_page.hpp"

#include <stdexcept>

namespace tibia::client {

std::string hotkey_object_action(std::int32_t mode) {
    switch (mode) {
    case 1: return "(use object on target)";
    case 2: return "(use object on yourself)";
    case 0: return "(use object with crosshairs)";
    case 3: return "(use object)";
    default: return "(use object)";
    }
}

std::string format_hotkey_entry(std::size_t index,
    const HotkeyPreference& preference) {
    if (index >= hotkey_count)
        throw std::out_of_range("Hotkey index is outside the supported range");
    std::string key;
    if (index < 12) key = "F" + std::to_string(index + 1);
    else if (index < 24) key = "Shift+F" + std::to_string(index - 11);
    else key = "Control+F" + std::to_string(index - 23);
    const auto action = preference.data[0] != 0
        ? hotkey_object_action(preference.data[2]) : preference.text;
    return key + ": " + action;
}

void clear_hotkey(HotkeyPreference& preference) noexcept {
    preference = {};
}

void set_hotkey_object_mode(HotkeyPreference& preference, std::int32_t mode) noexcept {
    preference.data[2] = mode;
}

}
