#include <cradle/io/file.hpp>

namespace cradle {

void
open(std::fstream& file, file_path const& path, std::ios::openmode mode)
{
    file.open(path.c_str(), mode);
    if (!file)
    {
        CRADLE_THROW(
            open_file_error() <<
                file_path_info(path) <<
                open_mode_info(mode));
    }
    file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
}
void
open(std::ifstream& file, file_path const& path, std::ios::openmode mode)
{
    file.open(path.c_str(), mode);
    if (!file)
    {
        CRADLE_THROW(
            open_file_error() <<
                file_path_info(path) <<
                open_mode_info(mode));
    }
    file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
}
void
open(std::ofstream& file, file_path const& path, std::ios::openmode mode)
{
    file.open(path.c_str(), mode);
    if (!file)
    {
        CRADLE_THROW(
            open_file_error() <<
                file_path_info(path) <<
                open_mode_info(mode));
    }
    file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
}

string
get_file_contents(file_path const& path)
{
    std::ifstream in;
    open(in, path, std::ios::in | std::ios::binary);
    string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return contents;
}

}
