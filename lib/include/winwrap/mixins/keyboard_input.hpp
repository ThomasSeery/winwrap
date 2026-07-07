#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes `WM_KEYDOWN` to `Derived::on_key_down(vk)` (the virtual-key code).
template <typename Derived>
struct KeyboardInput {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        switch (msg) {
            WW_CASE(WM_KEYDOWN, self->on_key_down(static_cast<WORD>(wparam)));
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
