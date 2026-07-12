#pragma once

#include "winwrap/win.hpp"

#include <windowsx.h>

#include <optional>

namespace winwrap {

/// Routes the mouse messages to `on_mouse_move` / `on_lbutton_down` /
/// `on_lbutton_up`, each taking the client-area (x, y) from lparam.
struct MouseInput {
    std::optional<LRESULT> handle([[maybe_unused]] this auto& self, UINT msg, WPARAM,
                                    LPARAM lparam) {
        switch (msg) {
            WW_CASE(WM_MOUSEMOVE, self.on_mouse_move(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            WW_CASE(WM_LBUTTONDOWN,
                    self.on_lbutton_down(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            WW_CASE(WM_LBUTTONUP, self.on_lbutton_up(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
