# winwrap — CONTROL_PLAN (v0.2 Controls)

Implementation plan for the **v0.2 Controls** layer: `Control<T>`, a CRTP base for
native child controls that you **subclass to customise**, mirroring `Window<T>`.
Self-contained — a fresh session should build this from this file + the repo.
Rationale in [VISION.md](VISION.md); surrounding work in [ROADMAP.md](ROADMAP.md)
(§ *v0.2 — Controls*), which this plan governs.

> **Status: ✅ implemented & build-verified** (2026-06-28). `Control<T>` is complete in
> `lib/include/winwrap/control.hpp` (`create` / `create_control` / `set_text` / `enable`
> / `handle_message` / `subclass_proc` / `~Control`) and compiles + links under MSVC via
> `tests/control_test.cpp`. **Not yet exercised on screen** — a live control needs a
> window + message pump (§8); that's the one remaining verification.

> **Independent of the v0.1 app track — not gated.** `Control<T>` is winwrap
> *library* work: it shares no code with `tray_app` or wifi-toggle and links nothing
> they touch, so build it before, after, or alongside them — order by priority, not
> dependency. (An earlier "ship v0.1 first" line here was a focus call mistaken for a
> constraint; dropped so it can't hold this back again.)

> **Supersedes** the earlier runtime-callback design (thin `Button` instances +
> a `CommandRouter` + a `Window<T>` routing edit). That is **rejected**. There is
> **no `CommandRouter`** and **no edit to `window.hpp`** in this plan.

---

## 1. The design — `Control<T>` is `Window<T>`'s sibling

A control is a `WS_CHILD` window of an OS-registered class (`"BUTTON"`, `"EDIT"`,
`"STATIC"`, …). `Control<T>` wraps one as a **CRTP base you derive**, exactly like
`Window<T>`:

```cpp
class MyButton : public winwrap::Control<MyButton> {
public:
    static constexpr const wchar_t* control_class = L"BUTTON";
    void on_paint();            // compile-time hook, shadowed like Window<T>'s
    void on_mouse_move(int x, int y);
};
```

Dispatch resolves at compile time via `if constexpr (requires { self->on_x(); })`
— **the same mechanism `window.hpp` already uses**, no virtuals, no vtables. The
difference from `Window<T>` is only *where the WndProc comes from*: a top-level
window **registers its own class** and owns the proc; a control belongs to a
**system class whose proc the OS owns**, so to customise it you splice your proc
into the control's message stream via **subclassing** (§3).

**In scope:** intercepting a control's own messages — custom paint / hover /
keyboard / focus (i.e. owner-draw and custom behaviour *is* the point here).
**Out of scope (hard boundary):** no layout engine, no theming framework, no
widget toolkit. Native controls + the subclass building block only — not a
Qt/wxWidgets replacement.

| | `Window<T>` (v0.1, done) | `Control<T>` (v0.2) |
|---|---|---|
| What you write | `class W : public Window<W>` | `class C : public Control<C>` |
| Window source | `RegisterClassW` + your `window_proc` | `CreateWindowExW(L"BUTTON", …)` — a system class |
| Message bridge | your `WndProc` via `GWLP_USERDATA` | `SetWindowSubclass`, `this` via `dwRefData` |
| Default handler | `DefWindowProcW` | `DefSubclassProc` |
| Dispatch core | `handle_message` + `if constexpr` hooks | **identical** |
| Teardown | `DestroyWindow` | `RemoveWindowSubclass` (parent destroys the HWND) |

The dispatch core is the same — you've already written it. Subclassing is the
new ~20%.

---

## 2. Mechanism — subclassing (NEW; teach before relying on it)

The system owns the `"BUTTON"` class's WndProc (it draws the button, tracks
clicks). To customise, you intercept its messages by inserting your own procedure
ahead of the real one. The modern API (comctl32, `<commctrl.h>`):

```cpp
// install: your proc runs first; `this` rides in the last arg (dwRefData)
SetWindowSubclass(hwnd, &subclass_proc, kSubclassId, reinterpret_cast<DWORD_PTR>(this));

// inside subclass_proc, for anything you don't handle:
DefSubclassProc(hwnd, msg, wparam, lparam);   // = "let the real button do it"

// detach (teardown):
RemoveWindowSubclass(hwnd, &subclass_proc, kSubclassId);
```

