#include <catch2/catch_test_macros.hpp>

#include "winwrap/window.hpp"

#include <string>
#include <vector>

namespace {
// The extensibility acceptance demo: a window gains file drag-drop purely by
// composing the mixin -- no raw drop API appears anywhere in this file.
struct DropDemoWindow : winwrap::Window<DropDemoWindow, winwrap::FileDroppable> {
    static constexpr const wchar_t* window_class_name = L"WinwrapDropDemoWindow";
    std::vector<std::wstring> received;
    void on_files_dropped(const std::vector<std::wstring>& paths) { received = paths; }
};
}  // namespace

TEST_CASE("Window<T, FileDroppable> composes and links") {
    (void)&DropDemoWindow::create;  // forms the address -> forces the template chain to compile
    REQUIRE(true);
}
