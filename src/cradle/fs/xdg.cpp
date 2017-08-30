#ifndef WIN32

#include <cradle/fs/xdg.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>

namespace cradle { namespace xdg {

file_path static
get_user_home_dir()
{
    return get_environment_variable("HOME");
}

file_path
get_user_config_dir()
{
    auto xdg_config_home = get_optional_environment_variable("XDG_CONFIG_HOME");
    if (xdg_config_home)
    {
        file_path dir = *xdg_config_home;
        if (dir.is_absolute())
            return dir;
    }
    return get_user_home_dir() / ".config";
}

std::vector<file_path>
get_system_config_dirs()
{
    auto xdg_config_dirs = get_optional_environment_variable("XDG_CONFIG_DIRS");

    if (!xdg_config_dirs)
        return { file_path("/etc/xdg") };

    // Split on ':'.
    std::vector<string> dirs;
    boost::split(dirs, *xdg_config_dirs, [ ](char c) { return c == ':'; });

    // Convert to file_path and filter out any relative paths.
    std::vector<file_path> valid_paths;
    for (auto const& dir : dirs)
    {
        file_path path(dir);
        if (path.is_absolute())
            valid_paths.push_back(path);
    }
    return valid_paths;
}

optional<file_path>
find_config_item(file_path const& relative_path)
{
    auto user_location = get_user_config_dir() / relative_path;
    if (exists(user_location))
        return some(user_location);

    auto system_dirs = get_system_config_dirs();
    for (auto const& dir : system_dirs)
    {
        auto this_location = dir / relative_path;
        if (exists(this_location))
            return some(this_location);
    }

    return none;
}

}}

#endif
