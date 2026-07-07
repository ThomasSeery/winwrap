#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>

#include "winwrap/controls/button.hpp"
#include "winwrap/window.hpp"

namespace {
// Bare carrier for the Clickable mixin, to exercise it without a live control.
struct ClickableFixture : winwrap::Clickable<ClickableFixture> {};

// A host window that owns a button, so the full parent -> reflect -> control path
// can run for real.
struct ClickHost : winwrap::Window<ClickHost> {
    static constexpr const wchar_t* window_class_name = L"WinwrapClickHost";
    std::unique_ptr<winwrap::Button> button;
    void on_created() {
        if (auto made = winwrap::Button::create({.parent = hwnd(), .id = button_id, .text = L"OK"}))
            button = std::move(*made);
    }
    static constexpr UINT button_id = 1;
};
}  // namespace

TEST_CASE("Clickable fires on_click on a reflected BN_CLICKED") {
    ClickableFixture fix;
    bool clicked = false;
    fix.on_click = [&] { clicked = true; };

    const auto handled = fix.dispatch(winwrap::wm_command_reflect, MAKEWPARAM(0, BN_CLICKED), 0);

    REQUIRE(clicked);
    REQUIRE(handled == 0);
}

TEST_CASE("Clickable ignores messages that are not a reflected click") {
    ClickableFixture fix;
    bool clicked = false;
    fix.on_click = [&] { clicked = true; };

    REQUIRE_FALSE(fix.dispatch(WM_PAINT, 0, 0).has_value());          // wrong message
    REQUIRE_FALSE(fix.dispatch(winwrap::wm_command_reflect, MAKEWPARAM(0, BN_SETFOCUS), 0)
                      .has_value());                                  // wrong notification
    REQUIRE_FALSE(clicked);
}

TEST_CASE("Clickable with no handler swallows the click without crashing") {
    ClickableFixture fix;  // on_click left unassigned
    REQUIRE(fix.dispatch(winwrap::wm_command_reflect, MAKEWPARAM(0, BN_CLICKED), 0) == 0);
}

TEST_CASE("Button::create(cfg, handler) wires on_click in one call") {
    auto host = ClickHost::create();
    REQUIRE(host);

    bool clicked = false;
    auto made = winwrap::Button::create({.parent = (*host)->hwnd(), .id = 2, .text = L"Go"},
                                        [&] { clicked = true; });
    REQUIRE(made);

    SendMessageW((*host)->hwnd(), WM_COMMAND, MAKEWPARAM(2, BN_CLICKED),
                 reinterpret_cast<LPARAM>((*made)->hwnd()));
    REQUIRE(clicked);
}

TEST_CASE("Button on_click fires through the parent window's reflection") {
    auto host = ClickHost::create();
    REQUIRE(host);
    REQUIRE((*host)->button);

    bool clicked = false;
    (*host)->button->on_click = [&] { clicked = true; };

    // Windows delivers a click to the parent as WM_COMMAND; SendMessageW runs the
    // whole reflect -> Clickable path synchronously, so no message pump is needed.
    SendMessageW((*host)->hwnd(), WM_COMMAND, MAKEWPARAM(ClickHost::button_id, BN_CLICKED),
                 reinterpret_cast<LPARAM>((*host)->button->hwnd()));

    REQUIRE(clicked);
}
