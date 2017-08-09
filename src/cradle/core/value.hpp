#ifndef CRADLE_CORE_VALUE_HPP
#define CRADLE_CORE_VALUE_HPP

#include <initializer_list>
#include <list>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cradle/core/type_info.hpp>

namespace cradle {

// DYNAMIC VALUES - Dynamic values are values whose structure is determined at
// run-time rather than compile time.

// All dynamic values have corresponding concrete C++ types.
// The following is a list of value types along with their significance and the
// corresponding C++ type.

std::ostream& operator<<(std::ostream& s, value_type t);

// Check that two value types match.
void check_type(value_type expected, value_type actual);

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(type_mismatch)
CRADLE_DEFINE_ERROR_INFO(value_type, expected_value_type)
CRADLE_DEFINE_ERROR_INFO(value_type, actual_value_type)

// MAPS

// This queries a map for a field with a key matching the given string.
// If the field is not present in the map, an exception is thrown.
value const& get_field(value_map const& r, string const& field);

CRADLE_DEFINE_EXCEPTION(missing_field)
CRADLE_DEFINE_ERROR_INFO(string, field_name)

// This is the same as above, but its return value indicates whether or not
// the field is in the map.
bool get_field(value const** v, value_map const& r, string const& field);

// Given a value_map that's meant to represent a union value, this checks that
// the map contains only one value and returns its key.
value const& get_union_value_type(value_map const& map);

CRADLE_DEFINE_EXCEPTION(multifield_union)

// When an error occurs in the processing of a dynamic value, this provides the path to the
// location within the value where the error occurred.
CRADLE_DEFINE_ERROR_INFO(std::list<value>, dynamic_value_path)

// Given an exception :e, this will add :path_element to the beginning of the dynamic_value_path
// info associated with :e. If there is currently no path info associated with :e, a path
// containing only :p is associated with it.
void
add_dynamic_path_element(boost::exception& e, value const& path_element);

// VALUES

// Cast a value to one of the base types.
template<class T>
T const& cast(value const& v)
{
    T const* x;
    v.get(&x);
    return *x;
}

std::ostream&
operator<<(std::ostream& os, value const& v);

// The following implements the CRADLE Regular type interface, plus some additional
// comparison operators.

struct api_type_info;

template<>
struct type_info_query<value>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_dynamic(api_dynamic_type());
    }
};

size_t
deep_sizeof(value const& v);

void
swap(value& a, value& b);

bool operator==(value const& a, value const& b);
bool operator!=(value const& a, value const& b);
bool operator<(value const& a, value const& b);
bool operator<=(value const& a, value const& b);
bool operator>(value const& a, value const& b);
bool operator>=(value const& a, value const& b);

static inline void to_value(value* v, value const& x)
{ *v = x; }
static inline void from_value(value* x, value const& v)
{ *x = v; }

size_t hash_value(value const& x);

// All regular CRADLE types provide to_value(&v, x) and from_value(&x, v).
// The following are alternate, often more convenient forms.
template<class T>
value to_value(T const& x)
{
    value v;
    to_value(&v, x);
    return v;
}
template<class T>
T from_value(value const& v)
{
    T x;
    from_value(&x, v);
    return x;
}

// Apply the functor fn to the value v.
// fn must have the function call operator overloaded for all supported
// types (including nil). If it doesn't, you'll get a compile-time error.
// fn is passed as a non-const reference so that it can accumulate results.
template<class Fn>
auto apply_to_value(Fn&& fn, value const& v)
{
    switch (v.type())
    {
     case value_type::NIL:
     default: // All cases are covered, so this is just to avoid warnings.
        return fn(nil);
     case value_type::BOOLEAN:
        return fn(cast<bool>(v));
     case value_type::INTEGER:
        return fn(cast<integer>(v));
     case value_type::FLOAT:
        return fn(cast<double>(v));
     case value_type::STRING:
        return fn(cast<string>(v));
     case value_type::BLOB:
        return fn(cast<blob>(v));
     case value_type::DATETIME:
        return fn(cast<boost::posix_time::ptime>(v));
     case value_type::LIST:
        return fn(cast<value_list>(v));
     case value_type::MAP:
        return fn(cast<value_map>(v));
    }
}

// Apply the functor fn to two values of the same type.
// If a and b are not the same type, this throws a type_mismatch exception.
template<class Fn>
auto apply_to_value_pair(Fn&& fn, value const& a, value const& b)
{
    check_type(a.type(), b.type());
    switch (a.type())
    {
     case value_type::NIL:
     default: // All cases are covered, so this is just to avoid warnings.
        return fn(nil, nil);
     case value_type::BOOLEAN:
        return fn(cast<bool>(a), cast<bool>(b));
     case value_type::INTEGER:
        return fn(cast<integer>(a), cast<integer>(b));
     case value_type::FLOAT:
        return fn(cast<double>(a), cast<double>(b));
     case value_type::STRING:
        return fn(cast<string>(a), cast<string>(b));
     case value_type::BLOB:
        return fn(cast<blob>(a), cast<blob>(b));
     case value_type::DATETIME:
        return fn(cast<boost::posix_time::ptime>(a), cast<boost::posix_time::ptime>(b));
     case value_type::LIST:
        return fn(cast<value_list>(a), cast<value_list>(b));
     case value_type::MAP:
        return fn(cast<value_map>(a), cast<value_map>(b));
    }
}

}

#endif
