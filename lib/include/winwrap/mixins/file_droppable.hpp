#pragma once

#include "winwrap/win.hpp"

#include <shellapi.h>

#include <optional>
#include <string>
#include <vector>

#include <wil/resource.h>

namespace winwrap {

/// Unpacks an HDROP into the dropped-file paths and releases it (DragFinish runs
/// even if an allocation throws), so the hook never sees the raw handle.
inline std::vector<std::wstring> make_dropped_paths(HDROP drop) {
    auto finish = wil::scope_exit([drop] { DragFinish(drop); });
    constexpr UINT query_count{0xFFFFFFFF};
    const UINT count = DragQueryFileW(drop, query_count, nullptr, 0);
    std::vector<std::wstring> paths;
    paths.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        const UINT len = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring path(len, L'\0');
        DragQueryFileW(drop, i, path.data(), len + 1);
        paths.push_back(std::move(path));
    }
    return paths;
}

/// WINDOW mixin: routes `WM_DROPFILES` to the final type's
/// `on_files_dropped(const std::vector<std::wstring>&)` when defined.
///
/// Drops only arrive if the window opted in -- create it with
/// `.ex_style = WS_EX_ACCEPTFILES`, or call `DragAcceptFiles(hwnd(), TRUE)` in
/// `on_created()`. An opted-in window that omits the hook leaks each drop's
/// HDROP (raw Win32 behaviour -- nothing claims the message), so define the hook
/// if you opt in.
///
/// @note An elevated process receives no drops from a non-elevated Explorer
///       (UIPI filters WM_DROPFILES).
struct FileDroppable {
    std::optional<LRESULT> handle([[maybe_unused]] this auto& self, UINT msg, WPARAM wparam,
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

}  // namespace winwrap
