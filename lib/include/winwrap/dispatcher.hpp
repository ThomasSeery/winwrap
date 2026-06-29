#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <optional>

namespace winwrap {

/// Chains a set of message traits into one mixin: inherits each, then tries their
/// `dispatch` in the order listed, stopping at the first that handles the message
/// (first-match wins). Compose a wrapper as
/// `class C : public Dispatcher<C, Paintable, MouseInput, ...>` and route to
/// `Dispatcher<...>::dispatch` from its `handle_message`.
///
/// @tparam Derived  The final user type, threaded into each trait.
/// @tparam Traits   The trait templates to mix in, tried in the order given.
template <typename Derived, template <typename> typename... Traits>
struct Dispatcher : Traits<Derived>... {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM lparam) {
        std::optional<LRESULT> result;
        ((result = Traits<Derived>::dispatch(msg, wparam, lparam)) || ...);
        return result;
    }
};

/// A Dispatcher plus the top-level `handle_message` entry point: try the composed
/// traits first (first match wins), and when none claims the message, fall back to
/// the derived type's `default_proc`. Use this in place of Dispatcher as the CRTP
/// base wherever a wrapper needs the full WndProc entry point, not just routing --
/// the only thing that varies between wrappers is which default proc closes the gap.
///
/// @tparam Derived  The final user type; must provide a public
///                  `LRESULT default_proc(UINT, WPARAM, LPARAM)`.
/// @tparam Traits   The message traits to compose, tried in the order given.
template <typename Derived, template <typename> typename... Traits>
struct MessageHandler : Dispatcher<Derived, Traits...> {
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
        if (auto r = this->dispatch(msg, wparam, lparam))
            return *r;
        return static_cast<Derived*>(this)->default_proc(msg, wparam, lparam);
    }
};

}  // namespace winwrap
