# winwrap

A C++23 static library of **thin wrappers over native Win32**: top-level windows,
native child controls, menus, a system-tray icon. Every type exposes its raw handle;
you can always drop to plain Win32. Not a framework, not cross-platform, not WinRT.
v0.1, headed for public release.

Its Win32 house style **is** the tier's — the same conventions as every Windows
project of mine — so load that first, then winwrap's own API conventions:

@../AGENTS.md

@CODE_CONVENTIONS.md

## Layout

`lib/include/winwrap/` — public headers (`window.hpp`, `control.hpp` + `controls/`,
`notify_icon.hpp`, `menu.hpp`, `mixins.hpp` + `mixins/`, `error.hpp`); `lib/src/` —
two `.cpp`s; `tests/` — Catch2, one file per type. Build from an *x64 Native Tools*
prompt: `cmake --preset dev`, `cmake --build --preset dev`, `ctest --preset dev`.

## Terminology

| Term | Means |
|---|---|
| **dispatcher** | `MessageDispatcher` — folds a `WM_*` over the composed mixins at compile time, then `default_proc` |
| **`handle_message`** | a mixin's message function; returns an engaged `optional<LRESULT>` if it handled it, `nullopt` to keep looking. First match wins |
| **mixin** | one opt-in behaviour composed into `Window<T, Mixins...>` / `Control<T, Mixins...>` |
| **`on_*` hook** | the public member the *user's* type defines (`on_paint`); mixins detect it with `requires` |
| **deducing this** (C++23) | how the final type is recovered — CRTP without a `Derived` parameter |
| **reflection** | bouncing a control's `WM_COMMAND`/`WM_NOTIFY` from the parent back to the control object |
| **`*Config`** | the designated-initializer struct every public factory takes |

## Read on demand

| Read | Before you |
|---|---|
| `VISION.md` | make a design call — pillars, non-goals |
| `ROADMAP.md` | start work — queue + locked decisions |
| `MIXINS.md`, `MESSAGE_LOOP_DESIGN.md` | touch mixins, dispatch, or the loop |
| `TECH_DEBT.md` | wonder why something is shaped oddly |

`*_PROMPT.md` are one-off session briefs, not standing guidance.
</content>
