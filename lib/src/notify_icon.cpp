#include "winwrap/notify_icon.hpp"

#include <cwchar>
#include <utility>

#include "winwrap/error.hpp"

namespace winwrap {

NotifyIcon::NotifyIcon(HWND owner, UINT callback_msg, UINT id, wil::unique_hicon icon,
                       const wchar_t* tooltip)
    : hwnd_{owner},
      callback_msg_{callback_msg},
      id_{id},
      icon_{std::move(icon)},
      tooltip_{tooltip ? tooltip : L""} {}

NotifyIcon::NotifyIcon(NotifyIcon&& other) noexcept
    : hwnd_{other.hwnd_},
      callback_msg_{other.callback_msg_},
      id_{other.id_},
      icon_{std::move(other.icon_)},
      tooltip_{std::move(other.tooltip_)} {
    // Neuter the source so its destructor won't NIM_DELETE the icon we now own.
    other.hwnd_ = nullptr;
}

NotifyIcon& NotifyIcon::operator=(NotifyIcon&& other) noexcept {
    if (this != &other) {
        remove();
        hwnd_ = other.hwnd_;
        callback_msg_ = other.callback_msg_;
        id_ = other.id_;
        icon_ = std::move(other.icon_);
        tooltip_ = std::move(other.tooltip_);
        other.hwnd_ = nullptr;
    }
    return *this;
}

NotifyIcon::~NotifyIcon() {
    remove();
}

std::expected<NotifyIcon, std::error_code> NotifyIcon::create(const NotifyIconConfig& cfg) {
    NotifyIcon ni{cfg.owner, cfg.callback_msg, cfg.id, wil::unique_hicon{cfg.icon}, cfg.tooltip};
    // On a partial add (NIM_SETVERSION fails after NIM_ADD), ni's destructor
    // removes the half-configured icon -- RAII does the cleanup.
    if (auto added = ni.add(); !added)
        return std::unexpected(added.error());
    return ni;
}

std::expected<void, std::error_code> NotifyIcon::set_tooltip(const wchar_t* text) {
    tooltip_ = text ? text : L"";
    NOTIFYICONDATAW nid = make_data();
    nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(nid.szTip, tooltip_.c_str(), _TRUNCATE);
    return checked(Shell_NotifyIconW(NIM_MODIFY, &nid));
}

std::expected<void, std::error_code> NotifyIcon::add() {
    NOTIFYICONDATAW nid = make_data();
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = callback_msg_;
    nid.hIcon = icon_.get();
    wcsncpy_s(nid.szTip, tooltip_.c_str(), _TRUNCATE);
    if (auto added = checked(Shell_NotifyIconW(NIM_ADD, &nid)); !added)
        return added;

    nid.uVersion = NOTIFYICON_VERSION_4;
    return checked(Shell_NotifyIconW(NIM_SETVERSION, &nid));
}

UINT NotifyIcon::taskbar_created_message() {
    static const UINT msg = RegisterWindowMessageW(L"TaskbarCreated");
    return msg;
}

NOTIFYICONDATAW NotifyIcon::make_data() const noexcept {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = id_;
    return nid;
}

void NotifyIcon::remove() noexcept {
    if (hwnd_) {
        NOTIFYICONDATAW nid = make_data();
        Shell_NotifyIconW(NIM_DELETE, &nid);
        hwnd_ = nullptr;
    }
}

}  // namespace winwrap
