# Refactor task: make every message-handling mixin a *behaviour* under `behaviours/`

You are working in `winwrap`, a modern-C++ (C++23, MSVC) static-library wrapper over native
Win32 (headers under `lib/include/winwrap/`, with some `.cpp` sources under `lib/src/`).
**One agent only** — do not run this alongside another session; concurrent edits on
these files cause duplicate-definition breaks.

## The principle

Everything that routes a window message to a handler is a **behaviour** — a composable CRTP
mixin. There is **no "messages vs hooks" distinction**; that split was wrong. `Paintable`,
`Clickable`, `Sizable` are all behaviours and all belong in `behaviours/`. They differ only
in *how the handler is supplied* (an internal detail, not an organizing axis):

- **Hook behaviours** — detect a method `on_xxx()` the derived type defines, via
  `if constexpr (requires { self->on_xxx(); })`. Stateless (empty struct, EBO). Use the
  `WW_CASE` macro. Examples: `Paintable`, `MouseInput`, `KeyboardInput`, `FocusAware`,
  `Sizable`, `Lifecycle`. (`Commandable` is a hook behaviour too but uses a plain
  `if`/`if constexpr`, no `WW_CASE`.)
- **Callback behaviours** — own a `std::function` member you assign at runtime; fire it via
  `dispatch_notification`. Examples: `Clickable`, `TextChangeable`, `SelectionChangeable`.

`Reflecting` is **not** a behaviour — it calls no user handler; it's the window-side plumbing
that bounces a control's `WM_COMMAND` back to the control. It lives with the engine.

## Current state (already done — do NOT redo)

- `win.hpp` — the `<windows.h>` preamble (`WIN32_LEAN_AND_MEAN`/`NOMINMAX`/`<windows.h>`),
  used by every header. `.clang-format` pins `"winwrap/win.hpp"` to Priority 1 so it sorts
  before `commctrl.h`/`windowsx.h`. **Keep as-is.**
- `behaviours/` currently holds only the callback behaviours: `clickable.hpp`,
  `text_changeable.hpp`, `selection_changeable.hpp`. They `#include "winwrap/messages.hpp"`
  for `dispatch_notification`.
- `messages.hpp` currently (WRONGLY) holds: `WW_CASE` (define line ~75, `#undef` at end), the
  hook mixins `Paintable`/`MouseInput`/`KeyboardInput`/`FocusAware`/`Lifecycle`/`Sizable`,
  `Commandable`, `Reflecting`, and the primitives `wm_command_reflect` +
  `dispatch_notification` (two overloads — a plain `void()` one and a payload-carrying
  template one; preserve both verbatim).
- `controls/` holds `button.hpp`, `checkbox.hpp`, `combobox.hpp`, `edit.hpp`.
- `control.hpp` / `window.hpp` are CRTP bases that `#include "winwrap/messages.hpp"` and
  compose the standard hooks + opt-in `Behaviours...`:
  - `Control<T, Behaviours...> : MessageHandler<T, Paintable, MouseInput, KeyboardInput, FocusAware, Behaviours...>`
  - `Window<T> : MessageHandler<T, Lifecycle, Sizable, Commandable, Reflecting, Paintable, MouseInput, KeyboardInput, FocusAware>`
- `dispatcher.hpp` (`Dispatcher`/`MessageHandler` fold), `error.hpp`, `window_handle.hpp`,
  `menu.hpp`, `notify_icon.hpp` — leave alone.
- Build is GREEN: 36 assertions / 13 test cases.

## Target structure

```
winwrap/
├── win.hpp              (unchanged)
├── dispatcher.hpp       (unchanged — the fold)
├── reflection.hpp       NEW. The reflection ENGINE, not a behaviour:
│                          - wm_command_reflect
│                          - dispatch_notification  (both overloads, verbatim)
│                          - Reflecting             (window-side bouncer)
├── behaviours.hpp       The umbrella + the WW_CASE wrapper (see below)
├── behaviours/
│   ├── paintable.hpp        hook  (WW_CASE)
│   ├── mouse_input.hpp      hook  (WW_CASE; also #include <windowsx.h>)
│   ├── keyboard_input.hpp   hook  (WW_CASE)
│   ├── focus_aware.hpp      hook  (WW_CASE)
│   ├── sizable.hpp          hook  (WW_CASE)
│   ├── lifecycle.hpp        hook  (WW_CASE)
│   ├── commandable.hpp      hook  (plain if/if-constexpr, NO WW_CASE)
│   ├── clickable.hpp        callback (dispatch_notification)  [exists]
│   ├── text_changeable.hpp  callback                          [exists]
│   └── selection_changeable.hpp callback                      [exists]
├── controls/  (unchanged)
├── control.hpp, window.hpp  (update includes — see below)
└── messages.hpp        DELETE at the end (contents fully redistributed)
```

