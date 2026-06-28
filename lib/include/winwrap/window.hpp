#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <expected>
#include <memory>
#include <system_error>

#include "winwrap/dispatcher.hpp"
#include "winwrap/error.hpp"
#include "winwrap/messages.hpp"

namespace winwrap {

/// Per-window settings passed to Window::create -- the CreateWindowExW arguments;
/// omitted fields take the defaults below.
struct WindowConfig {
    const wchar_t* title{L""};         ///< Window title-bar text.
    DWORD style{WS_OVERLAPPEDWINDOW};  ///< Window styles; the default is not visible -- OR in WS_VISIBLE, or call show().
    DWORD ex_style{0};                 ///< Extended (WS_EX_*) styles.
    int x{CW_USEDEFAULT};              ///< Left edge in pixels; CW_USEDEFAULT lets Windows place it.
    int y{CW_USEDEFAULT};              ///< Top edge in pixels; CW_USEDEFAULT lets Windows place it.
    int width{CW_USEDEFAULT};          ///< Width in pixels; CW_USEDEFAULT lets Windows size it.
    int height{CW_USEDEFAULT};         ///< Height in pixels; CW_USEDEFAULT lets Windows size it.
    HWND parent{nullptr};              ///< Owner/parent window; null for a top-level window.
};

/// CRTP base for a top-level window. Derive as
/// `class MyWindow : public winwrap::Window<MyWindow>`: the base owns class
/// registration, the WndProc->object bridge, and teardown, while T shadows
/// `handle_message` (and optionally `configure_class` / `on_created`). Dispatch
/// resolves at compile time via static_cast<T*> -- no virtual, no vtable.
///
/// @tparam T  The derived window type. Must provide
///            `static constexpr const wchar_t* window_class_name` and be
///            default-constructible.
template <typename T>
class Window : public Dispatcher<T, Paintable, MouseInput, KeyboardInput, FocusAware> {
    using messages = Dispatcher<T, Paintable, MouseInput, KeyboardInput, FocusAware>;

public:
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /// Builds the object, creates its window, and runs the post-create hook.
    /// @param cfg  Per-window settings (title, style, geometry, parent).
    /// @return     The sole owner of the live window, or the Win32 error
    ///             (as std::error_code) that stopped creation.
    [[nodiscard]] static std::expected<std::unique_ptr<T>, std::error_code>
    create(const WindowConfig& cfg = {}) {
        auto self = std::unique_ptr<T>{new T{}};
        if (auto made = self->create_window(cfg); !made)
            return std::unexpected(made.error());
        self->on_created();
        return self;
    }

    /// The underlying window handle, or nullptr once the window is destroyed.
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

    /// Shows the window (or applies another ShowWindow command).
    /// @param cmd  A ShowWindow command; defaults to SW_SHOW. Pass wWinMain's
    ///             nShowCmd on first display to honour how the app was launched.
    void show(int cmd = SW_SHOW) { ShowWindow(hwnd_, cmd); }

    /// Routes a message to the matching `on_*` hook the derived window defines,
    /// or to `DefWindowProcW` when there is none. Hooks are detected at compile
    /// time, so define only the ones you need -- as **public** members:
    ///
    ///   - `on_create()`            -- WM_CREATE
    ///   - `on_close()`             -- WM_CLOSE
    ///   - `on_destroy()`           -- WM_DESTROY
    ///   - `on_size(w, h)`          -- WM_SIZE (client width/height)
    ///   - `on_command(id)`         -- WM_COMMAND (menu / control id)
    ///
    /// Paint, mouse, keyboard, and focus hooks come from the composable message
    /// traits in <winwrap/messages.hpp>, which this base mixes in.
    ///
    /// For a message with a runtime id (e.g. a tray callback), shadow this whole
    /// function in T instead and delegate the rest with `Window::handle_message`.
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
        if (auto r = messages::dispatch(msg, wparam, lparam))
            return *r;
        [[maybe_unused]] T* self = static_cast<T*>(this);
        switch (msg) {
            case WM_CREATE:
                if constexpr (requires { self->on_create(); }) {
                    self->on_create();
                    return 0;
                } else
                    break;
            case WM_CLOSE:
                if constexpr (requires { self->on_close(); }) {
                    self->on_close();
                    return 0;
                } else
                    break;
            case WM_DESTROY:
                if constexpr (requires { self->on_destroy(); }) {
                    self->on_destroy();
                    return 0;
                } else
                    break;
            case WM_SIZE:
                if constexpr (requires { self->on_size(0U, 0U); }) {
                    self->on_size(LOWORD(lparam), HIWORD(lparam));
                    return 0;
                } else
                    break;
            case WM_COMMAND:
                if constexpr (requires { self->on_command(0); }) {
                    self->on_command(LOWORD(wparam));
                    return 0;
                } else
                    break;
            default:
                break;
        }
        return DefWindowProcW(hwnd_, msg, wparam, lparam);
    }

protected:
    Window() = default;
    ~Window() {
        if (hwnd_) {
            // Detach first so the destroy messages don't dispatch into a
            // half-destroyed object.
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
            DestroyWindow(hwnd_);
        }
    }

    /// Hook to customise the window class before registration (icon, background
    /// brush, class styles). Shadow in T; the base default changes nothing.
    void configure_class(WNDCLASSW& /*wc*/) {}

    /// Hook called once, right after the window exists. Shadow in T for setup
    /// that needs a live HWND.
    void on_created() {}

    std::expected<void, std::error_code> create_window(const WindowConfig& cfg) {
        // --- Registration (per class name): sensible defaults, then let T tweak.
        WNDCLASSW wc{};
        wc.lpfnWndProc = window_proc;
        wc.hInstance = instance_;
        wc.lpszClassName = T::window_class_name;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        static_cast<T*>(this)->configure_class(wc);

        if (RegisterClassW(&wc) == 0) {
            // A class already registered under this name is fine -- anything else
            // is a real failure.
            if (auto ec = last_error(); ec.value() != ERROR_CLASS_ALREADY_EXISTS)
                return std::unexpected(ec);
        }

        // --- Creation (per window): `this` rides through so the static callback
        // can recover the object in WM_NCCREATE.
        HWND hwnd = CreateWindowExW(cfg.ex_style, T::window_class_name, cfg.title, cfg.style,
                                    cfg.x, cfg.y, cfg.width, cfg.height, cfg.parent, nullptr,
                                    instance_, static_cast<T*>(this));
        if (!hwnd)
            return std::unexpected(last_error());
        return {};
    }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        T* self{};
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<T*>(cs->lpCreateParams);
            self->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<T*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self) {
            LRESULT result = self->handle_message(msg, wparam, lparam);
            if (msg == WM_NCDESTROY) {
                // The window is gone; sever the link so the destructor won't
                // DestroyWindow a dead handle.
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                self->hwnd_ = nullptr;
            }
            return result;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    HWND hwnd_{};
    const HINSTANCE instance_{GetModuleHandleW(nullptr)};
};

}  // namespace winwrap
