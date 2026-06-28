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

}  // namespace winwrap
