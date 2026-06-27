# WinWrap

Modern-C++ CRTP wrappers over raw Win32 — top-level windows and a system-tray
(notification) icon — for self-contained MSVC apps. Fills the gap WinLamb /
Win32++ leave open: a reusable tray icon, done CRTP-style.

> **Status:** v0.1, in progress.

Compiled static library exposing the `winwrap::winwrap` CMake target.

## Build (MSVC, from an *x64 Native Tools* prompt)

```
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Use it from another project

```cmake
include(FetchContent)
FetchContent_Declare(winwrap
    GIT_REPOSITORY <repo-url>
    GIT_TAG <tag>)
FetchContent_MakeAvailable(winwrap)

target_link_libraries(your_app PRIVATE winwrap::winwrap)
```

## License

MIT — see [LICENSE](LICENSE).
