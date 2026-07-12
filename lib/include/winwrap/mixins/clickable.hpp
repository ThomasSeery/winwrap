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
/// `on_click` is skipped, so a handler-less control is safe to click.
struct Clickable {
    std::function<void()> on_click;  ///< Assign your handler; unset = nothing happens.

    [[nodiscard]] std::optional<LRESULT> handle_message(UINT msg, WPARAM wparam, LPARAM) const {
        return handle_notification(msg, wparam, BN_CLICKED, on_click);
    }
};

}  // namespace winwrap
