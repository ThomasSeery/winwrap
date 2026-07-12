# winwrap — roadmap

What's done, what's next, and the spec for each piece. Design rationale lives in
[VISION.md](VISION.md).

## ▶ Next session: start here

**Goal: an MVP usable in wifi-toggle** = `Window` ✓ + `Menu` ✓ + `NotifyIcon`,
composing through the `on_*` hooks. Design is locked (see *Decisions locked* →
*Tray / menu integration*) — don't re-litigate; build straight against it.

> **v0.2 `Control<T>` is ✅ done** — implemented + MSVC-verified (compiles + links via
> `control_test`), though not yet exercised on screen. Next: a control-on-screen
> exercise app, then the v0.1 app track below.

1. ~~`hello-window` example~~ — ✅ **Done & runs on screen.**
   `MainWindow : public Window<MainWindow>` + `wWinMain` + message loop. It's a
   **standalone project** at `cpp/windows/hello-window` (scaffolded with
   `newcpp -Windows`), *not* an `examples/` subdir; it pulls winwrap's headers via a
   local include path (`Window` is header-only). Currently uncommitted (throwaway).
2. ~~Task 2 — `Menu`~~ — ✅ **Done** (see Task 2 below).
3. ~~Task 3 — `NotifyIcon`~~ — ✅ **Done** (implemented; not yet exercised — see Task 3).
4. **Wire into wifi-toggle** — swap its hand-rolled Win32 for winwrap. This is
   also the first on-screen exercise of `NotifyIcon` + `Menu` (the earlier
   standalone `tray_app` step is dropped — wifi-toggle *is* the exercise app). ← **next**
5. **Tag `v0.1`** — so wifi-toggle pins a tag (no surprise BC).

### wifi-toggle readiness gaps (2026-07-12 comparison vs WinLamb / Win32++ / WTL / WIL)

Close these before or while wiring — wifi-toggle needs each one:

1. **`NotifyIcon::set_icon`** *(blocking — the status recolor is the app's core
   feature)*. `NIM_MODIFY` with `uFlags = NIF_ICON`; identity = the original
   `(hWnd, uID)` from `NIM_ADD`; the shell does **not** take ownership of the
   `HICON`, so swap the owned `wil::unique_hicon` member only after the call
   succeeds. No `NIM_SETVERSION` re-send needed after a modify.
2. **`on_timer(id)` hook** — a `Timable` mixin (`WM_TIMER`, id = `wparam`) per the
   `MIXINS.md` recipe, for the status poll. Precedent: WTL's `MSG_WM_TIMER` →
   `OnTimer(UINT_PTR)`; no surveyed library wraps `SetTimer`/`KillTimer` beyond
   raw members, so the app keeps calling those (an RAII guard is additive later).
3. **Message loop** — stays app-side for the MVP (Microsoft's own NotificationIcon
   sample uses a plain `GetMessage` loop); a `run_message_loop()` free-function
   helper is additive later, growing toward accelerators / modeless-dialog /
   idle handling only if ever needed (WinLamb `RUN()`, Win32++ `CWinApp::Run`,
   WTL `CMessageLoop` are the precedents).
4. **Per-item menu callbacks** — elected for v0.1 (ergonomics, not strictly
   blocking): kills the `on_command` id switch in the app. Design settled — see
   *Decisions locked → Menu routing (revised)* and MENU_CALLBACKS_PROMPT.md.

