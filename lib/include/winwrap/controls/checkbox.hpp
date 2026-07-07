#pragma once

#include "winwrap/win.hpp"

#include "winwrap/mixins.hpp"
#include "winwrap/control.hpp"

namespace winwrap {

/// A native check box -- the "BUTTON" system class with the BS_AUTOCHECKBOX style, which
/// makes Windows toggle the check state on each click for you. It reuses the very same
/// Clickable mixin as Button (both are "BUTTON"-class controls that emit `BN_CLICKED`),
/// so `on_click` fires on every toggle; read the resulting state with `checked()`.
class Checkbox final : public Control<Checkbox, Clickable> {
public:
    static constexpr const wchar_t* control_class = L"BUTTON";
    static constexpr DWORD default_style = BS_AUTOCHECKBOX | WS_TABSTOP;

    /// Whether the box is currently ticked (`BM_GETCHECK`).
    [[nodiscard]] bool checked() const {
        return SendMessageW(hwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    /// Ticks or unticks the box (`BM_SETCHECK`).
    void set_checked(bool checked) {
        SendMessageW(hwnd(), BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
};

}  // namespace winwrap
