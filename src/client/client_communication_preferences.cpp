#include "client/client_communication_preferences.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tibia::client {
namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (first >= last) return {};
    return {first, last};
}

bool parse_yes_no(const std::string& value, std::size_t line_number) {
    if (value == "yes") return true;
    if (value == "no") return false;
    throw std::runtime_error("invalid Tibia.cfg yes/no value at line " +
        std::to_string(line_number));
}

std::vector<std::string> parse_name_list(const std::string& value,
    std::size_t line_number) {
    std::size_t position = 0;
    const auto skip_space = [&] {
        while (position < value.size() &&
            std::isspace(static_cast<unsigned char>(value[position])) != 0) ++position;
    };
    skip_space();
    if (position == value.size() || value[position++] != '{')
        throw std::runtime_error("invalid Tibia.cfg list at line " +
            std::to_string(line_number));

    std::vector<std::string> names;
    skip_space();
    while (position < value.size() && value[position] != '}') {
        if (value[position++] != '"')
            throw std::runtime_error("expected quoted Tibia.cfg list item at line " +
                std::to_string(line_number));
        std::string name;
        bool closed = false;
        while (position < value.size()) {
            const char character = value[position++];
            if (character == '"') {
                closed = true;
                break;
            }
            if (character == '\\') {
                if (position == value.size()) break;
                const char escaped = value[position++];
                if (escaped == 'n') name.push_back('\n');
                else if (escaped == '"' || escaped == '\\') name.push_back(escaped);
                else throw std::runtime_error("invalid Tibia.cfg escape at line " +
                    std::to_string(line_number));
            } else {
                name.push_back(character);
            }
        }
        if (!closed)
            throw std::runtime_error("unterminated Tibia.cfg list item at line " +
                std::to_string(line_number));
        names.push_back(std::move(name));
        skip_space();
        if (position < value.size() && value[position] == ',') {
            ++position;
            skip_space();
        } else if (position < value.size() && value[position] != '}') {
            throw std::runtime_error("expected comma in Tibia.cfg list at line " +
                std::to_string(line_number));
        }
    }
    if (position == value.size())
        throw std::runtime_error("unterminated Tibia.cfg list at line " +
            std::to_string(line_number));
    ++position;
    skip_space();
    if (position != value.size())
        throw std::runtime_error("trailing data after Tibia.cfg list at line " +
            std::to_string(line_number));
    return names;
}

std::vector<std::int32_t> parse_integer_list(const std::string& value,
    std::size_t line_number) {
    std::size_t position = 0;
    const auto skip_space = [&] {
        while (position < value.size() &&
            std::isspace(static_cast<unsigned char>(value[position])) != 0) ++position;
    };
    skip_space();
    if (position == value.size() || value[position++] != '{')
        throw std::runtime_error("invalid Tibia.cfg list at line " +
            std::to_string(line_number));

    std::vector<std::int32_t> values;
    skip_space();
    while (position < value.size() && value[position] != '}') {
        std::int32_t parsed_value{};
        const auto parsed = std::from_chars(value.data() + position,
            value.data() + value.size(), parsed_value);
        if (parsed.ec != std::errc{} || parsed.ptr == value.data() + position ||
            parsed_value < 0)
            throw std::runtime_error("invalid Tibia.cfg list integer at line " +
                std::to_string(line_number));
        position = static_cast<std::size_t>(parsed.ptr - value.data());
        values.push_back(parsed_value);
        skip_space();
        if (position < value.size() && value[position] == ',') {
            ++position;
            skip_space();
        } else if (position < value.size() && value[position] != '}') {
            throw std::runtime_error("expected comma in Tibia.cfg list at line " +
                std::to_string(line_number));
        }
    }
    if (position == value.size())
        throw std::runtime_error("unterminated Tibia.cfg list at line " +
            std::to_string(line_number));
    ++position;
    skip_space();
    if (position != value.size())
        throw std::runtime_error("trailing data after Tibia.cfg list at line " +
            std::to_string(line_number));
    return values;
}

