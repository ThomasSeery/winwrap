#pragma once

#include "winwrap/win.hpp"

#include <optional>

namespace winwrap {

/// Routes `WM_SIZE` to the final type's `on_size(width, height)` (client area,
/// from lparam).
struct Sizable {
    std::optional<LRESULT> handle([[maybe_unused]] this auto& self, UINT msg, WPARAM,
                                    LPARAM lparam) {
        switch (msg) {
            WW_CASE(WM_SIZE, self.on_size(LOWORD(lparam), HIWORD(lparam)));
            default:
                break;
        }
        return std::nullopt;
    }
};

}  // namespace winwrap
