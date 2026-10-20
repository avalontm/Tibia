#pragma once

#include <string>

namespace tibia::client {

struct ReportViolationDialogState {
    bool open{};
    bool text_focused{};
    std::wstring text;
    int pressed_button{-1};

    void show();
    void close() noexcept;
    void type(wchar_t character);
    void erase_character() noexcept;
};

}
