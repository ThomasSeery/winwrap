# winwrap — roadmap

What's done, what's next, and the spec for each piece. Design rationale lives in
[VISION.md](VISION.md).

## ▶ Next session: start here

**Goal: an MVP usable in wifi-toggle** = `Window` ✓ + `Menu` ✓ + `NotifyIcon`,
composing through the `on_*` hooks. Design is locked (see *Decisions locked* →
*Tray / menu integration*) — don't re-litigate; build straight against it.

> **v0.2 `Control<T>` is ✅ done** — implemented + MSVC-verified (compiles + links via
> `control_test`; see [CONTROL_PLAN.md](CONTROL_PLAN.md)), though not yet exercised on
> screen (§8). Next: a control-on-screen exercise app, then the v0.1 app track below.

1. ~~`hello-window` example~~ — ✅ **Done & runs on screen.**
   `MainWindow : public Window<MainWindow>` + `wWinMain` + message loop. It's a
   **standalone project** at `cpp/windows/hello-window` (scaffolded with
   `newcpp -Windows`), *not* an `examples/` subdir; it pulls winwrap's headers via a
   local include path (`Window` is header-only). Currently uncommitted (throwaway).
2. ~~Task 2 — `Menu`~~ — ✅ **Done** (see Task 2 below).
3. ~~Task 3 — `NotifyIcon`~~ — ✅ **Done** (implemented; not yet exercised — see Task 3).
4. **`tray_app`** (standalone, like hello-window) — window + tray + menu
   end-to-end. The wifi-toggle shape minus the wifi logic. ← **next in the v0.1 track** (after v0.2 Controls)
5. **Wire into wifi-toggle** — swap its hand-rolled Win32 for winwrap.
6. **Tag `v0.1`** — so wifi-toggle pins a tag (no surprise BC).

## Current state (v0.1)

- **`Window<T>`** — ✅ **Done & exercised.** Configurable `create(WindowConfig)`,
  two-layer registration/creation, `std::expected` error model, `configure_class`
  hook, `WM_NCDESTROY` lifetime fix, a **`show(cmd)`** convenience, and **compile-time
  `on_*` message hooks** (see *Decisions locked → Message dispatch*). Runs on screen
  via `hello-window`. Builds clean (MSVC `/W4`), clang-tidy-clean, Doxygen-documented.
- **`Menu`** — ✅ **Done.** `class Menu final`; `create()` factory →
  `std::expected<Menu, std::error_code>`; `add_item` / `show` / `handle()`; owns its
  `HMENU` via `wil::unique_hmenu` (move-only). `show` posts `WM_COMMAND` → owner's
  `on_command`. Builds clean, clang-tidy-clean.
