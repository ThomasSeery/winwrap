#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <windowsx.h>

#include <optional>

namespace winwrap {

/// Composable CRTP mixins that route one family of window messages to the
/// compile-time `on_*` hooks the final type defines. Each trait's `dispatch`
/// returns an engaged optional (the LRESULT) when it handled the message, or
/// `std::nullopt` to let the caller keep looking -- first match wins, so no
/// message may appear in two traits. Each trait is empty, so mixing them in adds
/// no size (empty-base optimization) and no virtual dispatch.
///
/// A wrapper can chain traits by hand at the top of its `handle_message`, or
/// compose several at once with winwrap::Dispatcher (see
/// <winwrap/dispatcher.hpp>).
///
/// @tparam Derived  The final user type (e.g. MyButton), threaded through the
///                  wrapper so the trait sees its hooks.

/// One dispatch case: call the hook iff `Derived` defines it -- existence is
/// detected in an unevaluated `requires` and resolved by `if constexpr`, so the
/// missing-hook branch emits no code (zero runtime cost). Deriving the `requires`
/// check from the same `call` makes the two impossible to drift apart.
/// Header-local; #undef'd at the end so it never leaks to includers.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define WW_CASE(message, call)              \
    case message:                           \
        if constexpr (requires { call; }) { \
            call;                           \
            return 0;                       \
        } else                              \
            break

/// Routes WM_PAINT to `Derived::on_paint()` when defined.
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

/// Routes the mouse messages to `on_mouse_move` / `on_lbutton_down` /
/// `on_lbutton_up`, each taking the client-area (x, y) from lparam.
template <typename Derived>
struct MouseInput {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM, LPARAM lparam) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        switch (msg) {
            WW_CASE(WM_MOUSEMOVE, self->on_mouse_move(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            WW_CASE(WM_LBUTTONDOWN,
                    self->on_lbutton_down(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            WW_CASE(WM_LBUTTONUP, self->on_lbutton_up(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            default:
                break;
        }
        return std::nullopt;
    }
};

/// Routes WM_KEYDOWN to `Derived::on_key_down(vk)` (the virtual-key code).
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

/// Routes WM_SETFOCUS / WM_KILLFOCUS to `Derived::on_focus(gained)` -- true on
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

/// Routes the window lifecycle messages to `on_create` / `on_close` /
/// `on_destroy`. (Top-level windows only -- a subclassed control is attached
/// after WM_CREATE has already fired.)
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

/// Routes WM_SIZE to `Derived::on_size(width, height)` (client area, from lparam).
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

/// Routes WM_COMMAND to `Derived::on_command(id)` (menu / control id, low word of
/// wparam). The parent window receives a control's WM_COMMAND, not the control.
template <typename Derived>
struct Commandable {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        switch (msg) {
            WW_CASE(WM_COMMAND, self->on_command(LOWORD(wparam)));
            default:
                break;
        }
        return std::nullopt;
    }
};

#undef WW_CASE

}  // namespace winwrap
