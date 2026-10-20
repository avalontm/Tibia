#include "client/report_violation_dialog.hpp"

namespace tibia::client {

void ReportViolationDialogState::show() {
    open = true;
    text_focused = true;
    text.clear();
    pressed_button = -1;
}

void ReportViolationDialogState::close() noexcept {
    open = false;
    text_focused = false;
    pressed_button = -1;
}

void ReportViolationDialogState::type(wchar_t character) {
    if (text.size() < 255) text.push_back(character);
}

void ReportViolationDialogState::erase_character() noexcept {
    if (!text.empty()) text.pop_back();
}

}
