#pragma once

#include "winwrap/win.hpp"

#include <commctrl.h>

#include <expected>
#include <memory>
#include <system_error>

#include "winwrap/message_dispatcher.hpp"
#include "winwrap/error.hpp"
#include "winwrap/mixins.hpp"
#include "winwrap/window_handle.hpp"

namespace winwrap {

/// Settings passed to Control<T>::create, set with designated initializers; omitted
/// fields take the defaults.
struct ControlConfig {
    HWND parent{};             ///< Window the control lives in; gets its `WM_COMMAND`.
    UINT id{};                 ///< Command id; match it in the parent's on_command.
    const wchar_t* text{L""};  ///< Initial caption: button label, static text, edit contents.
    int x{0};                  ///< Left edge, in pixels, from the parent's client area.
    int y{0};                  ///< Top edge, in pixels, from the parent's client area.
    int width{80};             ///< Width, in pixels.
    int height{24};            ///< Height, in pixels.
    DWORD style{0};            ///< Extra control styles; WS_CHILD | WS_VISIBLE are added.
};

/// CRTP base for a native child control -- a button, edit box, checkbox, etc.: a
/// WS_CHILD window of a system-registered class ("BUTTON", "EDIT", "STATIC", ...).
/// Derive as `class Button : public winwrap::Control<Button, Clickable>`: the base
/// owns creation, the SetWindowSubclass->object bridge, message routing, and teardown,
/// while T provides `static constexpr const wchar_t* control_class` and composes the
/// mixins it supports. Dispatch resolves at compile time -- no virtual. Non-movable;
/// lives as a unique_ptr member of the owner window, created in its on_created().
///
/// Two kinds of message handling, both via the composable mixins in
/// <winwrap/mixins.hpp>, both dispatched by the same compile-time fold:
///
///   - **Input hooks** every control gets -- define the ones you need as **public**
///     members on T; absent ones cost nothing:
///       `on_paint()`            -- `WM_PAINT`
///       `on_mouse_move(x, y)`   -- `WM_MOUSEMOVE`
///       `on_lbutton_down(x, y)` -- `WM_LBUTTONDOWN`
///       `on_lbutton_up(x, y)`   -- `WM_LBUTTONUP`
///       `on_key_down(vk)`       -- `WM_KEYDOWN` (virtual-key code)
///       `on_focus(gained)`      -- `WM_SETFOCUS` (true) / `WM_KILLFOCUS` (false)
///   - **Notification mixins** the control opts into via `Mixins...` -- each
///     brings a `std::function` callback you assign (e.g. Clickable -> `on_click`).
///     The owner window's Reflecting mixin bounces the notification down so it lands
///     here. Adding one is mechanical -- see MIXINS.md.
///
/// Shadow dispatch_message in T for anything the mixins don't cover, and delegate the
/// rest with `Control::dispatch_message`.
///
/// @tparam T           The derived control type. Must provide
///                     `static constexpr const wchar_t* control_class` and be
///                     default-constructible.
/// @tparam Mixins  Notification mixins to compose (e.g. Clickable).
template <typename T, typename... Mixins>
class Control
    : public WindowHandle,
      public MessageDispatcher<Paintable, MouseInput, KeyboardInput, FocusAware, Mixins...> {
public:
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    Control(Control&&) = delete;
    Control& operator=(Control&&) = delete;

    /// Creates the child control and sets the default GUI font.
    /// @param cfg  Control settings (parent, id, text, geometry, style).
    /// @return     Sole owner of the live control, or the Win32 error
    ///             (as std::error_code) that stopped creation.
    [[nodiscard]] static std::expected<std::unique_ptr<T>, std::error_code> create(
        const ControlConfig& cfg) {
        auto self = std::unique_ptr<T>{new T{}};
        if (auto make = self->create_control(cfg); !make)
            return std::unexpected(make.error());
        return self;
    }

    /// The control's command id -- what the parent matches in on_command to
    /// identify this control among its siblings.
    [[nodiscard]] UINT id() const { return id_; }

    /// The message fallback: hands any message no hook claimed to DefSubclassProc.
    /// Called by dispatch_message (inherited from MessageDispatcher); not for direct use.
    LRESULT default_proc(UINT msg, WPARAM wparam, LPARAM lparam) {
        return DefSubclassProc(hwnd(), msg, wparam, lparam);
    }

protected:
    Control() = default;
    /// Detaches the subclass if the control is still live (the parent destroys the
    /// HWND, so no DestroyWindow here).
    ~Control() {
        if (hwnd())
            RemoveWindowSubclass(hwnd(), &subclass_proc, 1);
    }

private:
    std::expected<void, std::error_code> create_control(const ControlConfig& cfg) {
        // WS_CHILD | WS_VISIBLE always; the control type's baseline style (e.g.
        // BS_AUTOCHECKBOX) when it declares one; then the caller's extra styles.
        DWORD style = WS_CHILD | WS_VISIBLE | cfg.style;
        if constexpr (requires { T::default_style; })
            style |= T::default_style;
        HWND h = CreateWindowExW(0, T::control_class, cfg.text, style, cfg.x, cfg.y, cfg.width,
                                 cfg.height, cfg.parent,
                                 reinterpret_cast<HMENU>(static_cast<UINT_PTR>(cfg.id)),
                                 GetModuleHandleW(nullptr), nullptr);
        if (!h)
            return std::unexpected(last_error());
        attach(h);
        id_ = cfg.id;
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
        SetWindowSubclass(h, &subclass_proc, 1, reinterpret_cast<DWORD_PTR>(this));
        return {};
    }
    static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                          UINT_PTR /*id_subclass*/, DWORD_PTR ref_data) {
        T* self = reinterpret_cast<T*>(ref_data);
        LRESULT result = self->dispatch_message(msg, wparam, lparam);
        if (msg == WM_NCDESTROY) {
            // The HWND is going away -- detach our proc and sever the dangling
            // pointer so the dtor won't RemoveWindowSubclass a dead handle.
            RemoveWindowSubclass(hwnd, &subclass_proc, 1);
            self->detach();
        }
        return result;
    }

    UINT id_{};  // command id the parent routes on (see on_command)
};

}  // namespace winwrap
