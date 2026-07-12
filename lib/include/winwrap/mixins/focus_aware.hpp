#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes `WM_SETFOCUS` / `WM_KILLFOCUS` to the final type's `on_focus(gained)` --
/// true on gain, false on loss.
struct FocusAware {
    std::optional<LRESULT> handle([[maybe_unused]] this auto& self, UINT msg, WPARAM, LPARAM) {
        switch (msg) {
            WW_CASE(WM_SETFOCUS, self.on_focus(true));
            WW_CASE(WM_KILLFOCUS, self.on_focus(false));
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