BuddyPreference parse_buddy_preference(const std::string& value,
    std::size_t line_number) {
    std::size_t position = 0;
    const auto skip_space = [&] {
        while (position < value.size() &&
            std::isspace(static_cast<unsigned char>(value[position])) != 0) ++position;
    };
    const auto parse_integer_field = [&]<typename Integer>(Integer& result) {
        skip_space();
        const auto parsed = std::from_chars(value.data() + position,
            value.data() + value.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr == value.data() + position)
            throw std::runtime_error("invalid Tibia.cfg buddy at line " +
                std::to_string(line_number));
        position = static_cast<std::size_t>(parsed.ptr - value.data());
    };
    skip_space();
    if (position == value.size() || value[position++] != '(')
        throw std::runtime_error("invalid Tibia.cfg buddy at line " +
            std::to_string(line_number));
    BuddyPreference buddy;
    parse_integer_field(buddy.id);
    skip_space();
    if (position == value.size() || value[position++] != ',')
        throw std::runtime_error("invalid Tibia.cfg buddy at line " +
            std::to_string(line_number));
    parse_integer_field(buddy.type);
    skip_space();
    if (position == value.size() || value[position++] != ',')
        throw std::runtime_error("invalid Tibia.cfg buddy at line " +
            std::to_string(line_number));
    skip_space();
    if (position == value.size() || value[position++] != '\"')
        throw std::runtime_error("invalid Tibia.cfg buddy name at line " +
            std::to_string(line_number));
    bool closed = false;
    while (position < value.size()) {
        const char character = value[position++];
        if (character == '\"') {
            closed = true;
            break;
        }
        if (character == '\\') {
            if (position == value.size()) break;
            const char escaped = value[position++];
            if (escaped == 'n') buddy.name.push_back('\n');
            else if (escaped == '\"' || escaped == '\\') buddy.name.push_back(escaped);
            else throw std::runtime_error("invalid Tibia.cfg buddy escape at line " +
                std::to_string(line_number));
        } else {
            buddy.name.push_back(character);
        }
    }
    if (!closed)
        throw std::runtime_error("unterminated Tibia.cfg buddy name at line " +
            std::to_string(line_number));
    skip_space();
    if (position == value.size() || value[position++] != ',')
        throw std::runtime_error("invalid Tibia.cfg buddy at line " +
            std::to_string(line_number));
    skip_space();
    if (value.compare(position, 3, "yes") == 0) {
        buddy.enabled = true;
        position += 3;
    } else if (value.compare(position, 2, "no") == 0) {
        position += 2;
    } else {
        throw std::runtime_error("invalid Tibia.cfg buddy status at line " +
            std::to_string(line_number));
    }
    skip_space();
    if (position == value.size() || value[position++] != ')')
        throw std::runtime_error("invalid Tibia.cfg buddy at line " +
            std::to_string(line_number));
    skip_space();
    if (position != value.size())
        throw std::runtime_error("trailing data after Tibia.cfg buddy at line " +
            std::to_string(line_number));
    return buddy;
}

bool has_list_closing_brace(std::string_view value) {
    bool quoted = false;
    bool escaped = false;
    for (const char character : value) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (quoted && character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') quoted = !quoted;
        else if (!quoted && character == '}') return true;
    }
    return false;
}

std::string serialize_name_list(const std::vector<std::string>& names) {
    std::string value = "{";
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index != 0) value += ", ";
        value.push_back('"');
        for (const char character : names[index]) {
            if (character == '"' || character == '\\') value.push_back('\\');
            if (character == '\n') value += "\\n";
            else value.push_back(character);
        }
        value.push_back('"');
    }
    value.push_back('}');
    return value;
}

std::string serialize_integer_list(const std::vector<std::int32_t>& values) {
    std::string value = "{";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) value += ", ";
        value += std::to_string(values[index]);
    }
    value.push_back('}');
    return value;
}