- **`NotifyIcon`** — ✅ **Done.** Move-only RAII wrapper over `Shell_NotifyIcon`,
  targeting `NOTIFYICON_VERSION_4`. `create(NotifyIconConfig)` → `std::expected`;
  `add()` (initial add + `WM_TASKBARCREATED` re-add), `set_tooltip`, static
  `taskbar_created_message()`; owns `HICON` via `wil::unique_hicon`; `NIM_DELETE` on
  destruct; hand-written Rule-of-Five (the shell registration isn't an RAII handle).
  Builds clean (`/W4` + sanitizers), clang-tidy-clean. **Not yet exercised** by an app.
- **`error.hpp`** — `last_error()` lives in `winwrap/error.hpp` (shared by
  `window.hpp` and `menu.cpp`).
- **Build** — CMake + WIL + install/export + warnings/sanitizers. Solid; no work
  needed.

## Decisions locked

- **Build our own CRTP window bridge** — not ATL `CWindowImpl`. (See VISION → the
  reuse rule.)
- **Window config via a `WindowConfig` struct** + C++20 designated initializers —
  not a builder, not static traits. Drops the variadic/forwarding from `create()`.
- **Config-struct factories (generalised):** any public factory with multiple args
  (or any two same-typed args) takes a `*Config` struct + designated initializers —
  `NotifyIconConfig` joins `WindowConfig`. Codified in `CODE_CONVENTIONS.md` (winwrap);
  the struct-doc style lives in `cpp/CODE_CONVENTIONS.md` (all projects).
- **Errors as `std::expected<T, std::error_code>`** at the public API; Win32 codes
  via `std::system_category()`. WIL stays for RAII handles only, not control flow.
- **Message dispatch via named `on_*` hooks** (evolved this session). `Window`'s
  `handle_message` is now a **compile-time dispatcher**: for each well-known message
  it does `if constexpr (requires { self->on_x(); })` and calls the derived hook if
  present — `on_create` / `on_close` / `on_destroy` / `on_paint` / `on_size(w,h)` /
  `on_command(id)` — else `DefWindowProcW`. No vtables. Derived windows define the
  **public** `on_*` they need instead of writing a `switch`. `handle_message` stays
  **shadowable** as the escape hatch for runtime-id messages (e.g. the tray
  callback) — delegate the rest with `Window::handle_message`.
- **Class-level config** (the `WNDCLASS`) is customized by a `configure_class`
  hook the derived type shadows — same CRTP mechanism as the `on_*` dispatch.
- **Doc comments:** Doxygen `///` + `@`-commands on the **public API** only;
  one-line `if` bodies drop braces. (Both codified in `CODE_CONVENTIONS.md`.)
- **Lint carve-outs** (`.clang-tidy`): `#pragma once` allowed; `reinterpret-cast`
  and `int-to-ptr` allowed (inherent to native Win32). The `-Windows` variant also
  disables `convert-member-functions-to-static` (the `on_*` hooks are instance
  methods by contract) and `named-parameter` (unused Win32 callback / entry-point
  params stay unnamed, else MSVC `/W4 C4100` fires). All propagated to `newcpp.ps1`.
  winwrap's `.clang-tidy` further disables `pro-type-union-access` (SDK anonymous
  unions, e.g. `NOTIFYICONDATAW.uVersion`) and `easily-swappable-parameters` (Win32
  passes adjacent same-typed scalars pervasively) — candidates to propagate.
  **Lint triage policy** (in `CODE_CONVENTIONS.md`): the compiler is the source of
  truth — suppress a check if it's wrong/inapplicable (with a comment why), else fix
  the code.

### Tray / menu integration (locked Stage-0, governs Tasks 2–3)

- **Tray model:** `NotifyIcon` **attaches to a `Window<T>` you own** — takes the
  owner's `HWND` + a callback message id; tray events arrive at that window's
  `handle_message`. *Not* a self-contained callback object — one event path, no
  parallel `std::function` layer. A `TrayApp` convenience bundle can be added later,
  additively.
- **Menu routing:** `Menu::show` posts **`WM_COMMAND` to the owner window**,
  delivered to the owner's **`on_command(id)`** hook (via the `handle_message`
  dispatcher) — not `TPM_RETURNCMD`. All events on one path.
- **Tray events exposed raw** (you `switch` on the callback message in
  `handle_message`) for the MVP. A typed `TrayEvent` enum is a future *additive*
  layer, not a v1 requirement.
- **Target `NOTIFYICON_VERSION_4`** from the start (richer events + GUID identity).
  Changing the protocol later would be a BC break — do it now.
