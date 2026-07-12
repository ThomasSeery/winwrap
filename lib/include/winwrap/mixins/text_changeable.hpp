#pragma once

#include "winwrap/win.hpp"

#include <functional>
#include <optional>

#include "winwrap/message_reflection.hpp"

namespace winwrap {

/// CONTROL mixin: fires `on_text_changed` on a reflected `EN_CHANGE` -- the edit's
/// contents changed. Composed by any control that emits `EN_CHANGE` (the "EDIT" class)
/// via `class C : Control<C, TextChangeable>`. The callback is `void()`, not
/// `void(text)`: `EN_CHANGE` carries no text, and reading it on every keystroke would
/// allocate needlessly -- pull the current value with `control.text()` inside the
/// handler when you actually want it. An unassigned `on_text_changed` is skipped.
struct TextChangeable {
    std::function<void()> on_text_changed;  ///< Assign your handler; unset = nothing happens.

    [[nodiscard]] std::optional<LRESULT> handle(UINT msg, WPARAM wparam, LPARAM) const {
        return handle_notification(msg, wparam, EN_CHANGE, on_text_changed);
    }
};

}  // namespace winwrap
