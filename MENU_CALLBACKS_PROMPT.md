# Design task: per-item menu callbacks — kill the `on_command` switch

## Goal

Menus should work like buttons already do. Today a `Button` takes a lambda
(`Clickable` → `on_click`); a menu item takes a numeric id that the window must
demux in an `on_command(id)` switch. Replace that with per-item callbacks so the
id machinery becomes invisible library plumbing, exactly like the `lparam` fork
already is.

**Acceptance test (non-negotiable):** the demo app ends up with **zero ids, zero
`switch`, zero `on_command`** — one `add_item(label, handler)` line per item,
where each handler may be a named method on the window:

```cpp
void on_say_hello() { ... }
void on_quit()      { ... }

void on_created() {
    auto menu = Menu::create().value();
    menu.add_item(L"Say hello", [this] { on_say_hello(); });
    menu.add_item(L"Quit",      [this] { on_quit(); });
    menu_ = std::move(menu);
}
```

Grep-test for success: the demo window contains no `WM_*`, no `lparam`, no id
constants, no switch. One association line per item is the floor (no runtime
reflection in C++) — reach the floor, no residue above it.

## Current state (read these first)

- `lib/include/winwrap/menu.hpp` + `lib/src/menu.cpp` — `Menu::create()`,
  `add_item(UINT id, const wchar_t* text)`, `show(HWND owner)`. No callback
  storage. `show` uses `TrackPopupMenuEx` (no `TPM_RETURNCMD`); the pick arrives
  later as a posted `WM_COMMAND` at the owner.
- `lib/include/winwrap/mixins/commandable.hpp` — window mixin: `WM_COMMAND`
  with `lparam == 0` → calls the derived window's `on_command(LOWORD(wparam))`
  hook if defined. Note the documented invariant with `Reflecting`
  (`message_reflection.hpp`): they split `WM_COMMAND` on lparam; keep the split
  exact.
- `lib/include/winwrap/window.hpp` — composes `Commandable` in the mixin list.
- `MIXINS.md` — the hook-vs-callback taxonomy and the recipe for new mixins.
- `ROADMAP.md` → *Decisions locked → Tray / menu integration*: menu routing via
  `on_command` is currently **LOCKED**. This task deliberately revises that
  decision — update the locked section as part of the design, don't silently
  contradict it.

## Design questions to settle (the actual work)

a) **Where does the id→`std::function<void()>` map live, and how is it wired?**
   Options to weigh: inside `Menu` (then the window-side route must reach the
   right `Menu` instance to `fire(id)` — how does it know which menu is active,
   e.g. registration at `show()`?); on the window (then `add_item` must reach
   the window to register — coupling direction reverses); a new window mixin
   owning the map. Ownership and lifetime fall out of this choice.

b) **Id allocation.** The library auto-assigns ids internally — from what range,
   so they can never collide with user-chosen control ids (`ControlConfig::id`,
   e.g. 100/101 in the demo) or hand-picked menu ids from the legacy API?

c) **Escape hatch + precedence.** Does `on_command` survive for raw/advanced
   use (house precedent: raw tray events)? If an id has a registered callback
   AND the window defines `on_command`, who wins? Pick a rule and document it.

d) **Lifetime edge:** the pick is *posted* — `WM_COMMAND` can arrive after
   `show()` returned, and in principle after the `Menu` (and its map) was
   destroyed or reassigned. The route must handle "menu gone / id unknown"
   gracefully (fall through to `default_proc`? swallow?). Decide and justify.

e) **Additive only.** `add_item(id, text)` and `on_command` keep compiling and
   working — v0.1 (wifi-toggle) must not be blocked or broken.

## Precedents to compare against (briefly, not a research project)

Qt `QAction` (menu item as object + `connect`), wxWidgets `Bind(wxEVT_MENU,
handler, id)`, MFC `ON_COMMAND(id, &Cls::OnX)` message-map table. All are the
same one-association-line floor in different costumes — note what each chose
for (a)–(c).

## Working style (per NORTH_STAR.md — this is a learning project)

1. Teach the design options in chat first, with the trade-offs of (a)–(e).
2. Recommend ONE design, with rationale.
3. Update `ROADMAP.md`'s locked section + `MIXINS.md`/`TECH_DEBT.md` as needed.
4. Then hand me the spec and let **me** write the implementation — you review.
   Boilerplate repetition of an already-taught pattern is yours to write, but
   the novel logic (the map, the wiring, the route) is mine.
5. House style holds: config structs for multi-arg factories, `std::expected`
   errors, std-first (no new deps), `///` Doxygen on public API.
