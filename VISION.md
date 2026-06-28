# winwrap — vision

What winwrap is, what it deliberately isn't, and why it exists. The user-facing
"what + how to build" lives in [README.md](README.md); the work queue lives in
[ROADMAP.md](ROADMAP.md); this file is the design north star.

## The one-liner

**Modern C++ (C++23, MSVC) over *native* Win32 — a thin wrapper, not a
replacement runtime.** winwrap still calls `CreateWindowExW`, `Shell_NotifyIcon`,
`TrackPopupMenuEx` directly. It never puts its own world between you and the OS
the way Qt / wxWidgets / GTK do.

## Design pillars

1. **Native Win32, never a framework.** Every wrapper exposes the raw handle and
   you can always drop to plain Win32. winwrap adds ergonomics; it never hides
   the platform or locks you in.
2. **Zero-overhead dispatch via CRTP.** Window message routing resolves at
   compile time — `static_cast<T*>` + `if constexpr`/`requires` detection of named
   `on_*` hooks the derived window defines — no vtables, no virtual hierarchy, no
   macro message maps. You write only the `on_*` handlers you need.
3. **Low / zero dependencies.** Header-only WIL for RAII handles, and nothing
   else. No multi-megabyte runtime to ship next to a tray utility.
4. **RAII over every resource.** Handles are owned by wrappers — WIL's
   (`unique_hmenu`, `unique_hicon`, …) for the plumbing, winwrap's ergonomic
   types layered on top. No manual `CloseHandle` / `DestroyWindow` ladders.
5. **Value-based errors.** The public API returns
   `std::expected<T, std::error_code>` (Win32 codes via `std::system_category()`)
   — explicit, exception-free-friendly, the right shape for a library. Exceptions
   are for genuine programmer errors only.
6. **An integrated system-tray icon — the differentiator.** A reusable
   `Shell_NotifyIcon` abstraction that rides the *same* CRTP window bridge (tray
   events arrive as ordinary window messages). This window + tray combination, in
   modern standalone C++, is the gap winwrap fills.
7. **Unicode, MSVC.** UTF-16 at the Win32 boundary, the `…W` APIs, `/utf-8` for
   narrow literals. Classic Win32 desktop — not WinRT/UWP.

## What winwrap is *not* (non-goals)

- **Not a from-scratch GUI toolkit.** No custom widget tree, no layout engine, no
  theming / custom-drawn widgets. *Ergonomic wrappers over **native** Win32 controls*
  (`Button`, `Edit`, …) are in scope from v0.2 — but they stay **thin shells over the
  OS controls**: winwrap supplies the ergonomics (`button.on_click(...)`), Windows
  supplies the widget. The line is "native controls made pleasant," never a
  Qt/wxWidgets replacement.
- **Not Qt / wxWidgets / GTK.** No replacement runtime, no event system of its
  own, no cross-platform abstraction.
- **Not cross-platform.** Windows only, on purpose.
- **Not WinRT / UWP / C++/WinRT.** Classic Win32.
- **No ANSI / TCHAR dual builds.** Always Unicode.

## Why it exists — the landscape

The open-source field splits into two camps that never overlap: native-Win32
**window frameworks** with no tray, and **tray-only** libraries with no window
framework. None of the window frameworks use CRTP either.

| Library          | Native Win32 | Dependencies   | Dispatch              | Tray icon |
|------------------|:-----------:|----------------|-----------------------|:---------:|
| WinLamb          | yes         | header-only     | lambda-map (runtime)  | no        |
| Win32++ (Win32xx)| yes         | header-only     | virtual `WndProc`     | no        |
| LFWin32          | yes         | low             | signal-slot (no vtables) | no     |
| ATL `CWindowImpl`| yes         | ATL framework   | CRTP **+ macro maps** | no        |
| zserge/tray, traypp, CTrayNotifyIcon | — | varies | callbacks    | yes, but no window framework |
| **winwrap**      | **yes**     | **WIL only**    | **CRTP (compile-time)** | **yes** |

The closest existing thing — ATL's `CWindowImpl` plus a third-party tray class —
drags in the ATL framework, macro message maps, and a dated API, and still
doesn't bundle the tray. winwrap is that combination done modern, clean, and
standalone.

## The reuse rule

The principle that decides build-vs-adopt for every piece:

> **Reuse** a library when it gives you the thing *cleanly* — no baggage you're
> trying to avoid. **Build** only when the sole existing version drags that
> baggage in.

- **RAII handles** → WIL provides them cleanly → reuse. (e.g. `Menu` owns its
  `HMENU` via `wil::unique_hmenu`.)
- **The CRTP window bridge** → the only implementation is ATL's, welded to the
  ATL framework + macros + no tray → build it. This is the one justified
  hand-roll, and it's the heart of the library.
- **The tray abstraction** → nothing modern and standalone exists → build it.
  It's the differentiator.

## Future directions (not now)

- **C++17 backport.** Keep the design backport-friendly so a future update can
  reach down to C++17 via preprocessor feature-gating + `nonstd::expected`
  (expected-lite) where `std::expected` is absent. Deliberately deferred.
- More RAII-wrapped Win32 objects as the need recurs across real projects.
- Balloon / toast notifications, `NOTIFYICON_VERSION_4` message protocol.
- **Ergonomic native-control wrappers** (`Button`, `Edit`, …) — v0.2: thin shells
  over the OS control classes with on-event callbacks (`button.on_click(...)`),
  hiding the `WM_COMMAND`-id plumbing. Native controls only — no layout/theming.
