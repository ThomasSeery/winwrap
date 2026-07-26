#pragma once

#include "winwrap/win.hpp"

#include <shellapi.h>

#include <string>
#include <vector>

namespace winwrap {

/// RAII view over a `WM_DROPFILES` drop: adopts the `HDROP` and releases it
/// (`DragFinish`) on destruction, exposing the drop as plain values -- no
/// sentinel indices, no length probes, no manual cleanup.
///
/// Adopt straight from the message and query it before it falls out of scope:
///
///     Drop drop{reinterpret_cast<HDROP>(wparam)};
///     if (drop.count() > 10) return 0;            // reject without unpacking
///     auto where = drop.point();
///
/// Move-only, like every owning wrapper here (a copy would double-release).
class Drop final {
public:
    /// Adopts ownership of the drop handle (a `WM_DROPFILES` wparam).
    explicit Drop(HDROP handle) noexcept : handle_{handle} {}
    ~Drop() {
        if (handle_)
            DragFinish(handle_);
    }

    Drop(const Drop&) = delete;
    Drop& operator=(const Drop&) = delete;
    Drop(Drop&& other) noexcept : handle_{other.handle_} { other.handle_ = nullptr; }
    Drop& operator=(Drop&& other) noexcept {
        if (this != &other) {
            if (handle_)
                DragFinish(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /// Number of files in the drop.
    [[nodiscard]] UINT count() const noexcept {
        constexpr UINT query_count{0xFFFFFFFF};
        return DragQueryFileW(handle_, query_count, nullptr, 0);
    }

    /// The full path of file `index` (0-based; must be < count()).
    [[nodiscard]] std::wstring path(UINT index) const {
        const UINT len = DragQueryFileW(handle_, index, nullptr, 0);
        std::wstring result(len, L'\0');
        DragQueryFileW(handle_, index, result.data(), len + 1);
        return result;
    }

    /// Every path in the drop, in order.
    [[nodiscard]] std::vector<std::wstring> paths() const {
        const UINT total = count();
        std::vector<std::wstring> result;
        result.reserve(total);
        for (UINT i = 0; i < total; ++i)
            result.push_back(path(i));
        return result;
    }

    /// Where the drop landed, in the target window's client coordinates.
    [[nodiscard]] POINT point() const noexcept {
        POINT pt{};
        DragQueryPoint(handle_, &pt);
        return pt;
    }

    /// The raw handle (escape hatch); ownership stays with the Drop.
    [[nodiscard]] HDROP handle() const noexcept { return handle_; }

private:
    HDROP handle_{};
};

}  // namespace winwrap
