# Extensibility baseline: user mixin packs on Window + FileDroppable as proof

> **Run `DISPATCH_DESIGN_REVIEW_PROMPT.md` first** — it may adjust the engine
> this task builds on.

## Goal

Make winwrap's contributor story literally true: **"add a dispatch abstraction
(a mixin), and boom — the next version has the feature."** Two deliverables:

1. **`Window<T, ExtraMixins...>`** — give `Window` the user-supplied mixin pack
   `Control<T, Mixins...>` already has, so a mixin can be composed from *outside*
   the library. Today `Window`'s mixin list is hardcoded (`window.hpp`), so a new
   window mixin requires editing the library — that breaks the contributor model.
2. **`FileDroppable`** — a `WM_DROPFILES` window mixin, built as the proof that
   the model works: file drag-drop lands as a feature *without touching any
   existing library file* (umbrella-include line excepted).

**Acceptance test (non-negotiable):**

- Existing consumers compile unchanged: `class MainWindow : public
  Window<MainWindow>` still works — the pack is additive.
- A demo window declares `class DropWindow : public Window<DropWindow,
  FileDroppable>` and defines `on_files_dropped(const std::vector<std::wstring>&)`.
  Grep-test on the demo: zero `HDROP`, zero `DragQueryFile`, zero `WM_DROPFILES`.
- `git diff` of the change shows **no edits** to `message_handler.hpp`,
  `message_reflection.hpp`, or any existing mixin — the engine stays frozen.
- Document the contributor recipe: `MIXINS.md` currently teaches the *control*
  mixin recipe; add the *window* mixin variant (where the file goes, how the
  pack composes it, hook-vs-callback choice per the existing taxonomy).

## Current state (read these first)

- `lib/include/winwrap/window.hpp` — the fixed mixin list on the
  `MessageHandler<T, Lifecycle, ...>` base; the pack appends after the
  built-ins.
- `lib/include/winwrap/control.hpp` — the precedent: how `Control<T, Mixins...>`
  already forwards a pack.
- `lib/include/winwrap/message_handler.hpp` — first-match-wins fold; note the
  ordering consequence below.
- `MIXINS.md` — the taxonomy (hook vs callback mixins) and the control recipe.
- `TECH_DEBT.md` → *Message dispatch — known edges* — the overlap invariant new
  mixins must respect.

## Design questions to settle (the actual work)

a) **Pack position & ordering.** Built-ins first then extras, or extras first?
   First-match-wins makes this observable: an extra mixin matching a message a
   built-in also matches either loses (built-ins-first) or silently steals
   (extras-first). Pick, justify, document in the doc comment.

b) **`WM_DROPFILES` opt-in mechanics.** Drops only arrive if the window opted in
   — `WS_EX_ACCEPTFILES` ex-style at creation, or a `DragAcceptFiles(hwnd, TRUE)`
   call after. A mixin has no creation hook of its own — so does opt-in stay the
   user's job (config `.ex_style`, documented on the mixin), or can/should the
   mixin self-activate (e.g. lazily, or via a lifecycle message)? Decide;
   simplest correct answer wins.

c) **Hook or callback?** Per the MIXINS.md taxonomy: window features are hooks
   (`on_files_dropped` detected via `requires`), control events are callbacks.
   Confirm the taxonomy holds here or argue the exception.

d) **The `HDROP` unpack.** Count query is `DragQueryFileW(h, 0xFFFFFFFF, ...)`;
   per-index query returns lengths; `DragFinish` must run even if the hook
   throws (RAII — `wil` has no HDROP wrapper; a tiny local guard or
   `wil::scope_exit`-style cleanup, decide which fits the house deps).

e) **Additive only.** No BC break anywhere; the pack defaults empty.

## Working style (per NORTH_STAR.md — this is a learning project)

1. **Variadic templates / parameter packs are NOT in `WHAT_I_KNOW.md`** — teach
   them before the `Window` pack change (the engine's
   `template <typename> typename... Mixins` already exists to point at). Same
   for anything else off the known list that comes up.
2. Teach the design options for (a)–(d), recommend one each, then hand the spec:
   Tommy writes the novel logic (the pack change, the mixin). Boilerplate
   repetition of taught patterns is Claude's to write.
3. Update `MIXINS.md` (window recipe) + `ROADMAP.md` as part of the task.
4. House style holds: `std::expected` errors, `///` Doxygen on public API,
   config structs, no new deps, teaching in chat not in code comments.
5. MSVC build is ground truth (see memory / `TECH_DEBT.md` note on clangd
   phantoms): verify with the one-line `cmake --build build/dev --target
   winwrap_test` under vcvars64.
