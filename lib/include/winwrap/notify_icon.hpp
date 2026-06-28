#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>

#include <expected>
#include <string>
#include <system_error>

#include <wil/resource.h>

namespace winwrap {

/// Settings passed to NotifyIcon::create -- the Shell_NotifyIcon parameters, set with
/// designated initializers (omitted fields take the defaults). `owner` is the window
/// tray events post to (its handle_message); `callback_msg` is the app message id
/// they arrive as (use WM_APP + n); `id` identifies this icon within the owner;
/// `icon` is adopted, so it must be safe to DestroyIcon (not a shared system icon);
/// `tooltip` is truncated past 127 chars.
struct NotifyIconConfig {
    HWND owner{};
    UINT callback_msg{};
    UINT id{};
    HICON icon{};
    const wchar_t* tooltip{L""};
};

/// A system-tray (notification-area) icon. Owns its HICON and its shell
/// registration, and attaches to a Window<T> you own: pass that window's HWND
/// plus an app-private callback message id (use WM_APP + n).
///
/// Tray events arrive at the owner window as that callback message. Targeting
/// NOTIFYICON_VERSION_4, decode them in the owner's handle_message: the event is
/// LOWORD(lParam) (e.g. WM_CONTEXTMENU on right-click, NIN_SELECT on left-click)
/// and the icon id is HIWORD(lParam). On destruction the icon is removed.
class NotifyIcon final {
public:
    /// Adds the icon to the notification area, or the Win32 error that stopped it.
    [[nodiscard]] static std::expected<NotifyIcon, std::error_code>
    create(const NotifyIconConfig& cfg);

    NotifyIcon(NotifyIcon&& other) noexcept;
    NotifyIcon& operator=(NotifyIcon&& other) noexcept;
    NotifyIcon(const NotifyIcon&) = delete;
    NotifyIcon& operator=(const NotifyIcon&) = delete;
    ~NotifyIcon();

    /// Updates the hover text (NIM_MODIFY).
    std::expected<void, std::error_code> set_tooltip(const wchar_t* text);

    /// Registers the icon with the shell (NIM_ADD + NIM_SETVERSION). create() calls
    /// this; call it again from the owner on taskbar_created_message() so the icon
    /// survives an Explorer restart.
    std::expected<void, std::error_code> add();

    /// The "TaskbarCreated" broadcast id Explorer sends after it restarts. Compare
    /// against it in the owner's handle_message, then call add().
    [[nodiscard]] static UINT taskbar_created_message();

private:
    NotifyIcon(HWND owner, UINT callback_msg, UINT id, wil::unique_hicon icon,
               const wchar_t* tooltip);

    [[nodiscard]] NOTIFYICONDATAW make_data() const noexcept;
    void remove() noexcept;

    HWND hwnd_;
    UINT callback_msg_;
    UINT id_;
    wil::unique_hicon icon_;
    std::wstring tooltip_;
};

}  // namespace winwrap
