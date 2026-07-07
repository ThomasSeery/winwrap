#pragma once

#include "winwrap/win.hpp"

#include <functional>
#include <optional>

#include "winwrap/message_reflection.hpp"

namespace winwrap {

/// CONTROL mixin: fires `on_selection_changed(index)` on a reflected `CBN_SELCHANGE`
/// -- the user picked a different combo-box item. Composed by any control that emits
/// `CBN_SELCHANGE` (the "COMBOBOX" class) via `class C : Control<C, SelectionChangeable>`.
///
/// Unlike Clickable / TextChangeable, this carries a payload: the selected index is not
/// in the notification, so the mixin queries the control (`CB_GETCURSEL`) once the
/// match is confirmed -- which is why this mixin uses its `Derived` parameter (to reach
/// `hwnd()`), where the void mixins ignore theirs. The index is `CB_ERR` (-1) when
/// the selection was cleared. An unassigned `on_selection_changed` is skipped.
///
/// @tparam Derived  The composing control; supplies `hwnd()` for the `CB_GETCURSEL` query.
template <typename Derived>
struct SelectionChangeable {
    std::function<void(int)> on_selection_changed;  ///< Assign your handler; gets the new index.

    std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM) {
        auto* self = static_cast<Derived*>(this);
        return dispatch_notification(msg, wparam, CBN_SELCHANGE, on_selection_changed, [self] {
            return static_cast<int>(SendMessageW(self->hwnd(), CB_GETCURSEL, 0, 0));
        });
    }
};

}  // namespace winwrap
