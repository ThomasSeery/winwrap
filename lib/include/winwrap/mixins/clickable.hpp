#pragma once

#include "winwrap/win.hpp"

#include <functional>
#include <optional>

#include "winwrap/message_reflection.hpp"

namespace winwrap {

/// CONTROL mixin: fires `on_click` on a reflected `BN_CLICKED`. Composed by any
/// control that emits `BN_CLICKED` -- buttons today, checkboxes / radio buttons later
/// (all "BUTTON"-class) -- via `class C : Control<C, Clickable>`. It is tied to the
/// notification, not to any one control. The callback lives in the mixin, so a
/// control gains both the handler and its storage just by composing it; an unassigned
/// `on_click` is skipped, so a handler-less control is safe to click. The template
/// parameter is unused -- it only lets this compose alongside the Derived-keyed mixins.
template <typename>
struct Clickable {
    std::function<void()> on_click;  ///< Assign your handler; unset = nothing happens.

    std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM) {
        return dispatch_notification(msg, wparam, BN_CLICKED, on_click);
    }
};

}  // namespace winwrap
