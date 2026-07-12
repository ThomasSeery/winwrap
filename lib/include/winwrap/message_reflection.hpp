#pragma once

#include "winwrap/win.hpp"

#include <functional>
#include <optional>

namespace winwrap {

/// App-private message used to reflect a control's `WM_COMMAND` from the parent back
/// to the control: Reflecting sends it, the control's mixins (e.g. Clickable)
/// receive it. It sits in the
/// `WM_APP` range the OS reserves for application use -- safe because winwrap owns the
/// message space of the controls it subclasses.
inline constexpr UINT wm_command_reflect = WM_APP + 0x1c00;

/// The shared "reflected `WM_COMMAND` -> callback" dispatch, written once for every
/// control notification mixin to build on. If `msg` is the reflected command and
/// its notification code is `code`, fire `handler` (when assigned) and report the
/// message handled; otherwise pass. (See the mixins in <winwrap/mixins.hpp>.)
/// @param code     The control notification code to match (`BN_CLICKED`, `EN_CHANGE`, ...).
/// @param handler  The callback to fire on a match; an unassigned one is skipped.
/// @return         0 (handled) on a match, else std::nullopt to keep looking.
inline std::optional<LRESULT> dispatch_notification(UINT msg, WPARAM wparam, WORD code,
                                                    const std::function<void()>& handler) {
    if (msg == wm_command_reflect && HIWORD(wparam) == code) {
        if (handler)
            handler();
        return 0;
    }
    return std::nullopt;
}

/// Payload-carrying overload: same match, but the notification's data isn't in the
/// message -- it lives in the control and must be queried (e.g. `CBN_SELCHANGE` -> the
/// selected index via `CB_GETCURSEL`). `fetch` is run **only after** the match, so the
/// control is touched solely when the notification actually fired; its result is the
/// argument passed to `handler`.
/// @param handler  A callback taking the payload (e.g. std::function<void(int)>).
/// @param fetch    A nullary callable that produces the payload; evaluated lazily.
template <typename Handler, typename Fetch>
std::optional<LRESULT> dispatch_notification(UINT msg, WPARAM wparam, WORD code,
                                             const Handler& handler, Fetch fetch) {
    if (msg == wm_command_reflect && HIWORD(wparam) == code) {
        if (handler)
            handler(fetch());
        return 0;
    }
    return std::nullopt;
}

/// WINDOW plumbing mixin (pure routing -- it calls no user handler). Bounces a control's
/// notification down to the control that sent it (lparam carries the control HWND), so
/// the control can handle its own events via its mixins (see Clickable). Generic
/// over every control -- one mixin reflects them all; `WM_NOTIFY` joins here when it lands.
/// Reflecting and Commandable split `WM_COMMAND` between them by lparam (!= 0 here,
/// == 0 there). Dispatch is first-match-wins, so if these conditions ever overlap --
/// or another mixin matches `WM_COMMAND` -- one handler silently steals the other's
/// messages. Keep the split exact.
template <typename Derived>
struct Reflecting {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM lparam) {
        if (msg == WM_COMMAND)
            if (auto* child = reinterpret_cast<HWND>(lparam)) {
                SendMessageW(child, wm_command_reflect, wparam, lparam);
                return 0;
            }
        return std::nullopt;
    }
};

}  // namespace winwrap
