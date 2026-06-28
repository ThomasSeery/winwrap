#include <catch2/catch_test_macros.hpp>

#include "winwrap/window.hpp"

namespace {
// A minimal derived window used only as a compile-time fixture: forming
// &TestWindow::create (in the test below) forces the whole Window<T> chain to
// instantiate -- create -> create_window -> window_proc -> handle_message -- so
// the build compiles and links it. Two hooks are defined so both branches of the
// if-constexpr dispatch get checked. Running a window for real needs a message pump.
struct TestWindow : winwrap::Window<TestWindow> {
    static constexpr const wchar_t* window_class_name = L"WinwrapTestWindow";
    void on_paint() {}
    void on_command(UINT /*id*/) {}
};
}  // namespace

TEST_CASE("Window<T> compiles and links") {
    (void)&TestWindow::create;  // forms the address -> forces the template chain to compile
    REQUIRE(true);
}
