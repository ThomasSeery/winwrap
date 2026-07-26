#pragma once

#include "winwrap/win.hpp"

#include <wil/result_macros.h>

namespace winwrap {

/// Runs the calling thread's message loop until WM_QUIT, then returns its exit
/// code -- the value to hand back from wWinMain. Pumps GetMessageW /
/// TranslateMessage / DispatchMessageW, so dispatched messages reach each window's
/// WndProc and its compile-time on_* hooks. WM_QUIT arrives from PostQuitMessage;
/// post it from your main window's on_destroy so closing the window exits the app.
///
/// @return The WM_QUIT exit code (msg.wParam) -- PostQuitMessage's argument.
/// @note   GetMessageW's -1 error is unreachable here (run() supplies a valid &msg
///         and a nullptr HWND filter), so it is an invariant break (fail-fast), not
///         a recoverable std::expected error.
[[nodiscard]] inline int run() {
    constexpr int message_failed_code{-1};
    MSG msg{};
    BOOL status{};
    while ((status = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        FAIL_FAST_IF(status == message_failed_code);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

inline void quit(int error_code = 0) { PostQuitMessage(error_code); }

}  // namespace winwrap
