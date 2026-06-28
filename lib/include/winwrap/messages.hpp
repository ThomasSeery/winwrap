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
/// message may appear in two traits. Hooks are detected with
/// `if constexpr (requires { ... })`, so define only the ones you need, as public
/// members; the rest cost nothing. Each trait is empty, so mixing them in adds no
/// size (empty-base optimization) and no virtual dispatch.
///
/// A wrapper can chain traits by hand at the top of its `handle_message`:
///
///   if (auto r = Paintable<T>::dispatch(msg, wparam, lparam)) return *r;
///
/// or compose several at once with winwrap::Dispatcher (see
/// <winwrap/dispatcher.hpp>).
///
/// @tparam Derived  The final user type (e.g. MyButton), threaded through the
///                  wrapper so the trait sees its hooks.

/// Routes WM_PAINT to `Derived::on_paint()` when defined.
template <typename Derived>
struct Paintable {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM, LPARAM) {
        [[maybe_unused]] auto* self = static_cast<Derived*>(this);
        if (msg == WM_PAINT) {
            if constexpr (requires { self->on_paint(); }) {
                self->on_paint();
                return 0;
            }
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
            case WM_MOUSEMOVE:
                if constexpr (requires { self->on_mouse_move(0, 0); }) {
                    self->on_mouse_move(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                    return 0;
                } else
                    break;
            case WM_LBUTTONDOWN:
                if constexpr (requires { self->on_lbutton_down(0, 0); }) {
                    self->on_lbutton_down(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                    return 0;
                } else
                    break;
            case WM_LBUTTONUP:
                if constexpr (requires { self->on_lbutton_up(0, 0); }) {
                    self->on_lbutton_up(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                    return 0;
                } else
                    break;
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
            case WM_KEYDOWN:
                if constexpr (requires { self->on_key_down(0); }) {
                    self->on_key_down(static_cast<WORD>(wparam));
                    return 0;
                } else
                    break;
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
            case WM_SETFOCUS:
                if constexpr (requires { self->on_focus(true); }) {
                    self->on_focus(true);
                    return 0;
                } else
                    break;
            case WM_KILLFOCUS:
                if constexpr (requires { self->on_focus(true); }) {
                    self->on_focus(false);
                    return 0;
                } else
                    break;
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
            case WM_CREATE:
                if constexpr (requires { self->on_create(); }) {
                    self->on_create();
                    return 0;
                } else
                    break;
            case WM_CLOSE:
                if constexpr (requires { self->on_close(); }) {
                    self->on_close();
                    return 0;
                } else
                    break;
            case WM_DESTROY:
                if constexpr (requires { self->on_destroy(); }) {
                    self->on_destroy();
                    return 0;
                } else
                    break;
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
        if (msg == WM_SIZE) {
            if constexpr (requires { self->on_size(0U, 0U); }) {
                self->on_size(LOWORD(lparam), HIWORD(lparam));
                return 0;
            }
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
        if (msg == WM_COMMAND) {
            if constexpr (requires { self->on_command(0); }) {
                self->on_command(LOWORD(wparam));
                return 0;
            }
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
