#pragma once

#include "winwrap/win.hpp"

#include <shlobj.h>

#include <filesystem>

namespace winwrap {

/// Tells Explorer that `folder` changed, so any open view of it re-reads the folder
/// instead of drawing from its cache -- what makes a freshly written `desktop.ini`
/// take effect.
///
/// @note Fire-and-forget: the shell broadcasts the notification, so there is nothing
///       to fail and nothing to wait on.
inline void refresh_folder(const std::filesystem::path& folder) {
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, folder.c_str(), nullptr);
}

}  // namespace winwrap
