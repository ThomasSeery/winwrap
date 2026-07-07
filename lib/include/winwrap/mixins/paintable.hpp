#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes `WM_PAINT` to `Derived::on_paint()` when defined.
template <typename Derived>
struct Paintable {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM, LPARAM) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        switch (msg) {
            WW_CASE(WM_PAINT, self->on_paint());
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
