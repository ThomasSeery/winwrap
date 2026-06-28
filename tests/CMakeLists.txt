include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(winwrap_test
    window_test.cpp
    control_test.cpp)

target_link_libraries(winwrap_test
    PRIVATE
        winwrap::winwrap
        winwrap_warnings
        winwrap_sanitize
        Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(winwrap_test)
