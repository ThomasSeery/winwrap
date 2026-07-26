# winwrap — message-loop design proposal

Design for a reusable message-pump runner, so no winwrap app has to hand-roll
`GetMessageW` / `TranslateMessage` / `DispatchMessageW` in `wWinMain`. Sits under
the roadmap gap "**Message loop** — stays app-side for the MVP … a
`run_message_loop()` free-function helper is additive later" (ROADMAP → *wifi-toggle
readiness gaps* §3). Design rationale conventions: VISION.md, both `CODE_CONVENTIONS.md`,
`windows/CLAUDE.md`.

## Status: implemented (2026-07-13)

Built and MSVC-verified in `lib/include/winwrap/message_loop.hpp` (`run()` + `quit()`)
with four passing Catch2 tests in `tests/message_loop_test.cpp`. Two deltas from the
proposal below:

- **`quit(int = 0)` was added** — the proposal's Q5 originally said *not* to (see the
  correction in Q5). Reversed: `quit()` names user *intent* over the `PostQuitMessage`
  mechanism and is a genuine ergonomic wrapper, not a thin rename. It's the everyday
  user-facing exit call (an "Exit" menu item, a tray "Quit").
- **The `-1` sentinel is named `message_failed_code`** (not `get_message_failed`).

The three proposed tests plus a fourth (close-window → `on_destroy` → `quit()` →
`run()` returns) all pass.

## (a) Recommendation

Ship a single **header-only free function `winwrap::run()` returning `int`** (the
`wWinMain` return value, `msg.wParam`), in a new concept-named header
**`winwrap/message_loop.hpp`** — joining the existing `message_dispatcher.hpp` /
`message_reflection.hpp` family. The loop is *stateless*: the window already owns
`HINSTANCE` (`GetModuleHandleW(nullptr)`) and `nShowCmd` is consumed by
`show(cmd)`, so `run()` needs no arguments and no owning object — a free function
is exactly the right shape, and an `Application`/`MessageLoop` class would be state
without a reason to exist (YAGNI, §3). `GetMessageW`'s `-1` error is **unreachable
through winwrap's own always-valid arguments** (`&msg` local, `nullptr` HWND
filter), so it is an invariant break, not a recoverable failure — guard it with a
**fail-fast** (`FAIL_FAST_IF`, the sanctioned terminate tier in `windows/CLAUDE.md`
§4), *not* `std::expected`; wrapping the overwhelmingly-successful `WM_QUIT` path in
an `expected` for a can't-happen case is ergonomic noise against the value-error
model's actual purpose (surfacing recoverable, caller-caused failures). Build
**only** the bare pump now; accelerators, modeless dialogs, and idle processing are
walled *open*, not *off* — the free-function entry admits a future defaulted
`RunConfig` overload without breaking a single caller (§1 config-struct convention,
§3 abstractions-late).

## (b) Prior art — how established Win32 wrappers structure the run loop

| Library | Entry point | Shape | Accelerators | Modeless dialogs | Idle / `-1` handling |
|---|---|---|---|---|---|
| **WinLamb** (Freitas Dias) | `RUN(cls)` macro → generates `wWinMain`; loop in `window_main::run_as_main` | free-ish, per-window | member `HACCEL` → `TranslateAcceleratorW` in-loop | `IsDialogMessageW` for dialog windows | throws on `-1`; no idle |
| **Win32++** (Nash) | `CWinApp::Run()` (singleton app object) → `InitInstance()` + `CWinThread::MessageLoop()` | heavy app+thread objects | `virtual PreTranslateMessage` calls `TranslateAccelerator` | `PreTranslateMessage` calls `IsDialogMessage` (modeless list) | `virtual OnIdle` + `PeekMessage` idle loop |
| **WTL `CMessageLoop`** (atlapp.h) | `CMessageLoop::Run()`, added to the thread | reusable loop object; filter + idle chains | each window's `PreTranslateMessage` in a **message-filter chain** | same filter chain (`IsDialogMessage`) | **ignores `-1` (`continue`)**; `OnIdle` chain + `PeekMessage` |
| **ATL core** (`CWindowImpl`) | none — wizard `_tWinMain` writes the loop inline | — | inline in generated `_tWinMain` | inline | inline |
| **MS `NotificationIcon` sample** | inline plain `GetMessage` loop | — | none | none | none (returns `msg.wParam`) |

**What every one of them returns:** `(int)msg.wParam`. That is the fixed contract
`run()` must honour.

