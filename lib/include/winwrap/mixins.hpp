#pragma once

/// Umbrella header for winwrap's mixins -- composable compile-time mixins (C++23
/// deducing this: `handle` deduces the final type from its explicit object
/// parameter) that each route one family of window messages to a handler. Two kinds
/// (see MIXINS.md): *hook* mixins detect an `on_*` method the final type defines
/// (empty structs -> empty-base optimization, zero size); *callback* mixins own a
/// `std::function` you assign (one function-object of state). Either way `handle`
/// returns an engaged optional (the LRESULT) when it handled the message, or
/// `std::nullopt` to keep looking -- first match wins, so no message may appear in
/// two mixins. No virtual dispatch in either case.
///
/// Include this to pull in every mixin; the bases (Control, Window) compose the
/// standard hooks by default and accept opt-in mixins through their `Mixins...`.
/// One mixin per file, named for the mixin (not any control). The reflection
/// engine (wm_command_reflect, handle_notification, Reflecting) lives in
/// <winwrap/message_reflection.hpp>, which the callback mixins build on.

#include "winwrap/message_reflection.hpp"  // handle_notification, for the callback mixins

/// One message case: call the hook iff the final type defines it -- existence is
/// detected in an unevaluated `requires` and resolved by `if constexpr`, so the
/// missing-hook branch emits no code (zero runtime cost). Deriving the `requires`
/// check from the same `call` makes the two impossible to drift apart. Owned by this
/// umbrella: defined here, used by the hook fragments below, #undef'd at the end so it
/// never leaks to includers (the fragments are only ever included through here).
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define WW_CASE(message, call)              \
    case message:                           \
        if constexpr (requires { call; }) { \
            call;                           \
            return 0;                       \
        } else                              \
            break

// IWYU pragma: begin_exports
#include "winwrap/mixins/clickable.hpp"             // Clickable           -> on_click (BN_CLICKED)
#include "winwrap/mixins/commandable.hpp"           // Commandable         -> on_command (menu/accelerator)
#include "winwrap/mixins/file_droppable.hpp"        // FileDroppable       -> on_files_dropped (WM_DROPFILES)
#include "winwrap/mixins/focus_aware.hpp"           // FocusAware          -> on_focus (WM_SET/KILLFOCUS)
#include "winwrap/mixins/keyboard_input.hpp"        // KeyboardInput       -> on_key_down (WM_KEYDOWN)
#include "winwrap/mixins/lifecycle.hpp"             // Lifecycle           -> on_create/on_close/on_destroy
#include "winwrap/mixins/mouse_input.hpp"           // MouseInput          -> on_mouse_move/on_lbutton_*
#include "winwrap/mixins/paintable.hpp"             // Paintable           -> on_paint (WM_PAINT)
#include "winwrap/mixins/selection_changeable.hpp"  // SelectionChangeable -> on_selection_changed (CBN_SELCHANGE)
#include "winwrap/mixins/sizable.hpp"               // Sizable             -> on_size (WM_SIZE)
#include "winwrap/mixins/text_changeable.hpp"       // TextChangeable      -> on_text_changed (EN_CHANGE)
// IWYU pragma: end_exports

#undef WW_CASE
