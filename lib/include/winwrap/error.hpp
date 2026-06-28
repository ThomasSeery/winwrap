#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <system_error>

namespace winwrap {

/// The most recent Win32 error (GetLastError) as a std::error_code -- the
/// std-native carrier for a Win32 failure.
inline std::error_code last_error() {
    return std::error_code{static_cast<int>(GetLastError()), std::system_category()};
}

}  // namespace winwrap
