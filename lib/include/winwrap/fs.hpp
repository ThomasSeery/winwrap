#pragma once

#include "winwrap/win.hpp"

#include <expected>
#include <filesystem>
#include <system_error>

#include "winwrap/error.hpp"

namespace winwrap {

/// The `FILE_ATTRIBUTE_*` bits currently set on `path`.
///
/// @return The bitmask, or the Win32 error. Callers never see
///         `GetFileAttributesW`'s failure sentinel, which is `INVALID_FILE_ATTRIBUTES`
///         (`0xFFFFFFFF`) rather than the usual zero.
[[nodiscard]] inline std::expected<DWORD, std::error_code> file_attributes(
    const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return std::unexpected(last_error());
    return attributes;
}

/// Replaces every attribute bit on `path` with `attributes`.
///
/// @note Bits the OS owns (`FILE_ATTRIBUTE_DIRECTORY`, `_REPARSE_POINT`, ...) can't be
///       assigned and are ignored. Prefer add_file_attributes / remove_file_attributes
///       when changing individual bits.
[[nodiscard]] inline std::expected<void, std::error_code> set_file_attributes(
    const std::filesystem::path& path, DWORD attributes) {
    return check(SetFileAttributesW(path.c_str(), attributes));
}

/// Turns `attributes` on, keeping whatever bits `path` already has.
[[nodiscard]] inline std::expected<void, std::error_code> add_file_attributes(
    const std::filesystem::path& path, DWORD attributes) {
    return file_attributes(path).and_then(
        [&](DWORD current) { return set_file_attributes(path, current | attributes); });
}

/// Turns `attributes` off, keeping whatever bits `path` already has.
[[nodiscard]] inline std::expected<void, std::error_code> remove_file_attributes(
    const std::filesystem::path& path, DWORD attributes) {
    return file_attributes(path).and_then([&](DWORD current) {
        const DWORD remaining = current & ~attributes;
        // Win32 spells "no attributes" as FILE_ATTRIBUTE_NORMAL, never as a zero mask.
        return set_file_attributes(path, remaining ? remaining : FILE_ATTRIBUTE_NORMAL);
    });
}

}  // namespace winwrap