- **Tray app's window is message-only:** create with `WindowConfig{ .parent =
  HWND_MESSAGE }`. No new API — `Window` already supports it.
- **Composition & lifetime:** `NotifyIcon` and `Menu` are **members of the derived
  window**, initialized in `on_created()` (where `hwnd()` is valid). Their lifetime
  = the window's.
- **Window dependencies** (e.g. wifi-toggle's controller): inject via members /
  `on_created()` for now (`T` stays default-constructible). A ctor-arg-forwarding
  `create` overload is **additive** later — not a BC break.

---

## Task 1 — `Window<T>`: configurability + lifetime + error model — ✅ DONE

Delivered in `lib/include/winwrap/window.hpp`: `WindowConfig` struct; `create()`
returning `std::expected<std::unique_ptr<T>, std::error_code>`; two-layer
registration/creation; `last_error()` helper; `RegisterClassW`/`CreateWindowExW`
error propagation (tolerating `ERROR_CLASS_ALREADY_EXISTS`); `configure_class`
hook; `WM_NCDESTROY` lifetime fix (no dangling `hwnd_`). Verified: builds clean,
clang-tidy-clean, Doxygen-documented.

**Remaining to fully close the original "done when":** exercise it with a running
app — `examples/hello_window` (see *Next session*). The window code is finished;
it just hasn't been shown on screen yet.

## Task 2 — `Menu` — ✅ DONE

Delivered in `lib/include/winwrap/menu.hpp` + `lib/src/menu.cpp`:
- **`class Menu final`** — sealed, move-only (owns `HMENU` via `wil::unique_hmenu`).
- **`create()`** — static factory → `std::expected<Menu, std::error_code>`, wraps
  `CreatePopupMenu` (null → `last_error()`); private ctor adopts the handle.
- **`add_item(UINT id, const wchar_t* text)`** → `AppendMenuW(..., MF_STRING, ...)`,
  returns `std::expected<void, std::error_code>`.
- **`show(HWND owner)`** → `SetForegroundWindow` + `GetCursorPos` +
  `TrackPopupMenuEx` (no `TPM_RETURNCMD`) + `PostMessageW(owner, WM_NULL, 0, 0)`
  (both `TrackPopupMenu` gotchas).
- **`handle()`** — non-owning `HMENU` accessor (escape hatch).
- **Click routing (LOCKED):** `show` posts `WM_COMMAND` → owner's `on_command(id)`.

Builds clean, clang-tidy-clean. **Not yet exercised** by an example — wire a `Menu`
into a window (right-click → `show()` → `on_command`); the tray app will cover it.

## Task 3 — `NotifyIcon`: the differentiator — ✅ DONE

Delivered in `lib/include/winwrap/notify_icon.hpp` + `lib/src/notify_icon.cpp`:
- **`class NotifyIcon final`** — move-only; **hand-written Rule-of-Five** (the shell
  registration is keyed by `(hWnd, uID)`, not an RAII handle — moves neuter the
  source, the destructor runs `NIM_DELETE`). Owns the `HICON` via `wil::unique_hicon`.
- **`create(const NotifyIconConfig&)`** → `std::expected`; constructs then `add()`s,
  with RAII cleanup if `NIM_SETVERSION` fails after `NIM_ADD`.
- **`add()`** — `NIM_ADD` + `NIM_SETVERSION` (→ `NOTIFYICON_VERSION_4`); reused for the
  `WM_TASKBARCREATED` re-add. **`set_tooltip`** → `NIM_MODIFY`. Static
  **`taskbar_created_message()`** → `RegisterWindowMessageW`.
- **`NotifyIconConfig`** struct + designated initializers (kills swappable
  same-typed params; mirrors `WindowConfig`).
- Builds clean (`/W4` + sanitizers), clang-tidy-clean.
- **Not yet exercised** — `tray_app` will wire it into a window's `handle_message`
  (decode the v4 event from `LOWORD(lParam)`) + a `Menu`.

**LOCKED design:** attaches to a `Window<T>` (takes its `HWND` + callback message
id); lives as a member of the derived window, created in `on_created()`; targets
`NOTIFYICON_VERSION_4`.

1. **`NOTIFYICONDATAW`** + `Shell_NotifyIcon(NIM_ADD / NIM_MODIFY / NIM_DELETE)`,
   `NIM_SETVERSION` → `NOTIFYICON_VERSION_4`. RAII: `NIM_DELETE` on destruction.
2. **`uCallbackMessage`** = an app message (e.g. `WM_APP + n`) wired to the owner
   window — tray events arrive at the owner's `handle_message` (raw for MVP).
3. **`wil::unique_hicon`** owns the icon.
4. **`WM_TASKBARCREATED` resurrection** — re-add the icon if Explorer restarts
   (the robustness trick `CTrayNotifyIcon` had).
5. Value-based errors: a `NotifyIcon::create(...)` **factory** returning
   `std::expected<…, std::error_code>` (construction can fail → factory, not ctor).

---

## Post-v0.1 roadmap

Once v0.1 ships (Window + Menu + NotifyIcon + a `tray_app`, wired into wifi-toggle
and tagged), here's the planned order. **None of this blocks v0.1.**

### v0.2 — Controls (`Control<T>` — the CRTP control base)

Full spec: **[CONTROL_PLAN.md](CONTROL_PLAN.md)** (governs this section).

> **✅ `Control<T>` done** (2026-06-28) — implemented + MSVC-verified (compiles + links
> via `tests/control_test.cpp`). Remaining: exercise it on screen (CONTROL_PLAN §8).

> **Not gated behind v0.1.** Controls is library work, independent of the v0.1 app
> track (`tray_app` → wifi-toggle → tag): no shared code, nothing extra to link, so
> it can be built first. Order by priority, not dependency.

The headline post-MVP feature: `Control<T>`, a CRTP base for native child controls
that you **subclass to customise**, mirroring `Window<T>`. A control is just a
`WS_CHILD` window of an OS class (`"BUTTON"`, `"EDIT"`, …) — Windows supplies the
widget; `Control<T>` supplies the same compile-time `on_*` dispatch as `Window<T>`.

- **`Control<T>` CRTP base** — derive a control type
  (`class MyButton : public Control<MyButton>`), shadow compile-time `on_*` hooks
  (`on_paint`, mouse, key, focus), dispatched via `if constexpr` — the `Window<T>`
  pattern. The bridge into the system control's message stream is
  **`SetWindowSubclass`** (the OS owns the control's WndProc); unhandled →
  `DefSubclassProc`. `this` rides in the subclass `dwRefData`, **not**
  `GWLP_USERDATA`.
- **Subclass-to-customise / owner-draw IS the point.** This supersedes the earlier
  *runtime-callback* design (thin `Button` instances + a `CommandRouter` + a
  `Window<T>` routing edit) — **rejected**: no `CommandRouter`, no edit to
  `window.hpp`.
- **Click semantics:** a control's `BN_CLICKED` still arrives at the **parent** as
  `WM_COMMAND` → the window's existing `on_command(id)` — handle clicks there,
  keyed by `id`. `Control<T>` intercepts the control's *own* messages. Click-on-the-
  control-object (message reflection) is a future *additive* layer.
- **Lifetime** mirrors `Window<T>`: non-movable, `create()` →
  `std::expected<std::unique_ptr<T>, std::error_code>`, held as a `unique_ptr`
  member of the owner window; the parent destroys the child HWND (no
  `DestroyWindow`); the dtor / `WM_NCDESTROY` just `RemoveWindowSubclass`.
- **`Control<T>` is the deliverable, not a control catalog.** Apps derive their own
  control types from it; the library does **not** ship pre-built `Button` / `Edit` /
  `ComboBox` wrappers (that's the WTL/Win32++ toolkit direction — out of scope).
  A concrete `final` control enters the library *only reactively* — when the same
  derive-boilerplate recurs across 2–3 real apps — never as a planned set.

**Hard boundary:** native controls + the subclass building block *only* — no layout
engine, no theming framework, no widget toolkit. Not a Qt/wxWidgets replacement.
(Owner-draw via subclassing **is** in scope now — that's the point of `Control<T>`.)

### v0.3 / ongoing — convenience & reach

- **`TrayApp`** convenience bundle (message-only window + `NotifyIcon`), additive.
- **Balloon / toast notifications** over `NotifyIcon`.
- A typed **`TrayEvent`** enum over the raw tray callback message (additive).
- A **ctor-arg-forwarding `create`** overload (once `std::forward` is taught).
- More **RAII-wrapped Win32 objects** as real projects need them.
- **Catch2 tests** that exercise behaviour without a live message pump.

### Later

- **C++17 backport** via preprocessor feature-gating + `nonstd::expected`
  (expected-lite) where `std::expected` is absent. Keep the API backport-friendly
  meanwhile.
- Catch2 tests that exercise behaviour without a live message pump.
