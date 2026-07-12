# Naming review: the dispatch engine's vocabulary (fresh-eyes, binding)

> Run this in a **fresh session**. The names below were chosen at the end of a long
> refactor session; review them with no attachment to that session's reasoning.
> The *architecture* is locked (see ROADMAP → *Decisions locked → Message dispatch*
> and *Dispatch design review*) — this task is **names only**.

## What to review

The message dispatch engine (`lib/include/winwrap/message_dispatcher.hpp` +
`lib/include/winwrap/mixins/*.hpp` + `message_reflection.hpp`) was just respelled
to C++23 deducing this, and its names were changed in the same session. Current
scheme — provisional, not settled:

| Name | Where | Role |
|---|---|---|
| `MessageDispatcher<Mixins...>` | `message_dispatcher.hpp` | Inherits the mixins; routes each message to the first mixin that claims it, else `default_proc`. |
| `dispatch_message` | its entry point | Called by `window_proc` / `subclass_proc`; also the documented shadowable escape hatch for runtime-id messages. |
| `handle` | every mixin's member | Handles the message or declines: returns `std::optional<LRESULT>`, `nullopt` = "not mine, keep looking". |
| `handle_notification` | `message_reflection.hpp` | Shared match-and-fire the callback mixins delegate to (reflected `WM_COMMAND` + notification code → fire the `std::function`). |
| private static `handle<Mixin>` | inside `MessageDispatcher` | MSVC C7515 workaround (pack won't expand in `self.Mixins::handle`); *deliberately* overloads the mixins' member name. |

Rationale claimed for the scheme: *dispatchers dispatch, handlers handle* —
Reactor-pattern vocabulary, and Win32's own `DispatchMessage` sits one level up
in the same chain.

## History (don't re-propose these without addressing why they died)

- `MessageHandler` / `handle_message` / mixin `dispatch` — the original; rejected
  as semantically inverted (the engine routes, the mixins handle — the old names
  said the opposite on both ends).
- `dispatch_one`, `dispatch_as` — helper-name attempts; rejected as opaque.
- `try_handle` — rejected: `try_` (C# `TryGetValue`, C++ `try_lock`) implies
  *attempting something that can fail*; a mixin doesn't fail, it declines, and
  the `optional` return already says so.

## Questions the review must answer

1. Is **dispatcher/handler** the right vocabulary pair at all, versus the
   alternatives a C++ reader might expect (`process_message` — ATL's
   `ProcessWindowMessage`; router; sink; message map)? Cite real precedent.
2. Is bare **`handle`** sound as the mixin member — as a verb (clear enough?),
   as a Win32-adjacent word (`HANDLE`, `hwnd`, `WindowHandle` base — confusion
   risk?), and with the same-named private helper overloading it?
3. Is **`dispatch_message`** right for the entry point, given it's also the
   user-facing escape hatch (users shadow it in their window types)?
4. Is **`handle_notification`** right for the shared match-and-fire?
5. Whatever the verdict, check consistency with the house conventions
   (`cpp/CODE_CONVENTIONS.md`, winwrap `CODE_CONVENTIONS.md` §2 create/make
   verb rules, the reserved `try_find` idiom in TECH_DEBT).

## Constraints & deliverable

- Renaming is **cheap right now**: the tree is uncommitted and the only external
  consumer (`hello-window`) is a throwaway. Don't preserve a name out of inertia.
- Verdict is **binding**: either "held, here's why" or a concrete rename applied
  across code + docs (MIXINS.md, ROADMAP, TECH_DEBT, VISION). Update the
  *Naming (provisional)* note under ROADMAP → *Dispatch design review* to
  *settled* either way.
- Verify with the MSVC build + tests (see memory: *MSVC build is ground truth*;
  16/16 must stay green).
- Tommy decides; give one clear recommendation with reasoning, not a menu.

## After this settles (recorded, next in line — not part of this task)

1. `WW_CASE` hardening (ROADMAP → Dispatch design review, follow-up a).
2. Catch2 synthetic-message hook tests (TECH_DEBT → Silent hook misses).
3. MSVC empty-base bloat (TECH_DEBT).
4. Commit the whole working tree (respelling + rename + docs) once naming is final.
