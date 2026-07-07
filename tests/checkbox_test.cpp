#include <catch2/catch_test_macros.hpp>

#include "winwrap/controls/checkbox.hpp"
#include "winwrap/window.hpp"

namespace {
// A bare host window to parent the checkbox, so the parent -> reflect -> control path
// can run for real.
struct CheckboxHost : winwrap::Window<CheckboxHost> {
    static constexpr const wchar_t* window_class_name = L"WinwrapCheckboxHost";
};

constexpr UINT checkbox_id = 1;
}  // namespace

TEST_CASE("Checkbox reuses Clickable -- on_click fires through the parent's reflection") {
    auto host = CheckboxHost::create();
    REQUIRE(host);

    auto made = winwrap::Checkbox::create(
        {.parent = (*host)->hwnd(), .id = checkbox_id, .text = L"Enable"});
    REQUIRE(made);
    auto& checkbox = *made;

    bool clicked = false;
    checkbox->on_click = [&] { clicked = true; };

    SendMessageW((*host)->hwnd(), WM_COMMAND, MAKEWPARAM(checkbox_id, BN_CLICKED),
                 reinterpret_cast<LPARAM>(checkbox->hwnd()));

    REQUIRE(clicked);
}

TEST_CASE("Checkbox check state round-trips through set_checked / checked") {
    auto host = CheckboxHost::create();
    REQUIRE(host);

    auto made = winwrap::Checkbox::create({.parent = (*host)->hwnd(), .id = checkbox_id});
    REQUIRE(made);
    auto& checkbox = *made;

    REQUIRE_FALSE(checkbox->checked());
    checkbox->set_checked(true);
    REQUIRE(checkbox->checked());
    checkbox->set_checked(false);
    REQUIRE_FALSE(checkbox->checked());
}
