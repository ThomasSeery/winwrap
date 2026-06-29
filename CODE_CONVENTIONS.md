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

## 2. Factory & builder naming — `create` vs `make_*`

The verb a function picks tells the reader its contract. The deciding question is
**"can it fail / does it own a resource?"** — *not* whether it's public or private.

- **`create`** — a named constructor that **acquires an OS resource and can fail**,
  so it returns `std::expected<T, std::error_code>` (a real constructor can't). The
  public factory is `create`; its private worker that does the actual acquisition
  takes the same family with a suffix, `create_<thing>`.
- **`make_*`** — a helper that **builds a plain value and cannot fail**; it returns
  the value by value, never an `expected`. This is the standard-library idiom
  (`std::make_pair`, `make_tuple`, `make_optional`).

```cpp
static std::expected<NotifyIcon, std::error_code> create(const NotifyIconConfig&); // acquires + can fail
std::expected<void, std::error_code>              create_control(const ControlConfig&); // private worker
NOTIFYICONDATAW                                   make_data() const noexcept;        // builds a value, can't fail
```

Public/private is only a *correlation*: resource factories are usually the public
entry points and value-builders are usually private helpers — but the name follows
the **failability + ownership**, not the visibility. (`create_window` /
`create_control` are private yet correctly `create_*`: they acquire a window and
can fail.)

**Precedent:** `Window::create` / `NotifyIcon::create` (fallible factories),
`create_window` / `create_control` (private workers), `make_data` (infallible builder).

## 3. Where shared code lives — conventions early, abstractions late

As the library grows, decide *where* a thing belongs by what conceptually needs it
— and extract shared code **reactively**, never speculatively.

- **Per-resource headers own their specifics.** Anything only one wrapper needs
  stays in that wrapper's header, even if the underlying mechanism *looks* generic.
  `notify_icon.hpp` owns `taskbar_created_message()` — only tray icons care about
  the "TaskbarCreated" broadcast.
- **Cross-cutting concerns live in concept-named shared headers.** A utility every
  wrapper uses goes in a focused header named for the concept. `error.hpp`
  (`last_error`, `checked`) is the precedent. Future shared concepts each get their
  own header (e.g. a reserved `shell.hpp` for shell/taskbar integration commons).

**The move trigger — the second real consumer.** Keep a thing local until a
*second* wrapper genuinely needs it; only then lift it into the appropriate shared
header. This is `LIBRARY_CONVENTIONS.md`'s reactive-extraction rule applied inside
winwrap: a one-caller "utility" is premature abstraction — you'll guess the shape
wrong before you've seen two real uses.

Set the **convention** (the destination + naming) early so there's no sprawl and the
eventual move is mechanical; do the **extraction** late, when the trigger fires. Worked
example: `taskbar_created_message()` stays on `NotifyIcon` today, but its reserved home
is a future `shell.hpp` — so if an `ITaskbarList3` wrapper ever shares the
Explorer-restart concern, it moves there with no redesign.

## See also

- `VISION.md` — design pillars (CRTP, value-based errors, WIL-only deps).
- `ROADMAP.md` — locked decisions (incl. the `WindowConfig` choice this generalises)
  and the `.clang-tidy` lint carve-outs.
