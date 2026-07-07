#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes the window lifecycle messages to `on_create` / `on_close` /
/// `on_destroy`. (Top-level windows only -- a subclassed control is attached
/// after `WM_CREATE` has already fired.)
template <typename Derived>
struct Lifecycle {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM, LPARAM) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        switch (msg) {
            WW_CASE(WM_CREATE, self->on_create());
            WW_CASE(WM_CLOSE, self->on_close());
            WW_CASE(WM_DESTROY, self->on_destroy());
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
