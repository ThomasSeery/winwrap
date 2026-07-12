#include <catch2/catch_test_macros.hpp>

#include "winwrap/win.hpp"

#include <shellapi.h>
#include <shlobj_core.h>  // DROPFILES

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "winwrap/window.hpp"

namespace {
struct DropWindow : winwrap::Window<DropWindow, winwrap::FileDroppable> {
    static constexpr const wchar_t* window_class_name = L"WinwrapFileDroppableTestWindow";
    std::vector<std::wstring> dropped;
    void on_files_dropped(const std::vector<std::wstring>& paths) { dropped = paths; }
};

// Fabricates the HDROP Explorer would produce: a DROPFILES header followed by a
// double-null-terminated wide path list. Ownership passes to the recipient
// (make_dropped_paths DragFinish-frees it), exactly like a real drop.
HDROP make_test_hdrop(std::initializer_list<std::wstring_view> paths) {
    size_t chars = 1;  // the list's trailing second null
    for (auto path : paths)
        chars += path.size() + 1;
    HGLOBAL global = GlobalAlloc(GHND, sizeof(DROPFILES) + chars * sizeof(wchar_t));
    REQUIRE(global != nullptr);
    auto* header = static_cast<DROPFILES*>(GlobalLock(global));
    header->pFiles = static_cast<DWORD>(sizeof(DROPFILES));
    header->fWide = TRUE;
    auto* dst = reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(header) + sizeof(DROPFILES));
    for (auto path : paths) {
        dst = std::copy(path.begin(), path.end(), dst);
        *dst++ = L'\0';  // GHND zero-init supplies the final second null
    }
    GlobalUnlock(global);
    return static_cast<HDROP>(global);
}
}  // namespace

TEST_CASE("FileDroppable unpacks a synthetic WM_DROPFILES into on_files_dropped") {
    DropWindow window;
    HDROP drop = make_test_hdrop({L"C:\\one.txt", L"C:\\two two.png"});
    const LRESULT result =
        window.dispatch_message(WM_DROPFILES, reinterpret_cast<WPARAM>(drop), 0);
    CHECK(result == 0);
    REQUIRE(window.dropped.size() == 2);
    CHECK(window.dropped[0] == L"C:\\one.txt");
    CHECK(window.dropped[1] == L"C:\\two two.png");
}

TEST_CASE("FileDroppable leaves other messages unclaimed") {
    DropWindow window;
    CHECK(window.dispatch_message(WM_NULL, 0, 0) == 0);
    CHECK(window.dropped.empty());
}
