# winwrap — tech debt

Deferred decisions and known-but-accepted rough edges: things we've *seen*, reasoned
about, and chosen not to act on yet. Each item says what it is, why it's parked, and
what resolving it would involve. Design rationale lives in [VISION.md](VISION.md); the
work queue in [ROADMAP.md](ROADMAP.md). This file is only the "we know, not now" list.

## Map lookup idiom — `find` / `!= end()` noise

`Menu::show` uses the iterator idiom (`if (auto it = m.find(k); it != m.end())`) —
one hash lookup, but noisy; the iterator-free spelling (`contains` + `at`, C++20)
costs a second lookup, and C++23 still has no optional-returning map get. **Parked:**
one call site doesn't justify a helper. **Resolve when** the idiom hits its second
real consumer (CODE_CONVENTIONS §3 trigger): add a tiny `try_find` returning a
pointer-to-value (`nullptr` = absent) in a concept-named shared header.

## Umbrella headers (`controls.hpp`, `mixins.hpp`) — convenience only; decide keep-vs-drop

`controls.hpp` and `mixins.hpp` contain **no code** — no shared macro, no shared
declaration. Each is a pure `#include` aggregator ("include one, get all"). The only
things an umbrella can buy:

- **Ergonomic one-include** — `#include <winwrap/controls.hpp>` instead of N lines. Minor.
- **Stable public entry point** — the umbrella is the API surface, so internal file
  layout (`controls/button.hpp`, …) could be reorganized without breaking consumers.
  Weak here: winwrap is small and the layout is unlikely to churn.

**Why it's debt:** it needed an `IWYU pragma: begin_exports` to silence include-cleaner's
"unused-includes" (the tooling flagging that the file does nothing is a smell); it pulls
every control into any TU that includes it (compile cost); and the `control.hpp` (base)
vs `controls.hpp` (catalog) one-letter difference is a readability trap.

**Resolution options:** (a) keep as-is — cheap, harmless if unused; (b) delete both and
require consumers to include the specific control/mixin they use — fits the "thin,
pay-for-what-you-include" pillar better. **Status:** parked. Decide at the first real
external consumer (wifi-toggle), when usage shows whether one-include earns
its place.

## Message dispatch — known edges (from the CRTP mixin review)

- **Silent hook misses** — a mis-signatured or typo'd `on_*` hook silently never fires:
  the `requires`-detection compiles the absent branch out, with no diagnostic. **Resolve**
  (split by the 2026-07-12 dispatch review): the *mis-signature* half is fixable at
  compile time — the `WW_CASE` static_assert hardening specced in ROADMAP (*Decisions
  locked → Dispatch design review*, follow-up a). The *typo* half stays behavioural:
  a Catch2 test per wrapper that pushes a synthetic message through `dispatch_message`
  and asserts the hook ran. No engine change either way. *Both recommended.*
- **`return 0` ceiling** — `WW_CASE` hard-codes `return 0` for every handled message, so a
  hook can't return a meaningful `LRESULT`. Fine for the current fire-and-forget set;
  **revisit when `WM_NOTIFY` / `WM_CTLCOLOR*` land** — either extend `WW_CASE` to carry a
  return value, or route those few messages via the shadow-`handle_message` escape hatch.
- **Public `on_*` hooks** *(accepted; won't-fix)* — handlers must be `public` because the
  mixin calls them from a different class. Making them private costs friend declarations
  or an accessor shim; not worth it at this scale. Recorded so it isn't relitigated.
- **MSVC empty-base bloat** — MSVC folds only the *first* empty base to zero bytes; each
  further empty mixin base costs a byte (+ padding). Measured 2026-07-12: a plain
  `Window<T>` is 24 bytes vs 16 with `__declspec(empty_bases)` on `MessageDispatcher`, so
  the mixins.hpp "zero size" doc comment overstates on MSVC. Harmless at our scale (few,
  heap-allocated windows). **Resolve:** apply `__declspec(empty_bases)` behind a small
  portability macro (MSVC-only attribute), or just soften the doc comment.