std::string serialize_buddy_preference(const BuddyPreference& buddy) {
    std::string value = "Buddy = (" + std::to_string(buddy.id) + "," +
        std::to_string(buddy.type) + ",\"";
    for (const char character : buddy.name) {
        if (character == '\"' || character == '\\') value.push_back('\\');
        if (character == '\n') value += "\\n";
        else value.push_back(character);
    }
    value += buddy.enabled ? "\",yes)" : "\",no)";
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::array<std::int32_t, 3> parse_display_mode(const std::string& value,
    std::size_t line_number) {
    std::array<std::int32_t, 3> mode{};
    std::size_t position = 0;
    const auto skip_space = [&] {
        while (position < value.size() &&
            std::isspace(static_cast<unsigned char>(value[position])) != 0) ++position;
    };
    skip_space();
    if (position == value.size() || value[position++] != '(')
        throw std::runtime_error("invalid Tibia.cfg display mode at line " +
            std::to_string(line_number));
    for (std::size_t index = 0; index < mode.size(); ++index) {
        skip_space();
        const auto parsed = std::from_chars(value.data() + position,
            value.data() + value.size(), mode[index]);
        if (parsed.ec != std::errc{} || parsed.ptr == value.data() + position)
            throw std::runtime_error("invalid Tibia.cfg display mode at line " +
                std::to_string(line_number));
        position = static_cast<std::size_t>(parsed.ptr - value.data());
        skip_space();
        const char delimiter = index + 1 == mode.size() ? ')' : ',';
        if (position == value.size() || value[position++] != delimiter)
            throw std::runtime_error("invalid Tibia.cfg display mode at line " +
                std::to_string(line_number));
    }
    skip_space();
    if (position != value.size())
        throw std::runtime_error("invalid Tibia.cfg display mode at line " +
            std::to_string(line_number));
    return mode;
}

std::int32_t parse_integer(const std::string& value, std::size_t line_number) {
    std::int32_t result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::runtime_error("invalid Tibia.cfg integer at line " +
            std::to_string(line_number));
    return result;
}

std::array<std::int32_t, 2> parse_integer_pair(const std::string& value,
    std::size_t line_number) {
    std::array<std::int32_t, 2> result{};
    std::size_t position = 0;
    const auto skip_space = [&] {
        while (position < value.size() &&
            std::isspace(static_cast<unsigned char>(value[position])) != 0) ++position;
    };
    skip_space();
    if (position == value.size() || value[position++] != '(')
        throw std::runtime_error("invalid Tibia.cfg content window at line " +
            std::to_string(line_number));
    for (std::size_t index = 0; index < result.size(); ++index) {
        skip_space();
        const auto parsed = std::from_chars(value.data() + position,
            value.data() + value.size(), result[index]);
        if (parsed.ec != std::errc{} || parsed.ptr == value.data() + position)
            throw std::runtime_error("invalid Tibia.cfg content window at line " +
                std::to_string(line_number));
        position = static_cast<std::size_t>(parsed.ptr - value.data());
        skip_space();
        const char delimiter = index + 1 == result.size() ? ')' : ',';
        if (position == value.size() || value[position++] != delimiter)
            throw std::runtime_error("invalid Tibia.cfg content window at line " +
                std::to_string(line_number));
    }
    skip_space();
    if (position != value.size())
        throw std::runtime_error("invalid Tibia.cfg content window at line " +
            std::to_string(line_number));
    return result;
}

std::string serialize_content_window(const ContentWindowPreference& window) {
    return "ContentWindow = (" + std::to_string(window.type) + "," +
        std::to_string(window.value) + ")";
}

}

