# Design review: is variadic-CRTP mixin dispatch the BEST zero-cost design?

> **Run this prompt BEFORE `EXTENSIBILITY_PROMPT.md`.** That task builds more on
> the dispatch engine; if this review changes the engine, do it first.

## Goal

An adversarial review of winwrap's core dispatch mechanism — the variadic-CRTP
mixin composition in `message_handler.hpp` — against every credible alternative,
answering one question: **is this the best zero-cost abstraction a modern (C++23,
MSVC) Win32 wrapper can have, or is there a better one?**

The bar ("zero-cost" per the house definition): you don't pay for what you don't
use; what you use compiles to essentially the hand-written `WndProc` switch. The
verdict is binding: if the current design wins, write down *why* (that's README
ammo); if an alternative wins at equal cost, propose the migration.

## Current design (read these first)

- `lib/include/winwrap/message_handler.hpp` — the engine: ~30 lines,
  `MessageHandler<Derived, Mixins...>` inherits each mixin, `handle_message`
  folds over their `dispatch()` calls (`||`, first-match-wins), falls back to
  `Derived::default_proc`.
- `lib/include/winwrap/mixins/*.hpp` — the two mixin species (see `MIXINS.md`):
  hook mixins (detect an `on_*` method via `requires`) and callback mixins (own a
  `std::function` the user assigns).
- `lib/include/winwrap/mixins.hpp` — the `WW_CASE` macro used by hook mixins.
- `lib/include/winwrap/window.hpp` / `control.hpp` — the two consumers.
- `TECH_DEBT.md` → *Message dispatch — known edges* — the honest weaknesses.

## Known weaknesses to stress-test (don't defend — attack)

1. **Silent hook misses** — a typo'd/mis-signatured `on_*` hook compiles clean
   and silently never fires (`requires`-detection compiles the branch out). Any
   alternative that catches this at compile time, at the same runtime cost,
   scores a major win.
2. **First-match-wins overlap** — two mixins matching the same message silently
   steal from each other; the `WM_COMMAND` lparam split is convention, not
   compiler-enforced.
3. **The `WW_CASE` macro** — the one macro in an otherwise macro-free design.
4. **Compile-time cost** — every window instantiates every mixin's `dispatch`.

## Alternatives to evaluate (at minimum)

- **ATL/WTL macro message maps** (`BEGIN_MSG_MAP` / `MESSAGE_HANDLER`) — the
  classic CRTP + macro-table hybrid. What do the macros buy that we gave up?
- **Virtual `WndProc`** (Win32++ style) — one vtable call per message; measure
  what that actually costs before dismissing it.
- **Runtime lambda registries** (WinLamb: `on_message(WM_X, lambda)` into a
  vector-map) — per-message `std::function` lookup; flexible, not zero-cost.
- **Signal/slot** (LFWin32, Qt-like) — per-event multicast.
- **C++23 `deducing this`** (explicit object parameter) — the modern CRTP
  replacement: can the mixins/hooks work without the `Derived` template
  parameter and `static_cast` entirely? This is the most serious contender for
  a *simplification* at identical cost — evaluate it thoroughly, including
  MSVC's current support status.
- **Compile-time message tables** — `constexpr` array of `{msg, member-fn-ptr}`
  pairs built from the hook set; does it fix silent-miss / overlap detection?
- **Concepts (`concept` + `static_assert`) hardening** of the current design —
  not a replacement: can hook signatures be *validated* (fixing weakness 1)
  while keeping everything else? E.g. "if a type has any member named
  `on_paint`, assert it's invocable with the expected signature."

For each: runtime cost (be concrete about codegen), compile-time cost,
silent-miss safety, overlap safety, contributor ergonomics (one-file-per-feature
must survive), user ergonomics, escape-hatch quality, C++23-idiomatic-ness.

## Deliverable

A written verdict in `ROADMAP.md` (*Decisions locked → Message dispatch*):
either "reviewed against N alternatives on <date>, held, here's why" or a
migration proposal. If concepts-hardening (or similar) fixes weakness 1 as a
bolt-on, spec it as an additive task regardless of the main verdict.

## Working style (per NORTH_STAR.md — this is a learning project)

1. Research + reasoned comparison in chat; teach each alternative before judging
   it. **`deducing this`, concepts/`requires` internals, and function-pointer
   member tables are all NOT in `WHAT_I_KNOW.md` — teach before relying on.**
2. No code changes without an explicit go — this task's output is a verdict and
   (maybe) specs.
3. Web research is fine (talks, blogs, the libraries' sources); cite what you
   lean on.
