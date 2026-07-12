#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes the window lifecycle messages to `on_create` / `on_close` /
/// `on_destroy`. (Top-level windows only -- a subclassed control is attached
/// after `WM_CREATE` has already fired.)
struct Lifecycle {
    std::optional<LRESULT> handle([[maybe_unused]] this auto& self, UINT msg, WPARAM, LPARAM) {
        switch (msg) {
            WW_CASE(WM_CREATE, self.on_create());
            WW_CASE(WM_CLOSE, self.on_close());
            WW_CASE(WM_DESTROY, self.on_destroy());
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
