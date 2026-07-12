#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// The WndProc-shaped entry point over a set of composed message mixins: inherits
/// each, and `dispatch_message` tries their `handle_message` in the order listed, stopping
/// at the first that handles the message (first-match wins). When no mixin claims
/// the message, falls back to the final type's `default_proc` -- the only thing
/// that varies between wrappers is which default proc closes the gap. Compose a
/// wrapper as `class C : public MessageDispatcher<Paintable, MouseInput, ...>` and
/// route its WndProc to `dispatch_message`.
///
/// The final type is never named here: `dispatch_message` takes an explicit object
/// parameter (C++23 deducing this), so `self` is deduced as the most-derived type
/// at each call site and flows into every mixin's `handle_message` -- the same
/// compile-time resolution CRTP gave, without a `Derived` parameter or casts.
/// The final type must provide a public `LRESULT default_proc(UINT, WPARAM, LPARAM)`.
///
/// @tparam Mixins  The message mixins to compose, tried in the order given.
template <typename... Mixins>
struct MessageDispatcher : Mixins... {
    LRESULT dispatch_message(this auto& self, UINT msg, WPARAM wparam, LPARAM lparam) {
        std::optional<LRESULT> result;
        ((result = handle_message<Mixins>(self, msg, wparam, lparam)) || ...);
        if (result)
            return *result;
        return self.default_proc(msg, wparam, lparam);
    }

private:
    // One mixin's handle_message, called on the full object. Exists only because MSVC
    // rejects expanding the pack inside `self.Mixins::handle_message` in the fold
    // (C7515); a template argument is a position it can expand.
    template <typename Mixin>
    static std::optional<LRESULT> handle_message(auto& self, UINT msg, WPARAM wparam,
                                                 LPARAM lparam) {
        return self.Mixin::handle_message(msg, wparam, lparam);
    }
};

}  // namespace winwrap
