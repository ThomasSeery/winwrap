# winwrap — code conventions

Conventions specific to **this library's public API**. They layer on top of, and
never override:

- `cpp/CODE_CONVENTIONS.md` — naming (`name_`, snake_case), `{}` init, Doxygen `///` style.
- `cpp/windows/CLAUDE.md` — Win32 house style (Unicode `…W`, WIL RAII, error model).

Where those say *how to write C++*, this says *how winwrap shapes its API*.

## 1. Multi-argument factories take a `*Config` struct, not positional parameters

When a public factory (`create`, or any public function) takes **more than ~2
arguments**, or **any two arguments of the same type** that a caller could
transpose, gather them into a `XConfig` struct passed by `const&`, and call it
with C++20 **designated initializers**.

```cpp
// Don't: positional, and the two UINTs are silently swappable.
static std::expected<NotifyIcon, std::error_code>
create(HWND owner, UINT callback_msg, UINT id, HICON icon, const wchar_t* tooltip);

// Do: named fields at the call site, swap-proof and extensible.
struct NotifyIconConfig { HWND owner{}; UINT callback_msg{}; UINT id{}; /* … */ };
static std::expected<NotifyIcon, std::error_code> create(const NotifyIconConfig& cfg);

NotifyIcon::create({.owner = hwnd(), .callback_msg = tray_callback, .id = 1, /* … */});
```

**Why:**

- **Swap-proofing.** Adjacent same-typed scalars (two `UINT`s, two `int`s) can be
  transposed with no compile error and no crash — a silent bug. `clang-tidy`'s
  `bugprone-easily-swappable-parameters` flags it, but we disable that check
  project-wide (it fires across every Win32 wrapper). The config struct fixes the
  *real* hazard by construction; named fields can't be swapped.
- **Self-documenting call sites.** `.callback_msg = …` beats a bare positional `1`.
- **Extensible.** Add a field (with a default) without breaking existing callers.
- **Free defaults.** Omitted fields take the in-struct default.

**Scope:** the **public** surface. A **private** constructor or internal helper may
stay positional — it has a single call site and isn't exposed.

**Precedent:** `WindowConfig` → `Window::create`, `NotifyIconConfig` → `NotifyIcon::create`.

## See also

- `VISION.md` — design pillars (CRTP, value-based errors, WIL-only deps).
- `ROADMAP.md` — locked decisions (incl. the `WindowConfig` choice this generalises)
  and the `.clang-tidy` lint carve-outs.
