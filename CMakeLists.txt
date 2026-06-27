cmake_minimum_required(VERSION 3.28)

project(winwrap
    VERSION 0.1.0
    DESCRIPTION "Modern-C++ CRTP wrappers over Win32 (windows + tray icon)"
    LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    include(CTest)
endif()

# Warning flags gated by compiler (MSVC rejects GNU-style options).
add_library(winwrap_warnings INTERFACE)
target_compile_options(winwrap_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wsign-conversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->)

option(WINWRAP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
if(WINWRAP_WARNINGS_AS_ERRORS)
    target_compile_options(winwrap_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

# Opt-in sanitizers: GNU/Clang get ASan+UBSan, MSVC gets ASan (no UBSan).
option(WINWRAP_SANITIZE "Build with AddressSanitizer (and UBSan where available)" OFF)
add_library(winwrap_sanitize INTERFACE)
if(WINWRAP_SANITIZE)
    target_compile_options(winwrap_sanitize INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fsanitize=address,undefined;-fno-omit-frame-pointer>
        $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
    target_link_options(winwrap_sanitize INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fsanitize=address,undefined>)
    # MSVC ASan annotates STL containers; Catch2 isn't built with ASan, so the
    # two disagree and the link fails (LNK2038). Turn the annotations off to mix.
    target_compile_definitions(winwrap_sanitize INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:_DISABLE_STRING_ANNOTATION=1;_DISABLE_VECTOR_ANNOTATION=1>)
endif()

include(FetchContent)
FetchContent_Declare(wil
    GIT_REPOSITORY https://github.com/microsoft/wil.git
    GIT_TAG v1.0.260126.7
    GIT_SHALLOW TRUE
    SYSTEM)
set(WIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WIL_BUILD_PACKAGING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(wil)

add_subdirectory(lib)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif()
