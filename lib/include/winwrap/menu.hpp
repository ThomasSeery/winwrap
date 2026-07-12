#pragma once

#include "winwrap/win.hpp"

#include <expected>
#include <functional>
#include <system_error>
#include <unordered_map>
#include <utility>

#include <wil/resource.h>

namespace winwrap {

/// A popup (context) menu. Owns its HMENU. Items come in two flavours, each with
/// its own delivery route: a handler item (`add_item(text, handler)`) fires its
/// handler from show() when picked -- the command id is internal plumbing; an id
/// item (`add_item(id, text)`) posts `WM_COMMAND` to the owner window -- handle it
/// in the owner's on_command. Build with add_item, then show.
class Menu final {
public:
    /// Creates an empty popup menu, or the Win32 error that stopped it.
    [[nodiscard]] static std::expected<Menu, std::error_code> create();

    /// Appends a text item.
    /// @param id    Command id delivered to the owner's on_command when chosen.
    ///              Must be below 0xE000 -- ids from there up are auto-assigned
    ///              to handler items.
    /// @param text  The item's label.
    std::expected<void, std::error_code> add_item(UINT id, const wchar_t* text);

    /// Appends a text item with a per-item handler; no id, no on_command.
    /// @param text     The item's label.
    /// @param handler  Invoked from show() when the item is picked. Must not
    ///                 destroy this Menu.
    std::expected<void, std::error_code> add_item(const wchar_t* text,
                                                  std::function<void()> handler);

    /// Pops the menu up at the cursor and delivers the pick: a handler item fires
    /// its handler before show returns; an id item posts `WM_COMMAND` to owner.
    void show(HWND owner);

    /// The underlying menu handle -- a non-owning view, for dropping to raw Win32.
    [[nodiscard]] HMENU handle() const { return handle_.get(); }

private:
    // Handler-item ids are auto-assigned from first_handler_id up; user ids stay below.
    static constexpr UINT first_handler_id{0xE000};

    explicit Menu(wil::unique_hmenu handle) : handle_{std::move(handle)} {}

    wil::unique_hmenu handle_;
    std::unordered_map<UINT, std::function<void()>> handlers_;
    UINT next_id_{first_handler_id};
};

}  // namespace winwrap
