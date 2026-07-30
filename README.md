# winwrap

Modern C++23 wrappers over **native Win32** — top-level windows, native child
controls, menus, and a system-tray icon — for self-contained MSVC apps. A thin
wrapper, not a replacement runtime: winwrap still calls `CreateWindowExW`,
`Shell_NotifyIcon` and friends, every type exposes its raw handle, and you can drop
to plain Win32 at any point.

> **Status:** v0.1, in progress. Compiled static library exposing the
> `winwrap::winwrap` CMake target.

## Why

The open-source field splits into two camps that never overlap: native-Win32 window
frameworks with no tray icon (WinLamb, Win32++, LFWin32, ATL's `CWindowImpl`), and
tray-only libraries with no window framework. The closest existing combination — ATL
plus a third-party tray class — drags in the ATL framework and macro message maps.
winwrap is that pairing done standalone and modern.

- **Compile-time message dispatch.** No vtables, no virtual hierarchy, no macro
  message maps. Messages route to named `on_*` methods your window defines, detected
  with `requires` and resolved by `if constexpr`, with the final type deduced via
  C++23 *deducing this*. Handlers you don't define emit no code.
- **Value-based errors.** Every fallible call returns
  `std::expected<T, std::error_code>` (Win32 codes through `std::system_category()`).
  No `HRESULT` bookkeeping, no exceptions in the happy path.
- **RAII over every resource.** Handles are owned by their wrapper; no manual
  `DestroyWindow` / `DestroyIcon` / `DeleteObject`.
- **One header-only dependency** (WIL). Nothing to ship next to a tray utility.
- **Unicode only**, UTF-16 at the boundary, `…W` APIs throughout.

## Quick start

A window with a button, wired to a click handler:

```cpp
#include <winwrap/controls.hpp>
#include <winwrap/message_loop.hpp>
#include <winwrap/window.hpp>

class MainWindow : public winwrap::Window<MainWindow> {
public:
    static constexpr const wchar_t* window_class_name = L"winwrap_demo";

    void on_created() {
        auto button = winwrap::Button::create(
            {.parent = hwnd(), .id = 1, .text = L"Greet", .x = 12, .y = 12},
            [this] { set_text(L"Hello from winwrap"); });
        if (button)
            greet_ = std::move(*button);
    }

    void on_destroy() { winwrap::quit(); }

private:
    std::unique_ptr<winwrap::Button> greet_;
};

int wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    auto window = MainWindow::create(
        {.title = L"winwrap demo", .style = WS_OVERLAPPEDWINDOW | WS_VISIBLE});
    if (!window)
        return window.error().value();

    return winwrap::run();
}
```

Every hook is optional — define `on_paint`, `on_size(w, h)`, `on_key_down(vk)`,
`on_mouse_move(x, y)`, `on_command(id)`, `on_focus(gained)`, `on_close` and the rest
only when you want them. Extra behaviour composes as a mixin:

```cpp
class DropTarget : public winwrap::Window<DropTarget, winwrap::FileDroppable> {
public:
    static constexpr const wchar_t* window_class_name = L"winwrap_drop";

    void on_files_dropped(const std::vector<std::wstring>& paths) { /* … */ }
};
```

`FileDroppable` calls `DragAcceptFiles` itself — composing the mixin is the whole
declaration, there's no matching flag to remember.

## Tray icon

The tray icon rides the same window bridge: its events arrive as an ordinary window
message you pick an id for.

```cpp
class TrayWindow : public winwrap::Window<TrayWindow> {
public:
    static constexpr const wchar_t* window_class_name = L"winwrap_tray";

    void on_created() {
        auto icon = winwrap::NotifyIcon::create(
            {.owner = hwnd(),
             .callback_msg = tray_callback,
             .id = 1,
             .icon = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION)),
             .tooltip = L"winwrap"});
        if (icon)
            tray_ = std::move(*icon);
    }

    LRESULT dispatch_message(UINT msg, WPARAM wparam, LPARAM lparam) {
        if (msg == tray_callback && LOWORD(lparam) == WM_CONTEXTMENU) {
            show_tray_menu();
            return 0;
        }
        return Window::dispatch_message(msg, wparam, lparam);
    }

private:
    static constexpr UINT tray_callback{WM_APP + 1};

    void show_tray_menu() {
        auto menu = winwrap::Menu::create();
        if (!menu)
            return;
        std::ignore = menu->add_item(L"Exit", [] { winwrap::quit(); });
        menu->show(hwnd());
    }

    std::optional<winwrap::NotifyIcon> tray_;
};
```

The icon is adopted, so pass one that's safe to `DestroyIcon` (a `CopyIcon` of a
system icon, or a non-shared `LoadImageW`) — never a shared system handle.

## What's in the box

| Header | Gives you |
|---|---|
| `winwrap/window.hpp` | `Window<T, Mixins…>` — registration, the callback→object bridge, dispatch, teardown |
| `winwrap/control.hpp`, `winwrap/controls.hpp` | `Control<T, Mixins…>` and concrete `Button`, `Edit`, `Checkbox`, `ComboBox` |
| `winwrap/notify_icon.hpp` | `NotifyIcon` — a system-tray icon, plus the Explorer-restart re-add path (`taskbar_created_message()` → `add()`) |
| `winwrap/menu.hpp` | `Menu` — popup menus, items by id or by lambda |
| `winwrap/drop.hpp` | `Drop` — the `WM_DROPFILES` query protocol as a type |
| `winwrap/mixins.hpp` | the composable behaviours (`FileDroppable`, `Paintable`, `Clickable`, …) |
| `winwrap/message_loop.hpp` | `run()` and `quit()` |
| `winwrap/error.hpp` | `last_error()` / `check()` — Win32 codes as `std::error_code` |

## Requirements

MSVC (Build Tools for Visual Studio 2022 or newer) with C++23 — `std::expected` and
deducing this are both used. clang-cl works; MinGW is not supported. Windows only.

## Build

From an *x64 Native Tools* prompt:

```
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Use it from another project

```cmake
include(FetchContent)
FetchContent_Declare(winwrap
    GIT_REPOSITORY <repo-url>
    GIT_TAG <tag>)
FetchContent_MakeAvailable(winwrap)

target_link_libraries(your_app PRIVATE winwrap::winwrap)
```

WIL comes in transitively; you don't need to fetch it yourself.

## Design notes

[VISION.md](VISION.md) covers the design pillars and the explicit non-goals (no
layout engine, no custom-drawn widgets, no cross-platform layer, no WinRT).
[MIXINS.md](MIXINS.md) and [MESSAGE_LOOP_DESIGN.md](MESSAGE_LOOP_DESIGN.md) document
the dispatch model; [ROADMAP.md](ROADMAP.md) is the work queue.

## License

MIT — see [LICENSE](LICENSE).
</content>
