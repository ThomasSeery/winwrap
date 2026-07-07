#pragma once

// Single place for the <windows.h> preamble. WIN32_LEAN_AND_MEAN drops rarely-used
// sub-headers; NOMINMAX stops windows.h from #define-ing min/max as macros (which break
// std::min / std::max / std::numeric_limits::max). Include this instead of repeating the
// dance in every file. Pinned first by .clang-format so it precedes other Windows
// headers (commctrl.h, windowsx.h, ...), which depend on windows.h's macros.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
