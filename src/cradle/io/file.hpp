#ifndef CRADLE_FILE_IO_HPP
#define CRADLE_FILE_IO_HPP

#include <fstream>

#include <boost/filesystem/path.hpp>

#include <cradle/core/utilities.hpp>

namespace cradle {

typedef boost::filesystem::path file_path;

// Open a file into the given fstream. Throw an error if the open operation
// fails, and enable the exception bits on the fstream so that subsequent
// failures will throw exceptions.
void
open(std::fstream& file, file_path const& path, std::ios::openmode mode);
void
open(std::ifstream& file, file_path const& path, std::ios::openmode mode);
void
open(std::ofstream& file, file_path const& path, std::ios::openmode mode);

// If the above fails, it throws the following exception.
CRADLE_DEFINE_EXCEPTION(open_file_error)
CRADLE_DEFINE_ERROR_INFO(file_path, file_path)
CRADLE_DEFINE_ERROR_INFO(std::ios::openmode, open_mode)

// Get the contents of a file as a string.
string
get_file_contents(file_path const& path);

}

#endif