ClientCommunicationPreferences read_communication_preferences(
    const std::filesystem::path& path) {
    ClientCommunicationPreferences preferences;
    std::ifstream input(path);
    if (!input) return preferences;

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto split = line.find('=');
        if (split == std::string::npos) continue;
        const auto key = lowercase(trim(line.substr(0, split)));
        auto value = trim(line.substr(split + 1));
        if (key == "consoleheight") {
            std::int32_t console_height{};
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                console_height);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                throw std::runtime_error("invalid Tibia.cfg integer at line " +
                    std::to_string(line_number));
            preferences.console_height = console_height;
        } else if (key == "classiccontrol")
            preferences.classic_control = parse_yes_no(value, line_number);
        else if (key == "autochaseoff")
            preferences.auto_chase_off = parse_yes_no(value, line_number);
        else if (key == "attackmode" || key == "chasemode") {
            auto* destination = key == "attackmode" ? &preferences.attack_mode :
                &preferences.chase_mode;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                *destination);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                throw std::runtime_error("invalid Tibia.cfg integer at line " +
                    std::to_string(line_number));
            if (key == "chasemode" && preferences.chase_mode == 2)
                preferences.chase_mode = 0;
        } else if (key == "securemode") {
            std::int32_t secure_mode{};
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                secure_mode);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                throw std::runtime_error("invalid Tibia.cfg integer at line " +
                    std::to_string(line_number));
            preferences.secure_mode = secure_mode == 1;
        }
        else if (key == "shownewhints")
            preferences.show_new_hints = parse_yes_no(value, line_number);
        else if (key == "opendialogs") {
            const auto dialog = parse_integer(value, line_number);
            if (std::find(preferences.open_dialogs.begin(), preferences.open_dialogs.end(),
                    dialog) == preferences.open_dialogs.end())
                preferences.open_dialogs.push_back(dialog);
        } else if (key == "contentwindow") {
            const auto fields = parse_integer_pair(value, line_number);
            preferences.content_windows.push_back({fields[0], fields[1]});
        }
        else if (key == "buddysortmethods") {
            preferences.buddy_sort_methods = parse_display_mode(value, line_number);
        } else if (key == "buddyhideoffline")
            preferences.buddy_hide_offline = parse_yes_no(value, line_number);
        else if (key == "buddy")
            preferences.buddies.push_back(parse_buddy_preference(value, line_number));
        else if (key == "levelbar")
            preferences.skills_bar_visibility.level = parse_yes_no(value, line_number);
        else if (key == "staminabar")
            preferences.skills_bar_visibility.stamina = parse_yes_no(value, line_number);
        else if (key == "maglevelbar")
            preferences.skills_bar_visibility.magic_level = parse_yes_no(value, line_number);
        else if (key == "skillbars") {
            while (!has_list_closing_brace(value) && std::getline(input, line)) {
                ++line_number;
                value.push_back('\n');
                value += line;
            }
            preferences.skills_bar_visibility.skills.fill(false);
            for (const auto index : parse_integer_list(value, line_number)) {
                if (index >= static_cast<std::int32_t>(
                        preferences.skills_bar_visibility.skills.size()))
                    throw std::runtime_error("invalid Tibia.cfg skill index at line " +
                        std::to_string(line_number));
                preferences.skills_bar_visibility.skills[static_cast<std::size_t>(index)] = true;
            }
        }
        else if (key == "textualeffects")
            preferences.textual_effects = parse_yes_no(value, line_number);
        else if (key == "lighteffects")
            preferences.light_effects = parse_yes_no(value, line_number);
        else if (key == "nostretching")
            preferences.no_stretching = parse_yes_no(value, line_number);
        else if (key == "windowsmousepointer")
            preferences.windows_mouse_pointer = parse_yes_no(value, line_number);
        else if (key == "basicbrightness" || key == "confineframerate" ||
            key == "engine" || key == "automapzoom") {
            auto* destination = key == "basicbrightness" ? &preferences.basic_brightness :
                key == "confineframerate" ? &preferences.confine_framerate :
                key == "automapzoom" ? &preferences.automap_zoom : &preferences.engine;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                *destination);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                throw std::runtime_error("invalid Tibia.cfg integer at line " +
                    std::to_string(line_number));
        }
        else if (key == "fullscreenmode") {
            preferences.fullscreen_mode = parse_display_mode(value, line_number);
            preferences.fullscreen_mode_configured = true;
        }
        else if (key == "testserver")
            preferences.test_server_login = parse_yes_no(value, line_number);
        else if (key == "creatureinfo") {
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                preferences.creature_info);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                throw std::runtime_error("invalid Tibia.cfg integer at line " +
                    std::to_string(line_number));
        } else if (key == "eventmessages")
            preferences.routing.mode_13_and_14_enabled = parse_yes_no(value, line_number);
        else if (key == "statusmessages")
            preferences.routing.mode_15_enabled = parse_yes_no(value, line_number);
        else if (key == "infomessages")
            preferences.routing.mode_16_enabled = parse_yes_no(value, line_number);
        else if (key == "timestamps")
            preferences.display.timestamps_enabled = parse_yes_no(value, line_number);
        else if (key == "levels")
            preferences.display.levels_enabled = parse_yes_no(value, line_number);
        else if (key == "privatemessages")
            preferences.display.private_messages_enabled = parse_yes_no(value, line_number);
        else if (key == "hotkey") {
            auto parsed = parse_hotkey_preference(value, line_number);
            preferences.hotkeys[parsed.index] = std::move(parsed.preference);
        }
        else if (key == "knownhints") {
            while (!has_list_closing_brace(value) && std::getline(input, line)) {
                ++line_number;
                value.push_back('\n');
                value += line;
            }
            auto values = parse_integer_list(value, line_number);
            preferences.known_hints.insert(preferences.known_hints.end(),
                values.begin(), values.end());
        }
        else if (key == "white" || key == "ignore") {
            while (!has_list_closing_brace(value) && std::getline(input, line)) {
                ++line_number;
                value.push_back('\n');
                value += line;
            }
            auto names = parse_name_list(value, line_number);
            if (key == "white") preferences.white_list = std::move(names);
            else preferences.ignore_list = std::move(names);
        }
    }
    if (input.bad()) throw std::runtime_error("cannot read Tibia.cfg");
    return preferences;
}

