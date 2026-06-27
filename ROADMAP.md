# winwrap — roadmap

What's done, what's next, and the spec for each piece. Design rationale lives in
[VISION.md](VISION.md).

## ▶ Next session: start here

**Goal: an MVP usable in wifi-toggle** = `Window` (done) + `Menu` + `NotifyIcon`,
composing through `handle_message`. Design is locked (see *Decisions locked* →
*Tray / menu integration*) — don't re-litigate; build straight against it.

1. **`examples/hello_window`** — `MainWindow : public Window<MainWindow>` +
   `WinMain` + message loop. Proves `Window<T>` runs; introduces `GetMessage` /
   `TranslateMessage` / `DispatchMessage` (the part `create()` doesn't own).
2. **Task 2 — `Menu`** (spec below).
3. **Task 3 — `NotifyIcon`** (spec below) — the tray icon.
4. **`examples/tray_app`** — window + tray + menu end-to-end. The wifi-toggle shape
   minus the wifi logic; proves the whole MVP before betting wifi-toggle on it.
5. **Wire into wifi-toggle** — swap its hand-rolled Win32 for winwrap.
6. **Tag `v0.1`** — so wifi-toggle pins a tag (no surprise BC).

## Current state (v0.1)

- **`Window<T>`** — ✅ **Done.** Configurable `create(WindowConfig)`, two-layer
  registration/creation, `std::expected` error model, `configure_class` hook, and
  the `WM_NCDESTROY` lifetime fix. Builds clean (MSVC `/W4`), clang-tidy-clean,
  public API documented with Doxygen `///`. *Not yet exercised by a running app.*
- **`Menu`** — declares `add_item` / `show`, owns its `HMENU` via
  `wil::unique_hmenu`. Methods not yet implemented (`menu.cpp` is empty). ← **Task 2**
- **`NotifyIcon`** — header is an empty namespace. Not started. ← **Task 3**
- **Build** — CMake + WIL + install/export + warnings/sanitizers. Solid; no work
  needed.

## Decisions locked

- **Build our own CRTP window bridge** — not ATL `CWindowImpl`. (See VISION → the
  reuse rule.)
- **Window config via a `WindowConfig` struct** + C++20 designated initializers —
  not a builder, not static traits. Drops the variadic/forwarding from `create()`.
- **Errors as `std::expected<T, std::error_code>`** at the public API; Win32 codes
  via `std::system_category()`. WIL stays for RAII handles only, not control flow.
- **Class-level config** (the `WNDCLASS`) is customized by a `configure_class`
  hook the derived type shadows — same mechanism as `handle_message`.
- **Doc comments:** Doxygen `///` + `@`-commands on the **public API** only;
  one-line `if` bodies drop braces. (Both codified in `CODE_CONVENTIONS.md`.)
- **Lint carve-outs** (`.clang-tidy`): `#pragma once` allowed; `reinterpret-cast`
  and `int-to-ptr` allowed (inherent to native Win32). Propagated to the
  `newcpp.ps1` scaffold — pragma-once in the base template, the Win32 casts in the
  `-Windows` variant.

### Tray / menu integration (locked Stage-0, governs Tasks 2–3)

- **Tray model:** `NotifyIcon` **attaches to a `Window<T>` you own** — takes the
  owner's `HWND` + a callback message id; tray events arrive at that window's
  `handle_message`. *Not* a self-contained callback object — one event path, no
  parallel `std::function` layer. A `TrayApp` convenience bundle can be added later,
  additively.
- **Menu routing:** `Menu::show` posts **`WM_COMMAND` to the owner window**, handled
  in `handle_message` — not `TPM_RETURNCMD`. All events on one path.
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

## Task 2 — `Menu`: implement it

1. **`add_item(UINT id, const wchar_t* text)`** → `AppendMenuW(handle_.get(), MF_STRING, id, text)`.
2. **`show(HWND owner)`** → `TrackPopupMenuEx`, with the `SetForegroundWindow(owner)`
   gotcha (so the menu dismisses on click-away).
3. **Click routing — LOCKED:** `show` posts **`WM_COMMAND` to the owner**, routed
   through `handle_message` (not `TPM_RETURNCMD`). Keeps all events on one path.
4. Mirror the **factory pattern** if construction can fail: `CreatePopupMenu`
   returning null in the constructor can't surface as `expected`, so consider a
   static `Menu::create() -> std::expected<Menu, std::error_code>` and a private
   ctor.

## Task 3 — `NotifyIcon`: the differentiator

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

## Backlog (future)

- **C++17 backport** via preprocessor feature-gating + `nonstd::expected`. Keep
  the API backport-friendly meanwhile.
- Balloon / toast notifications.
- A typed `TrayEvent` enum over the raw tray callback message (additive).
- A `TrayApp` convenience bundle (message-only window + `NotifyIcon`), additive.
- A ctor-arg-forwarding `create` overload (once `std::forward` is on the menu).
- More RAII-wrapped Win32 objects as real projects need them.
- Child windows / control wrappers (only if a real use case demands it).
- Catch2 tests that exercise behaviour without a live message pump.