## The `WW_CASE` wrapper pattern (the key mechanic)

`WW_CASE` is shared by the hook fragments but must not leak or be duplicated. So **the
umbrella owns it**: `behaviours.hpp` defines `WW_CASE`, includes the fragments that use it,
then `#undef`s it. Shape:

```cpp
// behaviours.hpp
#pragma once
// ... doc ...
#include "winwrap/reflection.hpp"   // dispatch_notification, for the callback behaviours

#define WW_CASE(message, call)              \
    case message:                           \
        if constexpr (requires { call; }) { \
            call;                           \
            return 0;                       \
        } else                              \
            break

#include "winwrap/behaviours/clickable.hpp"
#include "winwrap/behaviours/commandable.hpp"
#include "winwrap/behaviours/focus_aware.hpp"
#include "winwrap/behaviours/keyboard_input.hpp"
#include "winwrap/behaviours/lifecycle.hpp"
#include "winwrap/behaviours/mouse_input.hpp"
#include "winwrap/behaviours/paintable.hpp"
#include "winwrap/behaviours/selection_changeable.hpp"
#include "winwrap/behaviours/sizable.hpp"
#include "winwrap/behaviours/text_changeable.hpp"

#undef WW_CASE
```

Rules for the fragments:
- A hook fragment **uses** `WW_CASE` but does **not** define it and has **no `#ifndef`
  guard** — keep it bare: `#pragma once`, `#include "winwrap/win.hpp"`, `#include <optional>`
  (+ `<windowsx.h>` for `mouse_input`), then the `struct`. They are only ever included via the
  umbrella; do not include them directly.
- Callback fragments (`clickable` etc.): `#pragma once`, `#include "winwrap/win.hpp"`,
  `#include <functional>`, `#include <optional>`, `#include "winwrap/reflection.hpp"`, then
  the `struct` using `dispatch_notification`.
- **Expected**: clangd will show errors on hook fragments when viewed standalone (`WW_CASE`
  undefined). That is inherent to this pattern and fine — MSVC is the ground truth.

## Include updates

- Callback fragments (`clickable`/`text_changeable`/`selection_changeable`): change
  `#include "winwrap/messages.hpp"` → `#include "winwrap/reflection.hpp"`.
- `control.hpp`: change `#include "winwrap/messages.hpp"` → `#include "winwrap/behaviours.hpp"`.
  (Its base list uses the hook behaviours, which come from the umbrella.)
- `window.hpp`: change `#include "winwrap/messages.hpp"` →
  `#include "winwrap/behaviours.hpp"` **and** `#include "winwrap/reflection.hpp"` (it uses
  `Reflecting`).
- Leave the base-class composition lists unchanged (the standard hooks stay composed by
  default; `Behaviours...` stays for opt-in). `behaviours/` is where behaviours are *defined*;
  the bases decide which are *composed*.
- Then delete `messages.hpp`.

## Constraints / conventions

- Preserve all Doxygen (`///` with `@param`/`@return`/`@tparam`) and the existing doc text;
  move it with each struct.
- `dispatch_notification` has TWO overloads (plain + payload-carrying template) — move both.
- Naming: these are **CRTP mixins** (mechanism) / **behaviours** (concept). Do not call them
  "traits" (that means C++ type-traits).
- One behaviour per file, named for the behaviour (e.g. `clickable.hpp`, never `button.hpp`).
- Follow `CODE_CONVENTIONS.md` and `windows/CLAUDE.md` (already in the repo).

## Verify (ground truth = MSVC, NOT clangd)

clangd shows phantom `<expected>` errors on MSVC's C++23 STL — ignore them. After each step,
build and run the tests:

```
cmd //c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build build/dev --target winwrap_test'
build/dev/bin/winwrap_test.exe
```

Done when: clean MSVC build, all tests pass (≥ 36 assertions / 13 cases), `messages.hpp` gone,
every behaviour lives in `behaviours/`, `reflection.hpp` holds the engine, `behaviours.hpp`
owns `WW_CASE`.
