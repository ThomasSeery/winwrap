#include "winwrap/menu.hpp"

#include "winwrap/error.hpp"

namespace winwrap {

std::expected<Menu, std::error_code> Menu::create() {
    return checked(CreatePopupMenu()).transform([](HMENU h) { return Menu{wil::unique_hmenu{h}}; });
}

std::expected<void, std::error_code> Menu::add_item(UINT id, const wchar_t* text) {
    return checked(AppendMenuW(handle_.get(), MF_STRING, id, text));
}

std::expected<void, std::error_code> Menu::add_item(const wchar_t* text,
                                                    std::function<void()> handler) {
    const UINT id = first_handler_id + static_cast<UINT>(handlers_.size());
    return checked(AppendMenuW(handle_.get(), MF_STRING, id, text)).transform([&] {
        handlers_.push_back(std::move(handler));
    });
}

void Menu::show(HWND owner) {
    SetForegroundWindow(owner);
    POINT pt{};
    GetCursorPos(&pt);
    const auto picked = static_cast<UINT>(
        TrackPopupMenuEx(handle_.get(), TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x,
                         pt.y, owner, nullptr));
    PostMessageW(owner, WM_NULL, 0, 0);
    if (picked == 0)
        return;
    if (picked >= first_handler_id && picked - first_handler_id < handlers_.size()) {
        // Copy first: the handler may do anything, including tearing down the
        // window that owns this menu -- the copy outlives us either way.
        auto handler = handlers_[picked - first_handler_id];
        handler();
        return;
    }
    PostMessageW(owner, WM_COMMAND, picked, 0);
}

}  // namespace winwrap
