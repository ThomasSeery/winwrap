#pragma once

#include "winwrap/win.hpp"

#include <shellapi.h>

#include <optional>
#include <string>
#include <vector>

#include "winwrap/drop.hpp"

namespace winwrap {

/// Unpacks an HDROP into the dropped-file paths and releases it -- the Drop view
/// owns the handle, so DragFinish runs even if an allocation throws.
inline std::vector<std::wstring> make_dropped_paths(HDROP drop) {
    return Drop{drop}.paths();
}

/// WINDOW mixin: routes `WM_DROPFILES` to the final type's
/// `on_files_dropped(const std::vector<std::wstring>&)`, and registers the window
/// to accept dropped files automatically -- no `.ex_style = WS_EX_ACCEPTFILES`.
///
/// Registration runs at `WM_NCCREATE` (the first point the HWND is live) via
/// `DragAcceptFiles`, and *only* when the final type actually defines the hook:
/// compose the mixin without `on_files_dropped` and nothing registers, so a
/// handler-less window accepts -- and leaks -- no drop.
///
/// @note An elevated process receives no drops from a non-elevated Explorer
///       (UIPI filters WM_DROPFILES).
struct FileDroppable {
    std::optional<LRESULT> handle_message(this auto& self, UINT msg, WPARAM wparam, LPARAM) {
        switch (msg) {
            case WM_NCCREATE:
                if constexpr (requires(const std::vector<std::wstring>& paths) {
                                  self.on_files_dropped(paths);
                              })
                    DragAcceptFiles(self.hwnd(), TRUE);
                break;
            WW_CASE(WM_DROPFILES,
                    self.on_files_dropped(make_dropped_paths(reinterpret_cast<HDROP>(wparam))));
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
