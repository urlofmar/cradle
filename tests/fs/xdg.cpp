#ifndef WIN32

#include <cradle/fs/xdg.hpp>

#include <cradle/core/testing.hpp>

using namespace cradle;

TEST_CASE("XDG user config dir", "[fs][xdg]")
{
    set_environment_variable("HOME", "");
    set_environment_variable("XDG_CONFIG_HOME", "");
    REQUIRE_THROWS(xdg::get_user_config_dir());

    set_environment_variable("HOME", "/home");
    REQUIRE(xdg::get_user_config_dir() == "/home/.config");

    // Check that relative paths are ignored.
    set_environment_variable("XDG_CONFIG_HOME", "abc/def");
    REQUIRE(xdg::get_user_config_dir() == "/home/.config");

    set_environment_variable("XDG_CONFIG_HOME", "/config");
    REQUIRE(xdg::get_user_config_dir() == "/config");
}

TEST_CASE("XDG system config dirs", "[fs][xdg]")
{
    set_environment_variable("XDG_CONFIG_DIRS", "");
    REQUIRE(xdg::get_system_config_dirs() == std::vector<file_path>({ file_path("/etc/xdg") }));

    set_environment_variable("XDG_CONFIG_DIRS", "/etc/abc");
    REQUIRE(xdg::get_system_config_dirs() == std::vector<file_path>({ file_path("/etc/abc") }));

    set_environment_variable("XDG_CONFIG_DIRS", "/etc/abc:/def");
    REQUIRE(
        xdg::get_system_config_dirs() ==
        std::vector<file_path>({ file_path("/etc/abc"), file_path("/def") }));

    // Check that relative paths are ignored.
    set_environment_variable("XDG_CONFIG_DIRS", "/etc/abc:de/f");
    REQUIRE(xdg::get_system_config_dirs() == std::vector<file_path>({ file_path("/etc/abc") }));
}

#endif
