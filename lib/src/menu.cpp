#include "winwrap/menu.hpp"

#include "winwrap/error.hpp"

namespace winwrap {

std::expected<Menu, std::error_code> Menu::create() {
    HMENU h = CreatePopupMenu();
    if (!h)
        return std::unexpected(last_error());
    return Menu{wil::unique_hmenu{h}};
}

std::expected<void, std::error_code> Menu::add_item(UINT id, const wchar_t* text) {
    if (AppendMenuW(handle_.get(), MF_STRING, id, text) == 0)
        return std::unexpected(last_error());
    return {};
}

void Menu::show(HWND owner) {
    SetForegroundWindow(owner);
    POINT pt{};
    GetCursorPos(&pt);
    TrackPopupMenuEx(handle_.get(), TPM_RIGHTBUTTON, pt.x, pt.y, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
}

}  // namespace winwrap
