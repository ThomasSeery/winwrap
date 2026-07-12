#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// WINDOW mixin. Routes a menu or accelerator command (lparam == 0) to the final
/// type's `on_command(id)` (low word of wparam). Control notifications (lparam != 0)
/// are not commands the window handles directly -- Reflecting bounces those home.
/// Commandable and Reflecting split `WM_COMMAND` between them by lparam (== 0 here,
/// != 0 there). Dispatch is first-match-wins, so if these conditions ever overlap --
/// or another mixin matches `WM_COMMAND` -- one handler silently steals the other's
/// messages. Keep the split exact.
struct Commandable {
    std::optional<LRESULT> handle_message([[maybe_unused]] this auto& self, UINT msg, WPARAM wparam,
                                          LPARAM lparam) {
        if (msg == WM_COMMAND && lparam == 0)
            if constexpr (requires { self.on_command(LOWORD(wparam)); }) {
                self.on_command(LOWORD(wparam));
                return 0;
            }
        return std::nullopt;
    }
};

}  // namespace winwrap
