#include <cradle/fs/utilities.h>

#include <filesystem>

namespace cradle {

void
reset_directory(file_path const& dir)
{
    if (exists(dir))
        remove_all(dir);
    create_directory(dir);
}

} // namespace cradle