void write_communication_preferences(const std::filesystem::path& path,
    const ClientCommunicationPreferences& preferences) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open Tibia.cfg for updating");

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) lines.push_back(std::move(line));
    if (input.bad()) throw std::runtime_error("cannot read Tibia.cfg");

    const auto event_value = std::string("EventMessages = ") +
        (preferences.routing.mode_13_and_14_enabled ? "yes" : "no");
    const auto status_value = std::string("StatusMessages = ") +
        (preferences.routing.mode_15_enabled ? "yes" : "no");
    const auto info_value = std::string("InfoMessages = ") +
        (preferences.routing.mode_16_enabled ? "yes" : "no");
    const auto timestamps_value = std::string("TimeStamps = ") +
        (preferences.display.timestamps_enabled ? "yes" : "no");
    const auto levels_value = std::string("Levels = ") +
        (preferences.display.levels_enabled ? "yes" : "no");
    const auto private_messages_value = std::string("PrivateMessages = ") +
        (preferences.display.private_messages_enabled ? "yes" : "no");
    const auto classic_control_value = std::string("ClassicControl = ") +
        (preferences.classic_control ? "yes" : "no");
    const auto auto_chase_off_value = std::string("AutoChaseOff = ") +
        (preferences.auto_chase_off ? "yes" : "no");
    const auto attack_mode_value = std::string("AttackMode = ") +
        std::to_string(preferences.attack_mode);
    const auto chase_mode_value = std::string("ChaseMode = ") +
        std::to_string(preferences.chase_mode);
    const auto secure_mode_value = std::string("SecureMode = ") +
        (preferences.secure_mode ? "1" : "0");
    const auto show_new_hints_value = std::string("ShowNewHints = ") +
        (preferences.show_new_hints ? "yes" : "no");
    const auto level_bar_value = std::string("LevelBar = ") +
        (preferences.skills_bar_visibility.level ? "yes" : "no");
    const auto stamina_bar_value = std::string("StaminaBar = ") +
        (preferences.skills_bar_visibility.stamina ? "yes" : "no");
    const auto magic_level_bar_value = std::string("MagLevelBar = ") +
        (preferences.skills_bar_visibility.magic_level ? "yes" : "no");
    std::vector<std::int32_t> enabled_skill_bars;
    for (std::size_t index = 0;
        index < preferences.skills_bar_visibility.skills.size(); ++index)
        if (preferences.skills_bar_visibility.skills[index])
            enabled_skill_bars.push_back(static_cast<std::int32_t>(index));
    const auto skill_bars_value = std::string("SkillBars = ") +
        serialize_integer_list(enabled_skill_bars);
    const auto creature_info_value = std::string("CreatureInfo = ") +
        std::to_string(preferences.creature_info);
    const auto textual_effects_value = std::string("TextualEffects = ") +
        (preferences.textual_effects ? "yes" : "no");
    const auto test_server_value = std::string("TestServer = ") +
        (preferences.test_server_login ? "yes" : "no");
    const auto fullscreen_mode_value = std::string("FullscreenMode = (") +
        std::to_string(preferences.fullscreen_mode[0]) + "," +
        std::to_string(preferences.fullscreen_mode[1]) + "," +
        std::to_string(preferences.fullscreen_mode[2]) + ")";
    const auto basic_brightness_value = std::string("BasicBrightness = ") +
        std::to_string(preferences.basic_brightness);
    const auto confine_framerate_value = std::string("ConfineFramerate = ") +
        std::to_string(preferences.confine_framerate);
    const auto engine_value = std::string("Engine = ") + std::to_string(preferences.engine);
    const auto automap_zoom_value = std::string("AutomapZoom = ") +
        std::to_string(preferences.automap_zoom);
    const auto buddy_sort_methods_value = std::string("BuddySortmethods = (") +
        std::to_string(preferences.buddy_sort_methods[0]) + "," +
        std::to_string(preferences.buddy_sort_methods[1]) + "," +
        std::to_string(preferences.buddy_sort_methods[2]) + ")";
    const auto buddy_hide_offline_value = std::string("BuddyHideOffline = ") +
        (preferences.buddy_hide_offline ? "yes" : "no");
    const auto light_effects_value = std::string("LightEffects = ") +
        (preferences.light_effects ? "yes" : "no");
    const auto no_stretching_value = std::string("NoStretching = ") +
        (preferences.no_stretching ? "yes" : "no");
    const auto mouse_pointer_value = std::string("WindowsMousepointer = ") +
        (preferences.windows_mouse_pointer ? "yes" : "no");
    const auto white_value = std::string("White = ") + serialize_name_list(preferences.white_list);
    const auto ignore_value = std::string("Ignore = ") + serialize_name_list(preferences.ignore_list);
    const auto known_hints_value = std::string("KnownHints = ") +
        serialize_integer_list(preferences.known_hints);
    bool event_written = false;
    bool status_written = false;
    bool info_written = false;
    bool timestamps_written = false;
    bool levels_written = false;
    bool private_messages_written = false;
    bool classic_control_written = false;
    bool auto_chase_off_written = false;
    bool attack_mode_written = false;
    bool chase_mode_written = false;
    bool secure_mode_written = false;
    bool show_new_hints_written = false;
    bool level_bar_written = false;
    bool stamina_bar_written = false;
    bool magic_level_bar_written = false;
    bool skill_bars_written = false;
    bool creature_info_written = false;
    bool textual_effects_written = false;
    bool test_server_written = false;
    bool fullscreen_mode_written = false;
    bool basic_brightness_written = false;
    bool confine_framerate_written = false;
    bool engine_written = false;
    bool automap_zoom_written = false;
    bool buddy_sort_methods_written = false;
    bool buddy_hide_offline_written = false;
    bool light_effects_written = false;
    bool no_stretching_written = false;
    bool mouse_pointer_written = false;
    bool white_written = false;
    bool ignore_written = false;
    bool known_hints_written = false;
    bool buddies_written = false;
    bool hotkeys_written = false;
    bool open_dialogs_written = false;
    bool content_windows_written = false;
    std::vector<std::string> output;
    output.reserve(lines.size() + 5);
    const auto serialize_open_dialogs = [&] {
        for (const auto dialog : preferences.open_dialogs)
            output.push_back("OpenDialogs = " + std::to_string(dialog));
    };
    const auto serialize_content_windows = [&] {
        for (const auto& window : preferences.content_windows)
            output.push_back(serialize_content_window(window));
    };
    const auto serialize_buddies = [&] {
        for (const auto& buddy : preferences.buddies)
            if (buddy.id != 0) output.push_back(serialize_buddy_preference(buddy));
    };

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto split = lines[index].find('=');
        if (split == std::string::npos) {
            output.push_back(lines[index]);
            continue;
        }
        const auto key = lowercase(trim(lines[index].substr(0, split)));
        if (key == "eventmessages") {
            output.push_back(event_value);
            event_written = true;
        } else if (key == "statusmessages") {
            output.push_back(status_value);
            status_written = true;
        } else if (key == "infomessages") {
            output.push_back(info_value);
            info_written = true;
        } else if (key == "timestamps") {
            output.push_back(timestamps_value);
            timestamps_written = true;
        } else if (key == "levels") {
            output.push_back(levels_value);
            levels_written = true;
        } else if (key == "privatemessages") {
            output.push_back(private_messages_value);
            private_messages_written = true;
        } else if (key == "classiccontrol") {
            output.push_back(classic_control_value);
            classic_control_written = true;
        } else if (key == "autochaseoff") {
            output.push_back(auto_chase_off_value);
            auto_chase_off_written = true;
        } else if (key == "attackmode") {
            output.push_back(attack_mode_value);
            attack_mode_written = true;
        } else if (key == "chasemode") {
            output.push_back(chase_mode_value);
            chase_mode_written = true;
        } else if (key == "securemode") {
            output.push_back(secure_mode_value);
            secure_mode_written = true;
        } else if (key == "shownewhints") {
            output.push_back(show_new_hints_value);
            show_new_hints_written = true;
        } else if (key == "opendialogs") {
            if (!open_dialogs_written) serialize_open_dialogs();
            open_dialogs_written = true;
        } else if (key == "contentwindow") {
            if (!content_windows_written) serialize_content_windows();
            content_windows_written = true;
        } else if (key == "levelbar") {
            output.push_back(level_bar_value);
            level_bar_written = true;
        } else if (key == "staminabar") {
            output.push_back(stamina_bar_value);
            stamina_bar_written = true;
        } else if (key == "maglevelbar") {
            output.push_back(magic_level_bar_value);
            magic_level_bar_written = true;
        } else if (key == "skillbars") {
            auto existing_value = trim(lines[index].substr(split + 1));
            while (!has_list_closing_brace(existing_value) && index + 1 < lines.size())
                existing_value += "\n" + lines[++index];
            if (!skill_bars_written) output.push_back(skill_bars_value);
            skill_bars_written = true;
        } else if (key == "creatureinfo") {
            output.push_back(creature_info_value);
            creature_info_written = true;
        } else if (key == "textualeffects") {
            output.push_back(textual_effects_value);
            textual_effects_written = true;
        } else if (key == "testserver") {
            output.push_back(test_server_value);
            test_server_written = true;
        } else if (key == "fullscreenmode") {
            output.push_back(fullscreen_mode_value);
            fullscreen_mode_written = true;
        } else if (key == "basicbrightness") {
            output.push_back(basic_brightness_value);
            basic_brightness_written = true;
        } else if (key == "confineframerate") {
            output.push_back(confine_framerate_value);
            confine_framerate_written = true;
        } else if (key == "engine") {
            output.push_back(engine_value);
            engine_written = true;
        } else if (key == "automapzoom") {
            output.push_back(automap_zoom_value);
            automap_zoom_written = true;
        } else if (key == "buddysortmethods") {
            output.push_back(buddy_sort_methods_value);
            buddy_sort_methods_written = true;
        } else if (key == "buddyhideoffline") {
            output.push_back(buddy_hide_offline_value);
            buddy_hide_offline_written = true;
        } else if (key == "lighteffects") {
            output.push_back(light_effects_value);
            light_effects_written = true;
        } else if (key == "nostretching") {
            output.push_back(no_stretching_value);
            no_stretching_written = true;
        } else if (key == "windowsmousepointer") {
            output.push_back(mouse_pointer_value);
            mouse_pointer_written = true;
        } else if (key == "hotkey") {
            if (!hotkeys_written) {
                for (std::size_t hotkey = 0; hotkey < preferences.hotkeys.size(); ++hotkey)
                    if (has_persisted_hotkey(preferences.hotkeys[hotkey]))
                        output.push_back(serialize_hotkey_preference(
                            hotkey, preferences.hotkeys[hotkey]));
                hotkeys_written = true;
            }
        } else if (key == "white" || key == "ignore") {
            auto existing_value = trim(lines[index].substr(split + 1));
            while (!has_list_closing_brace(existing_value) && index + 1 < lines.size()) {
                existing_value.push_back('\n');
                existing_value += lines[++index];
            }
            if (key == "white") {
                output.push_back(white_value);
                white_written = true;
            } else {
                output.push_back(ignore_value);
                ignore_written = true;
            }
        } else if (key == "knownhints") {
            auto existing_value = trim(lines[index].substr(split + 1));
            while (!has_list_closing_brace(existing_value) && index + 1 < lines.size())
                existing_value += "\n" + lines[++index];
            if (!known_hints_written) output.push_back(known_hints_value);
            known_hints_written = true;
        } else if (key == "buddy") {
            if (!buddies_written) serialize_buddies();
            buddies_written = true;
        } else {
            output.push_back(lines[index]);
        }
    }

    if (!event_written) output.push_back(event_value);
    if (!status_written) output.push_back(status_value);
    if (!info_written) output.push_back(info_value);
    if (!timestamps_written) output.push_back(timestamps_value);
    if (!levels_written) output.push_back(levels_value);
    if (!private_messages_written) output.push_back(private_messages_value);
    if (!classic_control_written) output.push_back(classic_control_value);
    if (!auto_chase_off_written) output.push_back(auto_chase_off_value);
    if (!attack_mode_written) output.push_back(attack_mode_value);
    if (!chase_mode_written) output.push_back(chase_mode_value);
    if (!secure_mode_written) output.push_back(secure_mode_value);
    if (!show_new_hints_written) output.push_back(show_new_hints_value);
    if (!level_bar_written) output.push_back(level_bar_value);
    if (!stamina_bar_written) output.push_back(stamina_bar_value);
    if (!magic_level_bar_written) output.push_back(magic_level_bar_value);
    if (!skill_bars_written) output.push_back(skill_bars_value);
    if (!creature_info_written) output.push_back(creature_info_value);
    if (!textual_effects_written) output.push_back(textual_effects_value);
    if (!test_server_written) output.push_back(test_server_value);
    if (preferences.fullscreen_mode_configured && !fullscreen_mode_written)
        output.push_back(fullscreen_mode_value);
    if (!basic_brightness_written) output.push_back(basic_brightness_value);
    if (!confine_framerate_written) output.push_back(confine_framerate_value);
    if (!engine_written) output.push_back(engine_value);
    if (!automap_zoom_written) output.push_back(automap_zoom_value);
    if (!buddy_sort_methods_written) output.push_back(buddy_sort_methods_value);
    if (!buddy_hide_offline_written) output.push_back(buddy_hide_offline_value);
    if (!light_effects_written) output.push_back(light_effects_value);
    if (!no_stretching_written) output.push_back(no_stretching_value);
    if (!mouse_pointer_written) output.push_back(mouse_pointer_value);
    if (!white_written) output.push_back(white_value);
    if (!ignore_written) output.push_back(ignore_value);
    if (!known_hints_written) output.push_back(known_hints_value);
    if (!buddies_written) serialize_buddies();
    if (!open_dialogs_written) serialize_open_dialogs();
    if (!content_windows_written) serialize_content_windows();
    if (!hotkeys_written) {
        for (std::size_t hotkey = 0; hotkey < preferences.hotkeys.size(); ++hotkey)
            if (has_persisted_hotkey(preferences.hotkeys[hotkey]))
                output.push_back(serialize_hotkey_preference(hotkey, preferences.hotkeys[hotkey]));
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) throw std::runtime_error("cannot write Tibia.cfg");
    for (const auto& output_line : output) file << output_line << '\n';
    if (!file) throw std::runtime_error("cannot finish writing Tibia.cfg");
}

}
