#ifndef CRADLE_IO_JSON_IO_HPP
#define CRADLE_IO_JSON_IO_HPP

#include <cradle/common.hpp>

// JSON I/O - conversion to and from JSON strings

namespace cradle {

// Parse some JSON test into a dynamic value.
value
parse_json_value(char const* json, size_t length);

// Same as above, but accepts a string.
value static inline
parse_json_value(string const& json)
{
    return parse_json_value(json.c_str(), json.length());
}

// Write a value to a string in JSON format.
string
value_to_json(value const& v);

// Write a value to a blob in JSON format.
// This does NOT include a terminating null character.
blob value_to_json_blob(value const& v);

}

#endif
