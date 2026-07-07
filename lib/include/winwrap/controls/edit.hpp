#pragma once

#include "winwrap/win.hpp"

#include "winwrap/mixins.hpp"
#include "winwrap/control.hpp"

namespace winwrap {

/// A native single-line text box (the "EDIT" system class). It composes TextChangeable,
/// so `on_text_changed` fires on every edit (the reflected `EN_CHANGE`); read the current
/// contents with the inherited `text()` and set them with `set_text(...)`. The default
/// style gives it a visible border and horizontal scrolling for over-long input.
class Edit final : public Control<Edit, TextChangeable> {
public:
    static constexpr const wchar_t* control_class = L"EDIT";
    static constexpr DWORD default_style = WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP;
};

}  // namespace winwrap
