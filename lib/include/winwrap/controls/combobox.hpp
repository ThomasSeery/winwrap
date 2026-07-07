#pragma once

#include "winwrap/win.hpp"

#include "winwrap/mixins.hpp"
#include "winwrap/control.hpp"

namespace winwrap {

/// A native drop-down list (the "COMBOBOX" system class, CBS_DROPDOWNLIST: pick from the
/// list, no free typing). It composes SelectionChangeable, so `on_selection_changed(index)`
/// fires when the user picks a different item (the reflected `CBN_SELCHANGE`). Populate it
/// with `add_item`, and read or set the current pick with `selection()` / `set_selection()`.
class ComboBox final : public Control<ComboBox, SelectionChangeable> {
public:
    static constexpr const wchar_t* control_class = L"COMBOBOX";
    static constexpr DWORD default_style = CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP;

    /// Appends an item to the end of the list (CB_ADDSTRING).
    void add_item(const wchar_t* text) {
        SendMessageW(hwnd(), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }

    /// The index of the selected item, or CB_ERR (-1) when nothing is selected.
    [[nodiscard]] int selection() const {
        return static_cast<int>(SendMessageW(hwnd(), CB_GETCURSEL, 0, 0));
    }

    /// Selects the item at `index` (CB_SETCURSEL); pass -1 to clear the selection.
    void set_selection(int index) { SendMessageW(hwnd(), CB_SETCURSEL, static_cast<WPARAM>(index), 0); }
};

}  // namespace winwrap
