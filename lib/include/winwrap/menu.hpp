#pragma once

#include <windows.h>

#include <wil/resource.h>

namespace winwrap {

class Menu {
public:
    Menu() : handle_{CreatePopupMenu()} {}

    void add_item(UINT id, const wchar_t* text);
    void show(HWND owner);

private:
    wil::unique_hmenu handle_;
};

}  // namespace winwrap