**Reading of the field, applied to winwrap's pillars (VISION):**

- The two *frameworks* (Win32++, WTL) grow the loop into an **object with virtual
  extension points** (`PreTranslateMessage`, `OnIdle`) and, in Win32++, a
  singleton `CWinApp`. That is precisely the "replacement runtime / event system
  of its own" winwrap is a non-goal against (VISION *not*). Their machinery exists
  to solve accelerators + modeless dialogs + idle *generically for many windows* —
  a problem winwrap does not have yet (one window, no accel table, no dialogs).
- **WinLamb** is closest to winwrap's spirit (header-only, no framework) and shows
  the minimum honest loop: `TranslateAccelerator?` → `IsDialogMessage?` →
  `Translate`/`Dispatch`. But its accelerator/dialog handling is welded into the
  window object via a `RUN()` macro. winwrap can start below even that — a bare
  pump — and add the two `if`s later behind a config, because the CRTP/deducing-this
  design means the loop never needs to know the window type.
- **The `-1` split is instructive.** WinLamb throws, WTL *silently ignores*. Silent
  ignore is out (against "report outcomes faithfully"); throwing is out (winwrap is
  exception-free for library control flow, VISION pillar 5). The third option none
  of them take — **fail-fast** — is the correct one *here specifically*, because
  unlike those generic loops (which accept arbitrary filter HWNDs and so can
  legitimately see `-1`), winwrap owns both `GetMessageW` arguments and keeps them
  valid, so `-1` cannot arise from correct use.

## (c) Proposed public API + usage

```cpp
#pragma once

#include "winwrap/win.hpp"

#include <wil/result_macros.h>  // FAIL_FAST_IF -- the terminate tier (windows/CLAUDE.md §4)

namespace winwrap {

/// Runs the thread's message loop until WM_QUIT, then returns its exit code --
/// the value to hand back from wWinMain. Pumps GetMessageW/TranslateMessage/
/// DispatchMessageW; dispatched messages reach each window's WndProc (and so its
/// compile-time on_* hooks). WM_QUIT arrives from PostQuitMessage -- post it from
/// your main window's on_destroy so closing the window exits the app.
///
/// @return The WM_QUIT exit code (msg.wParam), i.e. PostQuitMessage's argument.
/// @note   GetMessageW's -1 error is unreachable here (winwrap supplies a valid
///         &msg and a nullptr HWND filter), so it is treated as an invariant
///         break (fail-fast), not a recoverable std::expected error.
[[nodiscard]] inline int run() {
    constexpr int message_failed_code{-1};
    MSG msg{};
    BOOL status{};
    while ((status = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        FAIL_FAST_IF(status == message_failed_code);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

/// Ends the message loop: posts WM_QUIT so run() returns error_code. Call it from an
/// "Exit" menu handler, a tray "Quit" item, or a window's on_destroy.
inline void quit(int error_code = 0) { PostQuitMessage(error_code); }

}  // namespace winwrap
```

Call site (the whole point — `wWinMain` shrinks to this):

```cpp
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nShowCmd) {
    auto window = MainWindow::create({.title = L"Hello, winwrap"});
    if (!window)
        return EXIT_FAILURE;
    (*window)->show(nShowCmd);
    return winwrap::run();
}
```

## (d) The eight design questions

### 1. API shape — free function `run()` → `int`

**Free function `winwrap::run()` returning `int`.** Rationale: the loop holds *no
state*. It does not need `HINSTANCE` (the window grabs it via
`GetModuleHandleW(nullptr)` — `window.hpp:163`), nor `nShowCmd` (consumed by
`WindowHandle::show(cmd)` — `window_handle.hpp:40`). A function whose only job is to
drain the queue has nothing to own, so an `Application`/`MessageLoop` class would be
ceremony. It also matches every surveyed library's return contract (`int` =
`msg.wParam`) and reads cleanly qualified: `winwrap::run()`. `[[nodiscard]]` because
dropping the value means `wWinMain` returns garbage.

