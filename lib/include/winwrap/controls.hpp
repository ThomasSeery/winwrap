#pragma once

/// Umbrella header for winwrap's concrete native-control wrappers -- thin shells over the
/// OS control classes ("BUTTON", "EDIT", "COMBOBOX", ...) with on-event callbacks. Include
/// this to pull in every control, or include a single one (e.g.
/// <winwrap/controls/button.hpp>) to pull in just that. One control per file, named for
/// the control; each composes the mixins it emits (see <winwrap/mixins.hpp>).

// IWYU pragma: begin_exports
#include "winwrap/controls/button.hpp"    // Button   (BUTTON)   -> Clickable
#include "winwrap/controls/checkbox.hpp"  // Checkbox (BUTTON)   -> Clickable
#include "winwrap/controls/combobox.hpp"  // ComboBox (COMBOBOX) -> SelectionChangeable
#include "winwrap/controls/edit.hpp"      // Edit     (EDIT)     -> TextChangeable
// IWYU pragma: end_exports
