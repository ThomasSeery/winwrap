# winwrap — MESSAGE_TRAITS_PLAN (deferred refactor)

Design for factoring the per-message dispatch in `handle_message` into **composable
CRTP traits**, so message-handling is shared across wrappers (`Window<T>`,
`Control<T>`, future ones) without duplication and without tight coupling.

> **Status: planned, not started — deliberately deferred.** Extract this *reactively*,
> when a 3rd wrapper exists to design against (§5). Until then each wrapper keeps its
> own explicit `handle_message`; the small overlap (e.g. `WM_PAINT → on_paint` in both
> `Window<T>` and `Control<T>`) is acceptable. This is also a **learning milestone**
> (CRTP mixins / policy-based design) — tackle it as "learn the pattern, then write it
> myself," per `NORTH_STAR.md`, not a silent refactor.

---

## 1. The problem

Each CRTP wrapper's `handle_message` `switch`es on the message and routes to
compile-time `on_*` hooks. The *shape* is shared — but some *cases* are genuinely
identical across wrappers:

- `WM_PAINT → on_paint()` already appears, character-for-character, in **both**
  `window.hpp` and `control.hpp`.
- `WM_MOUSEMOVE → on_mouse_move(x, y)` is **not** control-only — a top-level
  `Window<T>` receives mouse moves over its client area too, so it'd be the same case.

Copy-pasting the case into each new wrapper is the duplication. But a single monolithic
`dispatch_common()` is *also* wrong: **"common" isn't common to _everything_** — mouse
is common to some wrappers, paint to others, keyboard to others. A flat bucket forces
every wrapper to carry every "common" message. Hence: **traits**, opted into à la carte.

## 2. The design — one CRTP mixin per capability

Each capability is its own trait: a CRTP mixin parameterized by the **final** derived
type (so it sees that type's hooks), exposing a `dispatch` returning
`std::optional<LRESULT>` (engaged = "I handled it"):

```cpp
template <typename Derived>
struct MouseInput {
    std::optional<LRESULT> dispatch(UINT msg, WPARAM w, LPARAM l) {
        auto* self = static_cast<Derived*>(this);
        switch (msg) {
            case WM_MOUSEMOVE:
                if constexpr (requires { self->on_mouse_move(0, 0); }) {
                    self->on_mouse_move(GET_X_LPARAM(l), GET_Y_LPARAM(l));
                    return 0;
                }
                break;
            // WM_LBUTTONDOWN / WM_LBUTTONUP -> on_lbutton_down / on_lbutton_up
        }
        return std::nullopt;
    }
};
// + KeyboardInput<Derived>, Paintable<Derived>, FocusAware<Derived>, ...
```

## 3. Composition — chained, no forced-common, no coupling

A wrapper inherits only the traits it wants and chains them at the top of its own
`handle_message`, then falls through to its class-specific cases + its `Def*Proc`:

```cpp
template <typename T>
class Control : public MouseInput<T>, public KeyboardInput<T>, public Paintable<T> {
    LRESULT handle_message(UINT msg, WPARAM w, LPARAM l) {
        if (auto r = MouseInput<T>::dispatch(msg, w, l))    return *r;
        if (auto r = KeyboardInput<T>::dispatch(msg, w, l)) return *r;
        if (auto r = Paintable<T>::dispatch(msg, w, l))     return *r;
        // ... control-only cases ...
        return DefSubclassProc(hwnd_, msg, w, l);
    }
};
```

`Window<T>` mixes in *its* set (`Paintable<T>` + window-specific) and falls through to
`DefWindowProcW`. Each wrapper picks its capabilities; nothing is duplicated; the
wrappers stay decoupled.

## 4. Why it's zero-overhead

Compiles down to ~the same machine code as a hand-written switch, with **no virtual
dispatch**:

- `if constexpr (requires { … })` resolves the hook at compile time → direct,
  inlinable call; no vtable, no indirection.
- `switch(msg)` → a jump table (O(1) on the message).
- Trait mixins are empty bases → empty-base optimization (zero size); their `dispatch`
  bodies inline into the caller.

Nuance: chained per-trait switches are a few small jump tables, not one big one — a
couple extra comparisons, still fully static/inlined, still zero virtual cost.
Negligible vs. the per-call cost a `virtual` base or `std::function` registry pays.

## 5. When to extract (the trigger)

Per `LIBRARY_CONVENTIONS.md` — **reactively, against 2–3 real cases**, not
speculatively. Today: two wrappers, ~one shared message — not enough to know the right
trait boundaries (you'd guess and redraw). Pull this out when a **third wrapper**
genuinely shares overlapping-but-not-identical message subsets; the natural traits will
then be obvious from real usage rather than invented up front.

## 6. Open questions for when it's built

- **CRTP threading:** `Derived` must be the *final* user type (e.g. `MyButton`) so the
  trait sees its hooks — confirm the inheritance order makes `static_cast<Derived*>`
  valid through all layers.
- **Ordering / first-match:** chained `dispatch` is first-trait-wins; document the order
  or guarantee no message appears in two traits.
- **Where traits live:** likely `winwrap/detail/` (implementation), or a public
  `winwrap/input.hpp` if apps are meant to mix them into their own `Window<T>` too.
- **DRYing the per-case `if constexpr (requires { … })` boilerplate — zero-cost DRY
  *is* possible, via a macro:**
  ```cpp
  #define WW_DISPATCH(msg, call) \
      case msg: if constexpr (requires { call; }) { call; return 0; } else break;
  ```
  Expands to the exact same `if constexpr`; the `requires` is an *unevaluated* context,
  so `call` isn't run twice -> genuinely zero runtime cost, one line per case. So the
  real trade is **macro vs. explicit**, and it's a *style* call, not a feasibility one:
  this project deliberately chose explicit dispatch over ATL/WTL-style macro
  message-maps (greppable, debuggable, scope-respecting -- see VISION/ROADMAP). **Not
  `std::function`** -- that's the one option that genuinely *adds* cost (runtime type
  erasure: heap + indirect call). A compile-time *lambda* wrapper can't do it either (a
  closure is always invocable, so it can't detect a missing hook). Member-pointer/table
  metaprogramming is a third, heavier, zero-cost route.
