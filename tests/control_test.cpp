#include <catch2/catch_test_macros.hpp>

#include "winwrap/control.hpp"

namespace {
// A minimal derived control used only as a compile-time fixture: forming
// &TestControl::create (in the test below) forces the whole Control<T> chain to
// instantiate -- create -> create_control -> subclass_proc -> handle_message --
// so the build compiles and links it. Two hooks are defined so both branches of
// the if-constexpr dispatch get checked. Exercising a control for real needs a
// live message pump; see CONTROL_PLAN.md section 8.
struct TestControl : winwrap::Control<TestControl> {
    static constexpr const wchar_t* control_class = L"BUTTON";
    void on_paint() {}
    void on_mouse_move(int /*x*/, int /*y*/) {}
};
}  // namespace

TEST_CASE("Control<T> compiles and links") {
    (void)&TestControl::create;  // forms the address -> forces the template chain to compile
    REQUIRE(true);
}
