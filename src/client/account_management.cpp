#include "client/account_management.hpp"

#include <Windows.h>
#include <shellapi.h>
#include <thread>

namespace tibia::client {

void open_account_management_page() {
    std::thread([] {
        ShellExecuteW(nullptr, nullptr,
            L"https://secure.tibia.com/account/?subtopic=accountmanagement",
            nullptr, nullptr, SW_SHOWNORMAL);
    }).detach();
}

}
