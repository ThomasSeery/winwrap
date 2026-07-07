#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes `WM_SIZE` to `Derived::on_size(width, height)` (client area, from lparam).
template <typename Derived>
struct Sizable {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM, LPARAM lparam) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        switch (msg) {
            WW_CASE(WM_SIZE, self->on_size(LOWORD(lparam), HIWORD(lparam)));
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