*Naming:* ROADMAP tentatively wrote `run_message_loop()`; I recommend the shorter
**`run()`** — the `winwrap::` namespace already supplies the "message loop" context
(WinLamb's `RUN()` precedent), and if an `Application` object ever lands it exposes a
`.run()` method with the free function staying as the thin entry point.

### 2. Error handling — `GetMessageW` returns `-1`

**Fail-fast, not `std::expected`, not throw, not silent-ignore.** The value-error
model (VISION pillar 5) exists to surface *recoverable failures the caller can react
to* — a class that won't register, a window that won't create, caused by the
environment or by arguments the caller supplied. `GetMessageW`'s `-1` is documented
to occur only for an invalid window handle or an invalid `lpMsg` pointer — **both
supplied by `run()` itself** (`nullptr` filter, a valid local `&msg`). So `-1` is
not a recoverable, caller-caused failure; it is an invariant break with no meaningful
recovery. That is exactly the tier `windows/CLAUDE.md` §4 assigns to `FAIL_FAST_IF`
("terminate"). Returning `std::expected<int, std::error_code>` would force every
`wWinMain` to unwrap an error that cannot happen, taxing the happy path (normal exit
is `WM_QUIT` = success) for nothing — the wrong shape for this call. WTL's silent
`continue` is rejected (hides an invariant break); WinLamb's throw is rejected
(winwrap is exception-free for library control flow).

*Note:* this would be winwrap's **first use of a WIL error macro** — deliberately,
because this is the one spot that is genuinely its sanctioned tier. If you'd rather
not pull `<wil/result_macros.h>` for a single macro, the std-only equivalent is
`if (status == get_message_failed) std::abort();` with a comment — but `FAIL_FAST_IF`
is the house-blessed spelling and survives release builds (unlike `assert`).

### 3. Accelerator tables (`TranslateAcceleratorW`) — leave room, don't build

**Do not build now; the free-function shape already leaves room.** No v0.1 app
(wifi-toggle) has an accelerator table, so per §3's *second-consumer* trigger it is
premature. The extension path is clean and non-breaking: a future defaulted config
overload

```cpp
struct RunConfig { HWND target{}; HACCEL accelerator{}; };   // §1 config struct
[[nodiscard]] inline int run(const RunConfig& cfg);          // additive overload
```

whose loop inserts `if (cfg.accelerator && TranslateAcceleratorW(cfg.target, cfg.accelerator, &msg)) continue;`
before `TranslateMessage`. Existing `winwrap::run()` callers are untouched (it stays,
or forwards to `run({})`). `TranslateAcceleratorW` needs the target `HWND`, which is
why the config carries a `target` — noted now so the eventual struct is right, built
later (conventions-early / abstractions-late).

### 4. Modeless dialogs (`IsDialogMessageW`) — leave room, don't build

**Same verdict, same door.** winwrap has no dialog wrapper today, so nothing can
produce a modeless dialog HWND to filter. When one exists, `IsDialogMessageW` slots
into the *same* `RunConfig` path (a dialog-HWND list, or a registration callback) —
again before `TranslateMessage`, again additive. Building it now would be guessing
the shape before a real consumer (§3). The one thing to preserve so it *can* be added:
keep `run()` a plain loop with no assumptions, which the recommended design does.

### 5. Interaction with the `Window<T>` lifecycle — the quit story

The loop ends on `WM_QUIT`, which only ever comes from `PostQuitMessage`. **winwrap
posts it nowhere today** — so a winwrap app currently has no way to exit its own
loop. The coherent, minimal story is that *the main window posts the quit when it is
destroyed*, via the existing `on_destroy` hook (routed by the built-in `Lifecycle`
mixin):

```cpp
struct MainWindow : winwrap::Window<MainWindow> {
    static constexpr const wchar_t* window_class_name = L"MainWindow";
    void on_destroy() { winwrap::quit(); }
};
```

Full path: user clicks ✕ → `WM_CLOSE` → `DefWindowProcW` → `DestroyWindow` →
`WM_DESTROY` → `Lifecycle` fires `on_destroy()` → `winwrap::quit()`
(`PostQuitMessage`) → `GetMessageW` returns `0` → `run()` returns `0`. This is honest and matches "you
write the `on_*` hooks you need": *which* window owns the app's lifetime is app
policy, not something the library should assume (a tray tool's message-only window
outlives every visible window; a tool window must **not** quit the app).

**Why not a `QuitOnDestroy` mixin:** it would have to claim `WM_DESTROY`, but a
mixin composes *after* the built-in `Lifecycle` (first-match-wins), so it would fire
only on windows that *don't* define `on_destroy` — the exact built-in-vs-extra
overlap footgun MIXINS.md warns against. A window that defines `on_destroy` for its
own cleanup would silently never quit. So the explicit one-liner is not just simpler,
it is *more correct* than a mixin here.

**Consistency flag (not a v0.1 blocker):** `DropZone` (the premade window) does
*not* quit on close, so running it as a top-level main window would hang. That's fine
for its intended use (a drop target inside a larger app), but if a premade
*main-window* convenience is ever built, that base is where the quit-on-destroy policy
belongs — bundled, opt-in, deferred per §3.

**Correction (as built): `winwrap::quit(int = 0)` *was* added.** The proposal
originally rejected it as a thin `PostQuitMessage` rename (per the LIBRARY_CONVENTIONS
thin-wrapper rule). That was wrong. The thin-wrapper rule targets renames that hide a
name the user still needs *and add nothing* — e.g. `DragQueryFileW`, whose sentinel +
length-probe protocol you must understand either way, so the fix there was a *class*
(`Drop`) that hides the protocol, not a rename. `quit()` is the opposite: it names
user *intent* over the `PostQuitMessage` *mechanism*, and the user never needs to know
the underlying call to use it. The deciding test — **"does the user need to understand
the underlying Win32 call?"** — is *no* for `quit()` (call it, the app exits; the arg
is the exit code) and *yes* for `DragQueryFileW`. So `quit()` is a sanctioned
ergonomic verb (VISION pillar 1) and the everyday user-facing exit call (an "Exit"
menu item, a tray "Quit"); `on_destroy` calls `winwrap::quit()`, not `PostQuitMessage`
directly. (This nuance is now codified in winwrap `CODE_CONVENTIONS.md`.)

