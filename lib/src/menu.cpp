#include "winwrap/menu.hpp"

#include "winwrap/error.hpp"

namespace winwrap {

std::expected<Menu, std::error_code> Menu::create() {
    return checked(CreatePopupMenu()).transform([](HMENU h) { return Menu{wil::unique_hmenu{h}}; });
}

std::expected<void, std::error_code> Menu::add_item(UINT id, const wchar_t* text) {
    return checked(AppendMenuW(handle_.get(), MF_STRING, id, text));
}

void Menu::show(HWND owner) {
    SetForegroundWindow(owner);
    POINT pt{};
    GetCursorPos(&pt);
    TrackPopupMenuEx(handle_.get(), TPM_RIGHTBUTTON, pt.x, pt.y, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
}

}  // namespace winwrap
