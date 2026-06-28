#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <commctrl.h>

#include <expected>
#include <memory>
#include <system_error>

#include "winwrap/dispatcher.hpp"
#include "winwrap/error.hpp"
#include "winwrap/messages.hpp"

namespace winwrap {

/// Settings passed to Control<T>::create, set with designated initializers; omitted
/// fields take the defaults.
struct ControlConfig {
    HWND parent{};             ///< Window the control lives in; gets its WM_COMMAND.
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
/// Derive as `class MyButton : public winwrap::Control<MyButton>`: the base owns
/// creation, the SetWindowSubclass->object bridge, and teardown, while T provides
/// `static constexpr const wchar_t* control_class` and shadows the on_* hooks.
/// Dispatch resolves at compile time -- no virtual. Non-movable; lives as a
/// unique_ptr member of the owner window, created in its on_created().
///
/// @tparam T  The derived control type. Must provide
///            `static constexpr const wchar_t* control_class` and be
///            default-constructible.
template <typename T>
class Control : public Dispatcher<T, Paintable, MouseInput, KeyboardInput, FocusAware> {
    using messages = Dispatcher<T, Paintable, MouseInput, KeyboardInput, FocusAware>;

public:
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    Control(Control&&) = delete;
    Control& operator=(Control&&) = delete;

    /// Creates the child control (and sets the default GUI font), or the Win32
    /// error that stopped creation.
    [[nodiscard]] static std::expected<std::unique_ptr<T>, std::error_code> create(
        const ControlConfig& cfg) {
        auto self = std::unique_ptr<T>{new T{}};
        if (auto make = self->create_control(cfg); !make)
            return std::unexpected(make.error());
        return self;
    }

    /// The underlying control handle, or nullptr once the control is destroyed.
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

    /// The control's command id -- what the parent matches in on_command to
    /// identify this control among its siblings.
    [[nodiscard]] UINT id() const { return id_; }

    /// Sets the control's caption (SetWindowTextW).
    void set_text(const wchar_t* text) { SetWindowTextW(hwnd_, text); }

    /// Enables or disables the control (EnableWindow).
    void enable(bool enabled) { EnableWindow(hwnd_, enabled); }

    /// Routes a message to the matching on_* hook T defines, or to DefSubclassProc
    /// when there is none. Hooks are detected at compile time, so define only the
    /// ones you need -- as **public** members. They come from the composable
    /// message traits in <winwrap/messages.hpp>, which this base mixes in:
    ///
    ///   - `on_paint()`              -- WM_PAINT
    ///   - `on_mouse_move(x, y)`     -- WM_MOUSEMOVE
    ///   - `on_lbutton_down(x, y)`   -- WM_LBUTTONDOWN
    ///   - `on_lbutton_up(x, y)`     -- WM_LBUTTONUP
    ///   - `on_key_down(vk)`         -- WM_KEYDOWN (virtual-key code)
    ///   - `on_focus(gained)`        -- WM_SETFOCUS (true) / WM_KILLFOCUS (false)
    ///
    /// Shadow this whole function in T for anything the named hooks don't cover,
    /// and delegate the rest with `Control::handle_message`.
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
        if (auto r = messages::dispatch(msg, wparam, lparam))
            return *r;
        return DefSubclassProc(hwnd_, msg, wparam, lparam);
    }

protected:
    Control() = default;
    /// Detaches the subclass if the control is still live (the parent destroys the
    /// HWND, so no DestroyWindow here).
    ~Control() {
        if (hwnd_)
            RemoveWindowSubclass(hwnd_, &subclass_proc, 1);
    }

private:
    std::expected<void, std::error_code> create_control(const ControlConfig& cfg) {
        HWND h = CreateWindowExW(0, T::control_class, cfg.text, WS_CHILD | WS_VISIBLE | cfg.style,
                                 cfg.x, cfg.y, cfg.width, cfg.height, cfg.parent,
                                 reinterpret_cast<HMENU>(static_cast<UINT_PTR>(cfg.id)),
                                 GetModuleHandleW(nullptr), nullptr);
        if (!h)
            return std::unexpected(last_error());
        hwnd_ = h;
        id_ = cfg.id;
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
        SetWindowSubclass(h, &subclass_proc, 1, reinterpret_cast<DWORD_PTR>(this));
        return {};
    }
    static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                          UINT_PTR /*id_subclass*/, DWORD_PTR ref_data) {
        T* self = reinterpret_cast<T*>(ref_data);
        LRESULT result = self->handle_message(msg, wparam, lparam);
        if (msg == WM_NCDESTROY) {
            // The HWND is going away -- detach our proc and sever the dangling
            // pointer so the dtor won't RemoveWindowSubclass a dead handle.
            RemoveWindowSubclass(hwnd, &subclass_proc, 1);
            self->hwnd_ = nullptr;
        }
        return result;
    }

    HWND hwnd_{};  // the control window; nulled on WM_NCDESTROY (the dtor's guard)
    UINT id_{};    // command id the parent routes on (see on_command)
};

}  // namespace winwrap