### 6. Placement & naming — `winwrap/message_loop.hpp`, header-only

**New concept-named header `winwrap/message_loop.hpp`** (§3: "cross-cutting concerns
live in concept-named shared headers"). It joins the established `message_*` family
(`message_dispatcher.hpp`, `message_reflection.hpp`) and names the concept exactly.
The runner is genuinely cross-cutting — every GUI app uses it, it belongs to no
single wrapper — so it does not go in `window.hpp`.

**Header-only, `inline`, no `.cpp`.** The function is non-template and tiny; `inline`
gives it a single definition across translation units (the `error.hpp` precedent —
`inline last_error()`). `menu.cpp` / `notify_icon.cpp` exist because those are large
non-template *classes*, not because "non-template ⇒ `.cpp`"; a ~10-line loop stays
inline. It `#include "winwrap/win.hpp"` first (the mandated `<windows.h>` preamble),
then `<wil/result_macros.h>`.

### 7. Testing — Catch2 without a live UI

The pump blocks in `GetMessageW`, but `PostQuitMessage` / `PostMessageW` queue
messages on the **current thread** synchronously, so the loop can be driven and
observed headlessly on the test thread. Proposed `tests/message_loop_test.cpp`
(add to `tests/CMakeLists.txt`):

1. **`run()` returns the quit code immediately when WM_QUIT is already queued.**
   `PostQuitMessage(42); CHECK(winwrap::run() == 42);` — `GetMessageW` sees the
   pending `WM_QUIT`, returns `0` on the first call, `run()` returns `42`. No window,
   no blocking. Proves the return-value contract.

2. **`run()` dispatches queued messages to a real WndProc before quitting.** Create
   a message-only window (`Window` with `WindowConfig{.parent = HWND_MESSAGE}`) whose
   `on_command(id)` records the id; `PostMessageW(hwnd, WM_COMMAND, 7, 0)` then
   `PostQuitMessage(0)`; `run()`; assert the handler saw `7` **and** `run()` returned
   `0`. Exercises the full `GetMessage → Translate → Dispatch → WndProc →
   dispatch_message → on_command` path with no visible UI. (Mirrors how
   `file_droppable_test.cpp` drives dispatch, but through the real pump rather than
   calling `dispatch_message` directly.)

3. **`run()` drains multiple messages in FIFO order, then exits on WM_QUIT.** Post
   `WM_COMMAND` ids `1, 2, 3` to the message-only window, then `PostQuitMessage(0)`;
   assert the recorded ids are `{1,2,3}` (everything before `WM_QUIT` was delivered)
   and `run()` returned `0`. Confirms the loop pumps until quit, not just once.

*Test hygiene:* each test consumes its own `WM_QUIT` (the loop exits and the flag is
cleared), so tests don't leak queue state into each other; keep them in one TU. This
is the "Catch2 tests that exercise behaviour without a live message pump" ROADMAP
item — turned inside out: we *do* run the pump, but headlessly and deterministically.

### 8. YAGNI — what is deliberately NOT built now

Per §3 (no second consumer yet; wifi-toggle needs only a bare pump) and VISION (not
a framework), the MVP is the free function + fail-fast and nothing else. Explicitly
deferred, each additive-without-a-break behind the free-function / `RunConfig` door:

- **Accelerator tables** (Q3) — no app has one.
- **Modeless-dialog `IsDialogMessageW`** (Q4) — no dialog wrapper exists.
- **`PeekMessage` idle / game-loop variant + `OnIdle` chain** (WTL/Win32++) — no
  continuous-render or background-work consumer; the blocking `GetMessage` pump is
  correct and power-friendly for event-driven desktop tools.
- **`PreTranslateMessage` filter chains** (WTL/Win32++) — machinery for routing
  accel/dialog across many windows; solves a problem winwrap doesn't have.
- **An `Application` / `CWinApp` singleton** owning `HINSTANCE`/`nShowCmd` — the
  window already owns both; the object would be stateless ceremony.
- **Nested / modal message loops** (`DialogBox`-style) — out of scope until a modal
  dialog wrapper exists.

## (e) Chosen header + placement (summary)

- **File:** `lib/include/winwrap/message_loop.hpp` (new).
- **Symbol:** `winwrap::run()` → `int`, `[[nodiscard]]`, `inline`, header-only.
- **Includes:** `"winwrap/win.hpp"` first, then `<wil/result_macros.h>`.
- **No `.cpp`.** No change to any existing header (purely additive).

## (f) Catch2 test plan (summary)

New `tests/message_loop_test.cpp`, three cases (Q7): (1) pre-queued `WM_QUIT` →
`run()` returns its code; (2) posted `WM_COMMAND` to a message-only window is
dispatched to `on_command` before quit; (3) FIFO drain of several messages then
quit. Register the file in `tests/CMakeLists.txt`'s `add_executable(winwrap_test …)`
list.

## (g) Step-by-step implementation plan (you write it) — ✅ done (2026-07-13)

> Completed as described, with the two deltas in *Status* above (`quit()` added;
> sentinel named `message_failed_code`) and a fourth test (close → exit). Steps 3/5
> below use `winwrap::quit()` rather than a raw `PostQuitMessage(0)`.

1. **Create the header.** `lib/include/winwrap/message_loop.hpp` — `#pragma once`,
   `#include "winwrap/win.hpp"`, then `#include <wil/result_macros.h>`, `namespace
   winwrap {`. Write the Doxygen `///` brief + `@return`/`@note` on `run()` (public
   API ⇒ documented, per `cpp/CODE_CONVENTIONS.md`).
2. **Write `run()`.** Match the sketch in (c): the `constexpr int get_message_failed
   {-1};` named constant (no bare `-1` at the call site, per "named constexpr at its
   contextual boundary" — function-local, its only user), the `while ((status =
   GetMessageW(...)) != 0)` loop with braces (multi-statement body), `FAIL_FAST_IF`,
   `TranslateMessage` + `DispatchMessageW`, and `return static_cast<int>(msg.wParam)`.
   Decide fail-fast vs the std-only `abort` fallback from Q2.
3. **Give a window a way to quit.** In your exercise/main window add
   `void on_destroy() { PostQuitMessage(0); }` (Q5). This is the piece that makes the
   loop terminable.
4. **Wire the call site.** Replace the hand-rolled loop in `hello-window`'s (or
   wifi-toggle's) `wWinMain` with `return winwrap::run();` per (c). Confirm the app
   still opens and — the new part — *closes cleanly* (✕ exits the process, no hang).
5. **Add the tests.** Write `tests/message_loop_test.cpp` with the three cases from
   (f); add it to `tests/CMakeLists.txt`. For cases 2–3, reuse the message-only-window
   fixture idea (`WindowConfig{.parent = HWND_MESSAGE}`) and record ids in an
   `on_command`.
6. **Build & verify (MSVC is ground truth).** Configure + build the `dev` preset,
   run `winwrap_test`; expect all green. Then eyeball the running app for the
   open-and-close-cleanly behaviour from step 4 (the pump can't be fully judged by
   tests alone).
7. **Update the docs.** Tick ROADMAP → *wifi-toggle readiness gaps* §3 (message loop)
   as done and note the header; add `run()` to any public-API listing. Leave the
   accelerator/modeless/`RunConfig` extension notes recorded as future-additive so the
   door stays documented.
```
