#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes `WM_SETFOCUS` / `WM_KILLFOCUS` to `Derived::on_focus(gained)` -- true on
/// gain, false on loss.
template <typename Derived>
struct FocusAware {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM, LPARAM) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        switch (msg) {
            WW_CASE(WM_SETFOCUS, self->on_focus(true));
            WW_CASE(WM_KILLFOCUS, self->on_focus(false));
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
