#pragma once

#include "winwrap/win.hpp"

#include <string>

namespace winwrap {

/// Non-owning base for everything backed by an HWND. The operations common to any
/// window -- its caption, enabled/visible state, and so on -- live here once, so
/// Window<T> and Control<T> share a single implementation instead of each re-exposing
/// their own subset. Mirrors ATL's CWindow: it stores the handle and operates on it,
/// but never manages its lifetime -- the derived wrapper owns that (Window destroys the
/// window; Control merely unsubclasses it). Holds no vtable; destruction always runs
/// through the derived unique_ptr, so the non-virtual destructor is correct.
class WindowHandle {
public:
    /// The underlying window handle, or nullptr once the window is destroyed.
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// This window's caption (title-bar text, button label, static text, edit contents)
    /// as UTF-16.
    [[nodiscard]] std::wstring text() const {
        const int len = GetWindowTextLengthW(hwnd_);
        std::wstring buf(static_cast<size_t>(len), L'\0');
        if (len > 0)
            GetWindowTextW(hwnd_, buf.data(), len + 1);  // writes len chars + the trailing NUL
        return buf;
    }

    /// Sets this window's caption (SetWindowTextW).
    void set_text(const wchar_t* text) noexcept { SetWindowTextW(hwnd_, text); }

    /// Enables or disables this window (EnableWindow).
    void enable(bool enabled) noexcept { EnableWindow(hwnd_, enabled); }

    /// Shows the window (or applies another ShowWindow command).
    /// @param cmd  A ShowWindow command; defaults to SW_SHOW. Pass wWinMain's
    ///             nShowCmd on first display to honour how the app was launched.
    void show(int cmd = SW_SHOW) noexcept { ShowWindow(hwnd_, cmd); }

    /// Whether the window is currently visible (WS_VISIBLE set up its parent chain).
    [[nodiscard]] bool is_visible() const noexcept { return IsWindowVisible(hwnd_) != FALSE; }

protected:
    WindowHandle() = default;
    WindowHandle(const WindowHandle&) = default;
    WindowHandle(WindowHandle&&) = default;
    WindowHandle& operator=(const WindowHandle&) = default;
    WindowHandle& operator=(WindowHandle&&) = default;
    ~WindowHandle() = default;  // non-owning: never touches the handle

    /// Binds the handle once the OS has created the window (called from the derived
    /// WndProc / subclass bridge).
    void attach(HWND h) noexcept { hwnd_ = h; }

    /// Severs the handle when the window is gone, so no stale operation can fire on a
    /// dead HWND.
    void detach() noexcept { hwnd_ = nullptr; }

private:
    HWND hwnd_{};
};

}  // namespace winwrap