- `DefSubclassProc` is the control's `DefWindowProcW` — falls through to the
  original system proc, so you only override the messages you care about.
- `this` is recovered from the `dwRefData` param the system hands back to
  `subclass_proc` — **cleaner than `Window<T>`'s `WM_NCCREATE` dance, and you must
  NOT use `GWLP_USERDATA` on a control** (the control may use it itself).
- `kSubclassId` is a small constant `UINT_PTR` identifying this subclass on the
  window (use e.g. `1`).

`subclass_proc` signature:
```cpp
static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l,
                                      UINT_PTR id_subclass, DWORD_PTR ref_data);
```

---

## 3. Decisions locked (recommended; confirm or override)

- **C1 — `Control<T>` CRTP base, mirrors `Window<T>`.** Concrete controls are
  *derived* (`class MyButton : public Control<MyButton>`); `T` provides
  `static constexpr const wchar_t* control_class` and is default-constructible.
- **C2 — `this`-recovery via `SetWindowSubclass` `dwRefData`**, never
  `GWLP_USERDATA` (don't stomp the control's own slot).
- **C3 — lifetime mirrors `Window<T>`:** **non-movable** (all four special members
  `= delete`d — the subclass holds a pointer to the object, so it can't move).
  `create()` returns `std::expected<std::unique_ptr<T>, std::error_code>` (heap →
  stable address for that pointer; factory can fail). Held as a `unique_ptr`
  **member of the derived window**, created in `on_created()`. The child HWND is
  destroyed by the parent — **no `DestroyWindow`**; the dtor only
  `RemoveWindowSubclass` if `hwnd_` is still live, and `WM_NCDESTROY` removes the
  subclass + nulls `hwnd_` (same shape as `Window<T>`'s `WM_NCDESTROY` fix).
- **C4 — click semantics:** a button's `BN_CLICKED` is sent to the **parent** as
  `WM_COMMAND` → the window's existing `on_command(id)`. **Handle clicks there,
  keyed by `id`.** `Control<T>` intercepts the control's *own* messages (paint,
  mouse, key, focus). "Click handler *on the control object*" needs message
  reflection (parent forwards `WM_COMMAND` to the child) — **future additive
  layer, not v1.** This is why there's no `CommandRouter` and no `window.hpp` edit.
- **C5 — id is user-supplied** (in `ControlConfig`), consistent with `Menu` ids —
  the app picks it so it can match in `on_command`.
- **C6 — starter hook set, kept lean:** `on_paint()`, `on_mouse_move(x,y)`,
  `on_lbutton_down(x,y)` / `on_lbutton_up(x,y)`, `on_key_down(WORD vk)`,
  `on_focus(bool gained)`. Add more as needed; `handle_message` stays **shadowable**
  as the escape hatch for anything not covered (mirror `Window<T>`'s note).

---

## 4. Public API (`winwrap/control.hpp` — header-only, it's a template)

```cpp
namespace winwrap {

/// Per-control settings for Control<T>::create.
struct ControlConfig {
    HWND parent;                 ///< Owner window (gets the control's WM_COMMAND).
    UINT id;                     ///< Control id; match it in the owner's on_command.
    const wchar_t* text{L""};
    int x{0}, y{0}, width{80}, height{24};
    DWORD style{0};              ///< Extra control styles; WS_CHILD|WS_VISIBLE are added.
};

/// CRTP base for a native child control. Derive as
/// `class MyButton : public Control<MyButton>`: the base owns creation, the
/// SetWindowSubclass->object bridge, and teardown; T provides
/// `static constexpr const wchar_t* control_class` and shadows on_* hooks.
/// Dispatch resolves at compile time -- no virtual. Non-movable; lives as a
/// unique_ptr member of the owner window. @tparam T the derived control type.
template <typename T>
class Control {
public:
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    Control(Control&&) = delete;
    Control& operator=(Control&&) = delete;

    /// Creates the child control (and sets the default GUI font), or the Win32 error.
    [[nodiscard]] static std::expected<std::unique_ptr<T>, std::error_code>
    create(const ControlConfig& cfg);

    [[nodiscard]] HWND hwnd() const { return hwnd_; }
    [[nodiscard]] UINT id() const { return id_; }

    void set_text(const wchar_t* text);   ///< SetWindowTextW.
    void enable(bool enabled);             ///< EnableWindow.

    /// Routes a message to the matching on_* hook T defines (else DefSubclassProc).
    /// Shadow this in T for messages the named hooks don't cover.
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

protected:
    Control() = default;
    ~Control();   ///< RemoveWindowSubclass if hwnd_ is still live. No DestroyWindow.

private:
    std::expected<void, std::error_code> create_control(const ControlConfig& cfg);
    static LRESULT CALLBACK subclass_proc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

    HWND hwnd_{};
    UINT id_{};
};

}  // namespace winwrap
```

Hooks are detected with `if constexpr (requires { self->on_x(); })` in
`handle_message` — copy the shape straight from `window.hpp`.

---

## 5. Implementation steps (ordered)

1. **`control.hpp`** scaffold — the class above; includes `<windows.h>` (lean +
   `NOMINMAX`, as the other headers), `<commctrl.h>`, `<expected>`, `<memory>`,
   `<system_error>`, `"winwrap/error.hpp"`.
2. **`create()`** — `auto self = std::unique_ptr<T>{new T{}};` then
   `create_control(cfg)`; on failure `return std::unexpected(...)`; else
   `return self;`.
3. **`create_control()`**:
   - `HWND h = CreateWindowExW(0, T::control_class, cfg.text, WS_CHILD | WS_VISIBLE | cfg.style,
     cfg.x, cfg.y, cfg.width, cfg.height, cfg.parent,
     reinterpret_cast<HMENU>(static_cast<UINT_PTR>(cfg.id)),
     GetModuleHandleW(nullptr), nullptr);` → `if (!h) return std::unexpected(last_error());`
     (the id rides in the `hMenu` param — that's how Win32 stamps a control id).
   - `hwnd_ = h; id_ = cfg.id;`
   - **Font (the un-ugly fix, G-font):**
     `SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);`
   - `SetWindowSubclass(h, &subclass_proc, /*kSubclassId*/ 1, reinterpret_cast<DWORD_PTR>(this));`
     (T's address is stable — heap via `create`.) `return {};`
4. **`subclass_proc`** — recover `T* self = reinterpret_cast<T*>(ref_data);`. On
   `WM_NCDESTROY`: call `self->handle_message(...)`, then `RemoveWindowSubclass`,
   then `self->hwnd_ = nullptr`. Otherwise `return self->handle_message(msg, w, l);`
5. **`handle_message`** — `switch(msg)` with `if constexpr (requires {...})` per
   hook (C6); each calls the derived hook and returns; default →
   `DefSubclassProc(hwnd_, msg, wparam, lparam)`. **Copy the structure from
   `window.hpp:84-127`.**
6. **`~Control`** — `if (hwnd_) RemoveWindowSubclass(hwnd_, &subclass_proc, 1);`
7. **`set_text`/`enable`** — `SetWindowTextW` / `EnableWindow`.
8. **Build wiring** — §7. **Exercise it** — §8.

---

## 6. Edge cases & gotchas

- **G-font — default control font is the Win95 bitmap font.** Always `WM_SETFONT`
  the default GUI font in `create()`. (For *themed* controls you additionally need
  the **Common Controls v6 manifest** + `InitCommonControlsEx` at the app level —
  see `cpp/windows/CLAUDE.md` §5. App concern, not the library's; note it where the
  example app lives.)
- **G-lifetime — sever the subclass before the object dies.** `subclass_proc` holds
  `this` via `dwRefData`; if the `Control` object is destroyed while the HWND still
  receives messages, that's a dangling deref. The dtor `RemoveWindowSubclass` +
  the `WM_NCDESTROY` handling (null `hwnd_`) cover both orders. **Never
  `DestroyWindow`** — the parent does (C3).
- **G-userdata — do not touch `GWLP_USERDATA` on a control.** Use the subclass
  `dwRefData` for `this` (C2).
- **G-click — clicks land on the PARENT.** `BN_CLICKED` → owner's `on_command(id)`,
  not the control's subclass proc (C4). The subclass proc sees raw mouse messages
  (`WM_LBUTTONDOWN/UP`), but the high-level "clicked" (keyboard/space activation,
  press-and-release-inside) is the system proc's job and is reported to the parent.
- **G-ownerdraw — custom-painting a stock control is per-control.** Intercepting
  `WM_PAINT` works but you then own *all* visual states (hot/pressed/disabled/
  focus). For some controls the cleaner route is `BS_OWNERDRAW` + the parent's
  `WM_DRAWITEM`, or `NM_CUSTOMDRAW` via `WM_NOTIFY` (both parent-side). Decide
  per-control when you actually need owner-draw — don't generalise it up front.
- **G-noexcept — don't let a hook throw into Win32.** A hook that throws propagates
  through the subclass/dispatch boundary (UB-ish). Document "hooks must not throw,"
  or wrap the dispatch in a swallowing `try/catch`. MVP: document it.

---

## 7. Files & build

**New file:** `lib/include/winwrap/control.hpp` (header-only — template, so no `.cpp`,
no `add_library` source entry).

**`lib/CMakeLists.txt`** — add to the `PUBLIC` link libraries:
- **`comctl32`** — `SetWindowSubclass` / `DefSubclassProc` / `RemoveWindowSubclass`.
- **`gdi32`** — `GetStockObject` (the font).

(`CreateWindowExW`/`SetWindowTextW`/`EnableWindow`/`SendMessageW` are `user32`,
already linked. Current list is `user32 shell32`.)

---

## 8. Verification (no Catch2 — needs a live pump)

1. Build clean under MSVC `/W4` + the project's sanitizers; clang-tidy-clean
   (as `Window`/`NotifyIcon`/`Menu` were).
2. Exercise in a small standalone app (the `hello-window` pattern): a
   `MainWindow : public Window<MainWindow>` holding a
   `std::unique_ptr<MyButton>` created in `on_created()`. Confirm:
   - the button renders in the modern GUI font (G-font), not Win95;
   - a shadowed `on_*` hook on `MyButton` fires (e.g. `on_mouse_move` →
     `OutputDebugStringW`);
   - the click reaches the parent's `on_command(id)` (C4);
   - no crash on close (G-lifetime — the subclass is removed cleanly).

---

## 9. Conventions (the CLAUDE.md chain — all apply)

Read first: `lib/include/winwrap/window.hpp` (the CRTP + `if constexpr` dispatch to
mirror — **this is 80% of `Control<T>`**), `notify_icon.{hpp,cpp}` (move-only RAII +
config-struct factory + `std::expected`), `menu.{hpp,cpp}` (existing `on_command`
routing — C4 relies on it, don't change it), `error.hpp` (`last_error()`).

- `std::expected<T, std::error_code>` at the public API; `last_error()` for Win32
  failures. WIL is for RAII handles only — **a control's HWND is non-owning (C3),
  so no WIL wrapper here.**
- `snake_case` funcs/vars, `name_` private members, PascalCase types, `{}` init.
- Doxygen `///` + `@`-commands on the **public API** only; terse `//` (why-not-what)
  in bodies; single-statement `if`/loop bodies drop braces.
- `…W` Unicode APIs; `WIN32_LEAN_AND_MEAN` + `NOMINMAX` before `<windows.h>` in the
  header (mirror existing headers).
- `reinterpret_cast` / int↔ptr (id-as-`HMENU`, `this`-as-`DWORD_PTR`, font handle)
  are inherent to Win32 — the `.clang-tidy` carve-outs already allow them.

---

## 10. Scope — `Control<T>` is the deliverable, not a control catalog

**The library ships `Control<T>` and stops there.** Apps derive their own control
types (`class MyButton : public Control<MyButton>`) with the system class and the
`on_*` hooks they need. winwrap is **primitives** (`Window<T>`, `Control<T>`,
`NotifyIcon`, `Menu`) — *not* a widget toolkit, so it does **not** ship a catalog of
pre-built `Button` / `Edit` / `Checkbox` / `ComboBox` wrappers. (That was the old
trajectory; it's the WTL/Win32++ direction and is explicitly **out of scope**.)

A concrete `final` control class enters the library **only reactively** — when the
same one-line derive-boilerplate has genuinely recurred across 2–3 real apps (the
`LIBRARY_CONVENTIONS.md` rule), and then as a thin convenience, never as a planned
set. Message reflection (clicks on the control object) and `WM_NOTIFY`-based hooks
are likewise additive — add when an app needs them, not speculatively.
