#ifndef CRADLE_ENCODINGS_LZ4_HPP
#define CRADLE_ENCODINGS_LZ4_HPP

#include <cradle/core.h>
#include <cradle/fs/types.hpp>

namespace cradle {

namespace lz4 {

// Given the size of a block of data, return the worst-case size of that data
// when it's compressed with LZ4.
size_t
max_compressed_size(size_t original_size);

// Compress a block of data with LZ4.
// Return the actual size of the compressed data.
size_t
compress(void* dst, size_t dst_size, void const* src, size_t src_size);

// Decompress a block of data that's been compressed with LZ4.
// When decompressing, we assume the caller already knows the size of the
// uncompressed data (based on other info related to the data), so the caller
// is expected to allocate the full block of data and pass in its size.
void
decompress(void* dst, size_t dst_size, void const* src, size_t src_size);

} // namespace lz4

// This is thrown when lz4 reports an error.
CRADLE_DEFINE_EXCEPTION(lz4_error)
// This exception provides internal_error_message_info.
// This exception MAY provide the internal error code from lz4 (if there is
// one).
CRADLE_DEFINE_ERROR_INFO(int, lz4_error_code)

} // namespace cradle

#endif
