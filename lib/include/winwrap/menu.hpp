#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <expected>
#include <system_error>
#include <utility>

#include <wil/resource.h>

namespace winwrap {

/// A popup (context) menu. Owns its HMENU; when the user picks an item, Win32
/// posts WM_COMMAND (carrying the item's id) to the owner window -- handle it in
/// the owner's on_command. Build with add_item, then show.
class Menu final {
public:
    /// Creates an empty popup menu, or the Win32 error that stopped it.
    [[nodiscard]] static std::expected<Menu, std::error_code> create();

    /// Appends a text item.
    /// @param id    Command id delivered to the owner's on_command when chosen.
    /// @param text  The item's label.
    std::expected<void, std::error_code> add_item(UINT id, const wchar_t* text);

    /// Pops the menu up at the cursor; the chosen item posts WM_COMMAND to owner.
    void show(HWND owner);

    /// The underlying menu handle -- a non-owning view, for dropping to raw Win32.
    [[nodiscard]] HMENU handle() const { return handle_.get(); }

private:
    explicit Menu(wil::unique_hmenu handle) : handle_{std::move(handle)} {}

    wil::unique_hmenu handle_;
};

}  // namespace winwrap
