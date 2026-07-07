#pragma once

#include "winwrap/win.hpp"

#include <expected>
#include <system_error>

namespace winwrap {

/// The most recent Win32 error (GetLastError) as a std::error_code -- the
/// std-native carrier for a Win32 failure.
inline std::error_code last_error() {
    return std::error_code{static_cast<int>(GetLastError()), std::system_category()};
}

/// Wraps a Win32 call that returns a handle/pointer: the value on success, or
/// last_error() when it is null.
/// @warning Null-on-failure APIs only (CreatePopupMenu, CreateWindowExW, ...) --
///          *not* the INVALID_HANDLE_VALUE (-1) ones like CreateFile*, whose -1
///          sentinel reads here as success.
template <typename H>
[[nodiscard]] std::expected<H, std::error_code> checked(H handle) {
    if (handle)
        return handle;
    return std::unexpected(last_error());
}

/// Wraps a Win32 call that returns BOOL (zero on failure): success, or last_error().
[[nodiscard]] inline std::expected<void, std::error_code> checked(BOOL ok) {
    if (ok)
        return {};
    return std::unexpected(last_error());
}

}  // namespace winwrap
