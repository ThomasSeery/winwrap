# Pre-v0.1 punch list — one task per fresh session

Each section below is a self-contained prompt: open a fresh context, point it at
one section, finish, commit, next. Ordered so earlier tasks unblock later ones.
Structural tasks (1–5) come first — each turns from "edit" into "breaking
change" if left until after the `v0.1` tag. Additive features (6–8) follow.

> **Session zero (before any of these):** commit the currently-uncommitted drop
> batch (`Drop`, `DropZone`, docs, the constants convention in
> `cpp/CODE_CONVENTIONS.md`) so every task starts from a clean tree.

> **Working style, every session:** per `cpp/CLAUDE.md`, offer Tommy the
> learning-vs-delivery choice up front. Teach anything off `WHAT_I_KNOW.md`
> before using it. MSVC is ground truth (clangd shows phantoms):
> `cmd //c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build build/dev --target winwrap_test'`
> then run `build\dev\bin\winwrap_test.exe`.

---

## 1. `WW_CASE` surgery — return values + signature hardening, ONE pass

**Why first:** the only remaining known engine change. Two parked items both
require changing the macro's shape and touching every call site; doing them
together is one churn instead of two, and afterwards the dispatch engine is
genuinely frozen.

**Read first:** `lib/include/winwrap/mixins.hpp` (the macro + umbrella),
`TECH_DEBT.md` → *Message dispatch — known edges* (both items),
`ROADMAP.md` → *Dispatch design review* → follow-up (a) (the hardening spec).

**Part A — lift the `return 0` ceiling.** Today every handled message returns 0,
so hooks can't produce a meaningful `LRESULT` (blocks future `WM_CTLCOLOR*` /
`WM_NOTIFY`-style mixins). Design question to settle: detect the hook's return
type (`void` → return 0; `LRESULT`-convertible → return it) inside the macro /
a helper, vs a second macro variant for value-returning hooks. Simplest correct
answer wins; don't gold-plate for messages that don't exist yet — but the macro
signature should not need to change *again*.

**Part B — the hardening assert.** Extend the macro to
`WW_CASE(message, hook, call)`; in the compiled-out `if constexpr` branch:
`static_assert(!requires { &std::remove_cvref_t<decltype(self)>::hook; },
"winwrap: '" #hook "' exists but doesn't match the hook signature")`. Notes
from a prior attempt: the macro then depends on the enclosing handler's explicit
object parameter being named `self` (document that in the macro doc comment);
`mixins.hpp` needs `<type_traits>`; `Commandable` is macro-free and gets the
same assert by hand. Known limits (record, don't fix): typo'd names, overloaded
hooks, and private hooks still compile out silently.

**Touches:** the macro + all ~12 `WW_CASE` call sites across the hook mixins +
`Commandable`. Update the TECH_DEBT entries (mis-signature half → done) and the
ROADMAP follow-up (a) → done.

**Acceptance:** suite green; a scratch TU with a wrong-signature `on_paint`
fails to compile with the winwrap message; a hook returning a value reaches the
caller as the `LRESULT`.

---

## 2. Umbrella headers — decide keep vs drop (public-surface decision)

**Read first:** `TECH_DEBT.md` → *Umbrella headers*. `controls.hpp` and
`mixins.hpp` are pure include-aggregators — except `mixins.hpp` also owns the
`WW_CASE` define/undef window, which is a real reason it must exist in some
form. Deciding now is free; deleting an umbrella after v0.1 breaks includers.
Outcome is a decision recorded in TECH_DEBT/ROADMAP (+ deletion if that wins).
Note the one-letter `control.hpp`/`controls.hpp` readability trap in the
discussion.

---

## 3. `__declspec(empty_bases)` on `MessageDispatcher`

**Read first:** `TECH_DEBT.md` → *MSVC empty-base bloat* (measured: 24 vs 16
bytes per window). Apply behind a small portability macro (MSVC-only attribute;
GCC/Clang need nothing) in `message_dispatcher.hpp`, or explicitly decide to
only soften the mixins.hpp "zero size" doc comment instead. Layout change ⇒ do
before the tag. Verify with a `sizeof` probe (a prior session's probe lives in
the 2026-07-12 session scratchpad pattern: `static_assert`/printf on
`sizeof(Window<T>)`).

---

## 4. Behavior tests for the seven untested hook mixins

**Why:** only `FileDroppable` has synthetic-message behavior tests; the rest are
compile/link only. These tests lock the frozen engine and are the only net that
catches typo'd hooks (the half the static_assert can't).

**Pattern to copy:** `tests/file_droppable_test.cpp` — construct the test window
directly (no pump needed), call `dispatch_message(msg, wparam, lparam)`, assert
the hook fired / fell through. Most messages are easier than drops (no memory
fabrication): `WM_SIZE` packs width/height in lparam, `WM_KEYDOWN` vk in wparam,
`WM_SETFOCUS`/`WM_KILLFOCUS`, `WM_MOUSEMOVE` x/y via lparam, lifecycle
messages, `WM_COMMAND` with `lparam == 0` for `Commandable` (mind the
lparam-split invariant). One test file per mixin is overkill — group sensibly.
Also assert the no-hook fall-through (a `BareWindow` with no hooks →
`default_proc`).

---

## 5. clang-tidy pass over the new drop files

`drop.hpp`, `drop_zone.hpp`, and the touched tests postdate the last tidy run.
Apply the lint-triage policy (`cpp/CODE_CONVENTIONS.md`): fix legitimate hits,
suppress-with-comment the wrong ones. One known expected hit:
`DropZone::on_drop` public data member trips
`cppcoreguidelines-non-private-member-variables-in-classes` — suppress; public
callback members are the API by design (`Clickable` precedent).

---

## 6. `NotifyIcon::set_icon` *(additive — blocking for wifi-toggle)*

Spec already in `ROADMAP.md` → *wifi-toggle readiness gaps* item 1: `NIM_MODIFY`
with `uFlags = NIF_ICON`, identity = the original `(hWnd, uID)`; the shell does
NOT take ownership of the `HICON`, so swap the owned `wil::unique_hicon` member
only after the call succeeds; no `NIM_SETVERSION` re-send. `std::expected<void,
std::error_code>` return, per the error model.

## 7. `Timable` mixin *(additive)*

`WM_TIMER` → `on_timer(UINT_PTR id)` (id = wparam), per the window-mixin recipe
in `MIXINS.md` (FileDroppable is the worked example — this one is simpler: no
payload unpack). `SetTimer`/`KillTimer` stay app-side for now (survey precedent
in ROADMAP). Include a behavior test (task 4 pattern).

## 8. Wire wifi-toggle, then tag `v0.1`

The finale: swap wifi-toggle's hand-rolled Win32 for winwrap — message-only
window (`WindowConfig{.parent = HWND_MESSAGE}`), `NotifyIcon` (+ `set_icon`),
`Menu` with per-item callbacks (acceptance: zero menu ids, zero `switch`, zero
`on_command` in the app), `Timable` for the poll, app-side `GetMessage` loop.
First on-screen exercise of the tray stack. Then tag `v0.1` so the app pins it.
