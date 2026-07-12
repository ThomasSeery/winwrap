# winwrap — mixins (the extension pattern)

How a control gets an event callback like `button->on_click = [...]`, and how a
window gains a message feature like file drag-drop, without touching the engine.
This is the pattern the library repeats for every message feature, so it's
written down once here.

See also: [VISION.md](VISION.md) (the "on-event callbacks, hiding the `WM_COMMAND`-id
plumbing" goal), `lib/include/winwrap/mixins.hpp` (the mixins) and
`lib/include/winwrap/message_reflection.hpp` (the reflection engine), `CODE_CONVENTIONS.md §3`
(where shared code lives).

## The two kinds of message mixin

Both are plain structs composed by the same compile-time fold (`MessageDispatcher`), both
add no vtable. A mixin whose `handle_message` needs the final type takes it as an explicit
object parameter — `handle_message(this auto& self, …)`, C++23 **deducing this** — so `self`
*is* the composed window/control, deduced at each call site. (The classic spelling of
this composition in the literature is **variadic CRTP** — winwrap used it until
2026-07; search that, "CRTP", or "C++ mixin" for the talks/articles, and see ROADMAP →
*Dispatch design review* for the respelling.) They differ in *who supplies the
handler*:

- **Hook mixins** (`Paintable`, `MouseInput`, …) — the mixin detects an `on_*` *method*
  the derived type defines, via `if constexpr (requires { self.on_x(); })`. The
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

## Recipe — adding a *control* mixin

1. Add `mixins/<name>.hpp` (one mixin per file, named for the mixin), same
   shape as `clickable.hpp` — the match-and-fire is delegated to `handle_notification`
   (in `message_reflection.hpp`), so the whole mixin is the callback plus one line:

   ```cpp
   struct TextChangeable {
       std::function<void()> on_text_changed;     ///< assign your handler
       [[nodiscard]] std::optional<LRESULT> handle_message(UINT msg, WPARAM wparam, LPARAM) const {
           return handle_notification(msg, wparam, EN_CHANGE, on_text_changed);
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
payload overload of `handle_notification`, which takes a **`fetch`** callable run
*only after* the code matches (so the control is queried solely when the notification
actually fired). The fetch needs the control's `hwnd()`, so this is the one control
mixin whose `handle_message` needs the object — it takes `this auto& self` and the lambda
captures it by reference (safe: the fetch runs synchronously inside the handler):

```cpp
struct SelectionChangeable {
    std::function<void(int)> on_selection_changed;  ///< gets the new index
    std::optional<LRESULT> handle_message(this auto& self, UINT msg, WPARAM wparam, LPARAM) {
        return handle_notification(msg, wparam, CBN_SELCHANGE, self.on_selection_changed,
            [&self] { return static_cast<int>(SendMessageW(self.hwnd(), CB_GETCURSEL, 0, 0)); });
    }
};
```

## Recipe — adding a *window* mixin

Window features are **hook** mixins (the taxonomy above): the message arrives at
the window itself — no reflection, no id plumbing — and the natural handler is
code on the derived window type. `FileDroppable` is the worked example.

1. Add `mixins/<name>.hpp`, same shape as `paintable.hpp`: a `WW_CASE` per
   message. If the message carries a packed payload, unpack it in a local
   `make_*` helper so the hook sees typed values, never raw `WPARAM`/`LPARAM`:

   ```cpp
   struct FileDroppable {
       std::optional<LRESULT> handle_message([[maybe_unused]] this auto& self, UINT msg, WPARAM wparam,
                                     LPARAM) {
           switch (msg) {
               WW_CASE(WM_DROPFILES,
                       self.on_files_dropped(make_dropped_paths(reinterpret_cast<HDROP>(wparam))));
               default:
                   break;
           }
           return std::nullopt;
       }
   };
   ```

2. Add the one-line `#include` to the `mixins.hpp` umbrella — that's what places
   the file inside the `WW_CASE` `#define`/`#undef` window.

3. Compose it through `Window`'s mixin pack — no library edit:

   ```cpp
   class DropWindow : public Window<DropWindow, FileDroppable> {
       // defines on_files_dropped(const std::vector<std::wstring>&)
   };
   ```

Rules specific to window mixins:

- **Extras run after the built-ins** (first-match-wins), so an extra can never
  steal a message a built-in hook handles. To intercept one anyway, shadow
  `dispatch_message` — explicit beats positional.
- **A built-in only claims a message when its hook is defined** — an extra
  matching a built-in's message would fire only on windows that *don't* define
  that built-in hook. Don't rely on that; keep the one-message-one-mixin
  invariant.
- **Opt-in mechanics stay the user's job.** If the OS must be told before the
  message ever arrives (`WM_DROPFILES` needs `WS_EX_ACCEPTFILES` or
  `DragAcceptFiles`), document it on the mixin. Don't self-activate off a
  lifecycle message: a built-in (`Lifecycle`) may claim it first and the
  activation silently never runs.
- **RAII the message's resources inside the helper** (`make_dropped_paths` runs
  `DragFinish` via `wil::scope_exit`), so the hook can throw safely and the raw
  handle never reaches user code.

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
