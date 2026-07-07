#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// The WndProc-shaped entry point over a set of composed message mixins: inherits
/// each, and `handle_message` tries their `dispatch` in the order listed, stopping
/// at the first that handles the message (first-match wins). When no mixin claims
/// the message, falls back to the derived type's `default_proc` -- the only thing
/// that varies between wrappers is which default proc closes the gap. Compose a
/// wrapper as `class C : public MessageHandler<C, Paintable, MouseInput, ...>` and
/// route its WndProc to `handle_message`.
///
/// @tparam Derived  The final user type; must provide a public
///                  `LRESULT default_proc(UINT, WPARAM, LPARAM)`.
/// @tparam Mixins   The message mixins to compose, tried in the order given.
template <typename Derived, template <typename> typename... Mixins>
struct MessageHandler : Mixins<Derived>... {
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
        std::optional<LRESULT> result;
        ((result = Mixins<Derived>::dispatch(msg, wparam, lparam)) || ...);
        if (result)
            return *result;
        return static_cast<Derived*>(this)->default_proc(msg, wparam, lparam);
    }
};

}  // namespace winwrap