Non-gaps confirmed by the survey: `NotifyIcon` already exceeds every surveyed
library (v4 protocol + TaskbarCreated re-add; Win32++'s tray sample is legacy-v0
with neither, WTL/WinLamb have no tray support at all), and drawing the red/green
`HICON` itself is app-side GDI by design (`CreateIconIndirect`; winwrap adopts the
result).

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
- **Message dispatch via composable CRTP mixins** (`mixins.hpp` +
  `message_reflection.hpp` + `message_handler.hpp`).
  Seven empty-base mixins — `Lifecycle`, `Sizable`, `Commandable`, `Paintable`,
  `MouseInput`, `KeyboardInput`, `FocusAware` — each expose
  `std::optional<LRESULT> dispatch(UINT, WPARAM, LPARAM)`: engaged = handled,
  `nullopt` = pass on. `MessageHandler<T, Mixins...>` chains them via a fold
  expression (`||`), short-circuiting on first match, and its `handle_message`
  falls back to the derived type's `default_proc` when no mixin claims the
  message. Both `Window<T>` and `Control<T>` inherit it. The `WW_CASE(message, call)` macro (defined + `#undef`'d inside
  `mixins.hpp`) is the only tool that can simultaneously put a maybe-absent member
  into an unevaluated `requires` and `return`/`break` from the caller's frame — one
  line per case, zero duplication. No vtables; all resolved at compile time. Derived
  types define only the **public** `on_*` hooks they need. `handle_message` stays
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
- **Menu routing (revised 2026-07-12 — supersedes the posted-`WM_COMMAND`-only
  design):** menu items take **per-item callbacks** — `add_item(text, handler)`
  auto-assigns an internal id (from `0xE000` up) and stores the handler inside the
  `Menu`; `show` uses `TPM_RETURNCMD` and fires the picked item's handler directly,
  so the id machinery is invisible library plumbing (like control-notification ids).
  **Escape hatch:** the legacy `add_item(id, text)` overload stays; a picked id with
  no stored handler is re-posted as `WM_COMMAND` to the owner → `on_command(id)` via
  `Commandable` — legacy menus, mixed menus, and accelerators behave exactly as
  before. Per item there is exactly one route, chosen by which overload added it.
  User-chosen ids must stay below `0xE000`; on a collision the callback wins.
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
- **Click routing (revised 2026-07-12):** per-item callbacks fired from `show` via
  `TPM_RETURNCMD` (see *Decisions locked → Menu routing*); the legacy
  `add_item(id, text)` → posted `WM_COMMAND` → `on_command(id)` path remains.
  Implementation pending (see MENU_CALLBACKS_PROMPT.md).

Builds clean, clang-tidy-clean. **Not yet exercised** by an example — wire a `Menu`
into a window (right-click → `show()` → `on_command`); wifi-toggle will cover it.

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
- **Not yet exercised** — wifi-toggle will wire it into a window's `handle_message`
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

Once v0.1 ships (Window + Menu + NotifyIcon, wired into wifi-toggle and tagged),
here's the planned order. **None of this blocks v0.1.**

### v0.2 — Controls (`Control<T>` — the CRTP control base)

> **✅ `Control<T>` done** (2026-06-28) — implemented + MSVC-verified (compiles + links
> via `tests/control_test.cpp`). Remaining: exercise it on screen.

> **Not gated behind v0.1.** Controls is library work, independent of the v0.1 app
> track (wifi-toggle → tag): no shared code, nothing extra to link, so
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
- **Click semantics:** a control's `BN_CLICKED` arrives at the **parent** as
  `WM_COMMAND`. ~~Click-on-the-control-object (message reflection) is a future
  *additive* layer.~~ **✅ Built (2026-06-30):** the parent's `Reflecting` mixin now
  bounces the notification back down to the control (`wm_command_reflect`), where the
  control's own mixin fires the callback. Menu / accelerator commands
  (`lparam == 0`) still go to the window's `on_command(id)` via `Commandable`.
- **Lifetime** mirrors `Window<T>`: non-movable, `create()` →
  `std::expected<std::unique_ptr<T>, std::error_code>`, held as a `unique_ptr`
  member of the owner window; the parent destroys the child HWND (no
  `DestroyWindow`); the dtor / `WM_NCDESTROY` just `RemoveWindowSubclass`.
- **`Control<T>` + a mixin-composed control catalog (revised 2026-06-30).** The
  earlier "no pre-built controls, extract only reactively" stance is **superseded.**
  `Control<T>` stays the base, but the library now *does* ship concrete `final`
  controls, each composing the **notification mixins** it emits:
    - `Button` / `Checkbox` → `Clickable` (BN_CLICKED)
    - `Edit` → `TextChangeable` (EN_CHANGE)
    - `ComboBox` → `SelectionChangeable` (CBN_SELCHANGE)
  A mixin is one file under `mixins/`; a control is one file under `controls/`;
  the engine (`message_reflection.hpp` / `message_handler.hpp`) never changes as either grows. See
  `MIXINS.md` for the recipe. This is still **thin shells over native controls** —
  not a widget toolkit; the hard boundary below (no layout / theming) is unchanged.

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
