#pragma once

#include "winwrap/win.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <system_error>

#include "winwrap/mixins.hpp"
#include "winwrap/control.hpp"

namespace winwrap {

/// A native push button (the "BUTTON" system class) -- a concrete, ready-to-use thin
/// shell over the OS control, not a base to derive from. Create it with Button::create
/// (see Control::create for the creation / error contract), then assign a click handler.
/// on_click is a plain std::function, so it is reassignable at any time and defaults to
/// unset (an unhandled click does nothing):
///
///     button->on_click = [] { /* ... */ };   // `button` is the created unique_ptr<Button>
///
/// The click reaches the parent window as `BN_CLICKED`; the window's Reflecting mixin
/// bounces it back here, where the composed Clickable mixin fires `on_click`
/// (which Clickable provides -- see <winwrap/mixins/clickable.hpp>).
class Button final : public Control<Button, Clickable> {
public:
    static constexpr const wchar_t* control_class = L"BUTTON";

    using Control::create;  // keep the handler-less create(cfg); see below re: name hiding

    /// Creates the button and wires its click handler in one call -- a convenience for
    /// create(cfg) then assigning on_click. Both forms stay valid, and on_click remains
    /// reassignable afterwards.
    /// @param cfg       Control settings (parent, id, text, geometry, style).
    /// @param on_click  Handler invoked on each click; moved into the button's on_click.
    /// @return          Sole owner of the live button, or the Win32 error
    ///                  (as std::error_code) that stopped creation.
    [[nodiscard]] static std::expected<std::unique_ptr<Button>, std::error_code> create(
        const ControlConfig& cfg, std::function<void()> on_click) {
        return Control::create(cfg).transform([cb = std::move(on_click)](auto button) mutable {
            button->on_click = std::move(cb);
            return button;
        });
    }
};

}  // namespace winwrap
