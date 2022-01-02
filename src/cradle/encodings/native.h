#ifndef CRADLE_ENCODINGS_NATIVE_H
#define CRADLE_ENCODINGS_NATIVE_H

#include <cradle/core.h>

#include <cradle/io/raw_memory_io.h>

namespace cradle {

dynamic
read_natively_encoded_value(uint8_t const* data, size_t size);

byte_vector
write_natively_encoded_value(dynamic const& value);

size_t
natively_encoded_sizeof(dynamic const& value);

string
natively_encoded_sha256(dynamic const& value);

} // namespace cradle

#endif
