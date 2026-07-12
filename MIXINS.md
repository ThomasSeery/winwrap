# winwrap — control mixins (the extension pattern)

How a control gets an event callback like `button->on_click = [...]`. This is the
pattern v0.2 repeats for every control notification, so it's written down once here.

See also: [VISION.md](VISION.md) (the "on-event callbacks, hiding the `WM_COMMAND`-id
plumbing" goal), `lib/include/winwrap/mixins.hpp` (the mixins) and
`lib/include/winwrap/message_reflection.hpp` (the reflection engine), `CODE_CONVENTIONS.md §3`
(where shared code lives).

## The two kinds of message mixin

Both are CRTP mixins composed by the same compile-time fold (`MessageHandler`), both add
no vtable. (The composition mechanism's name in the literature is **variadic CRTP** —
search that, "CRTP", or "C++ mixin" to find the talks/articles.) They differ in *who
supplies the handler*:

- **Hook mixins** (`Paintable`, `MouseInput`, …) — the mixin detects an `on_*` *method*
  the derived type defines, via `if constexpr (requires { self->on_x(); })`. The
  handler is code on the type. Every window/control gets these from the base list.
- **Callback mixins** (`Clickable`, …) — the mixin *owns a `std::function`
  callback member* and fires it. The handler is a value you assign at runtime. A
  control opts in by listing the mixin in `Control<T, Mixins...>`.

Use a callback mixin (not a hook) for anything a *user* wires up with a lambda.

## Why control notifications need reflection

Windows sends a control's notification (a click, a text change) to the control's
**parent** as `WM_COMMAND`, not to the control. So the parent's `Reflecting` mixin
bounces it back down to the control (`SendMessageW(child, wm_command_reflect, …)`),
where the control's own mixins handle it. `Reflecting` is generic — one
mixin reflects *every* control, so **adding a mixin never touches the parent.**

## Recipe — adding a new mixin

1. Add `mixins/<name>.hpp` (one mixin per file, named for the mixin), same
   shape as `clickable.hpp` — the match-and-fire is delegated to `dispatch_notification`
   (in `message_reflection.hpp`), so the whole mixin is the callback plus one line:

   ```cpp
   template <typename>                            // template param unused (composition only)
   struct TextChangeable {
       std::function<void()> on_text_changed;     ///< assign your handler
       std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM) {
           return dispatch_notification(msg, wparam, EN_CHANGE, on_text_changed);
       }
   };
   ```

2. Add the one-line `#include` to the `mixins.hpp` umbrella.

3. Compose it on a control in `controls/<name>.hpp`:

   ```cpp
   class Edit final : public Control<Edit, TextChangeable> {
       static constexpr const wchar_t* control_class = L"EDIT";
   };
   ```

That's it — `Reflecting` already delivers the notification, so there is nothing to
wire on the window side.

### Variant — a mixin that carries a payload

Some notifications carry data that **isn't in the message** — `CBN_SELCHANGE` says
"the selection changed" but not *to what*; the index lives in the control. Use the
payload overload of `dispatch_notification`, which takes a **`fetch`** callable run
*only after* the code matches (so the control is queried solely when the notification
actually fired). The fetch needs the control's `hwnd()`, so this is the one case where
the mixin *uses* its `Derived` template parameter:

```cpp
template <typename Derived>                        // Derived IS used here, for hwnd()
struct SelectionChangeable {
    std::function<void(int)> on_selection_changed;  ///< gets the new index
    std::optional<LRESULT> dispatch(UINT msg, WPARAM wparam, LPARAM) {
        auto* self = static_cast<Derived*>(this);
        return dispatch_notification(msg, wparam, CBN_SELCHANGE, on_selection_changed,
            [self] { return static_cast<int>(SendMessageW(self->hwnd(), CB_GETCURSEL, 0, 0)); });
    }
};
```

## Rules of thumb

- **One notification code per mixin** (BN_CLICKED → `Clickable`, EN_CHANGE →
  `TextChangeable`). Keep the demux trivial.
- **Callback signature carries the payload.** A click is `void()`; a notification with
  data takes it as parameters (e.g. a selection index), not as raw Win32 words.
- **Menu / accelerator commands are *not* control notifications** — they have no
  control behind them (`lparam == 0`). A callback menu item (`add_item(text,
  handler)`) is resolved inside `Menu::show` itself via `TPM_RETURNCMD` and never
  reaches the window; legacy-id items and accelerators still go to the window's
  `Commandable` → `on_command(id)`. `Reflecting` only handles `lparam != 0`.
- **A second message that needs reflecting (WM_NOTIFY)** joins `Reflecting`; the
  control-side mixins stay exactly this shape.
