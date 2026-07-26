#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "winwrap/message_loop.hpp"
#include "winwrap/window.hpp"

namespace {
// A message-only window (never shown) that records every command it's sent, so a
// test can post messages, pump them through run(), and assert what was dispatched.
struct PumpWindow : winwrap::Window<PumpWindow> {
    static constexpr const wchar_t* window_class_name = L"WinwrapPumpTestWindow";
    std::vector<UINT> commands;
    void on_command(UINT id) { commands.push_back(id); }
};

// The main-window pattern: quit the app when this window is destroyed.
struct ClosableWindow : winwrap::Window<ClosableWindow> {
    static constexpr const wchar_t* window_class_name = L"WinwrapClosableTestWindow";
    void on_destroy() { winwrap::quit(); }
};
}  // namespace

TEST_CASE("run() returns the posted exit code when WM_QUIT is already queued") {
    PostQuitMessage(42);
    CHECK(winwrap::run() == 42);
}

TEST_CASE("run() dispatches a posted message to the window before it quits") {
    auto window = PumpWindow::create({.parent = HWND_MESSAGE});
    REQUIRE(window.has_value());
    PostMessageW((*window)->hwnd(), WM_COMMAND, 7, 0);
    PostQuitMessage(0);
    CHECK(winwrap::run() == 0);
    REQUIRE((*window)->commands.size() == 1);
    CHECK((*window)->commands.front() == 7);
}

TEST_CASE("run() drains all queued messages in order, then quits") {
    auto window = PumpWindow::create({.parent = HWND_MESSAGE});
    REQUIRE(window.has_value());
    const HWND hwnd = (*window)->hwnd();
    PostMessageW(hwnd, WM_COMMAND, 1, 0);
    PostMessageW(hwnd, WM_COMMAND, 2, 0);
    PostMessageW(hwnd, WM_COMMAND, 3, 0);
    PostQuitMessage(0);
    CHECK(winwrap::run() == 0);
    CHECK((*window)->commands == std::vector<UINT>{1, 2, 3});
}

TEST_CASE("closing the window exits run() via on_destroy -> quit") {
    auto window = ClosableWindow::create({.parent = HWND_MESSAGE});
    REQUIRE(window.has_value());
    PostMessageW((*window)->hwnd(), WM_CLOSE, 0, 0);  // WM_CLOSE -> DefWindowProc destroys it
    CHECK(winwrap::run() == 0);
}
