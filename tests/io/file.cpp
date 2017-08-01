#include <cradle/io/file.hpp>

#include <cradle/core/testing.hpp>

using namespace cradle;

template<class File>
void
test_bad_open_file(std::ios::openmode mode)
{
    file_path path("/very/likely/to-be/bad/file/path/asfqwfa/--test");

    File file;
    try
    {
        open(file, path, mode);
        FAIL("no exception thrown");
    }
    catch (open_file_error& e)
    {
        REQUIRE(get_required_error_info<file_path_info>(e) == path);
        REQUIRE(get_required_error_info<open_mode_info>(e) == mode);
        get_required_error_info<internal_error_message_info>(e);
    }
}

TEST_CASE("file open errors", "[io][file]")
{
    test_bad_open_file<std::fstream>(std::ios::binary | std::ios::out | std::ios::trunc);
    test_bad_open_file<std::ifstream>(std::ios::in);
    test_bad_open_file<std::ofstream>(std::ios::binary | std::ios::out | std::ios::trunc);
}

TEST_CASE("file error bits set", "[io][file]")
{
    {
        std::fstream fs;
        open(fs, file_path("empty_file.txt"), std::ios::out | std::ios::trunc);
        int i;
        REQUIRE_THROWS(fs >> i);
    }
}

TEST_CASE("get_file_contents", "[io][file]")
{
    auto path = file_path("get_file_contents.txt");
    auto text = "some simple\n  text\n";
    {
        std::ofstream fs;
        open(fs, path, std::ios::out | std::ios::trunc | std::ios::binary);
        fs << text;
    }
    REQUIRE(get_file_contents(path) == text);
}
