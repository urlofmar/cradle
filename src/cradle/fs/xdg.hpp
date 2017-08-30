#ifndef CRADLE_FS_XDG_HPP
#define CRADLE_FS_XDG_HPP

#ifndef WIN32

#include <cradle/fs/types.hpp>

// This file provides utilities for resolving directory locations according to
// the XDG Base Directory Specification.

namespace cradle { namespace xdg {

// Get the directory that should be used to store user-specific configuration
// files.
file_path
get_user_config_dir();

// Get the list of system configuration directories.
std::vector<file_path>
get_system_config_dirs();

// Given a relative path to a configuration file (or directory) that the
// application wants to read, this will scan the list of possible locations
// where it could be stored and return the full path to the first place it's
// found. (Locations are scanned in the proper XDG precedence order.)
// If the return value is none, the file doesn't exist anywhere.
optional<file_path>
find_config_item(file_path const& relative_path);

}}

#endif

#endif
