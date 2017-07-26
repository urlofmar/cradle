#ifndef CRADLE_CORE_VALUE_HPP
#define CRADLE_CORE_VALUE_HPP

#include <initializer_list>
#include <list>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cradle/core/type_info.hpp>

namespace cradle {

// DYNAMIC VALUES - Dynamic values are values whose structure is determined at
// run-time rather than compile time.

struct value;

// All dynamic values have corresponding concrete C++ types.
// The following is a list of value types along with their significance and the
// corresponding C++ type.

enum class value_type
{
    NIL,            // no value, nil_t
    BOOLEAN,        // bool
    INTEGER,        // integer
    FLOAT,          // double
    STRING,         // string
    BLOB,           // binary blob, blob
    DATETIME,       // boost::posix_time::ptime
    LIST,           // ordered list of values, value_list
    MAP,            // collection of named values, value_map
};

std::ostream& operator<<(std::ostream& s, value_type t);

// Check that two value types match.
void check_type(value_type expected, value_type actual);

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(type_mismatch)
CRADLE_DEFINE_ERROR_INFO(value_type, expected_value_type)
CRADLE_DEFINE_ERROR_INFO(value_type, actual_value_type)

// Lists are represented as std::vectors and can be manipulated as such.
typedef std::vector<value> value_list;

// MAPS

// Maps are represented as std::maps and can be manipulated as such.
typedef std::map<value,value> value_map;

// This queries a map for a field with a key matching the given string.
// If the field is not present in the map, an exception is thrown.
value get_field(value_map const& r, string const& field);

CRADLE_DEFINE_EXCEPTION(missing_field)
CRADLE_DEFINE_ERROR_INFO(string, field_name)

// This is the same as above, but its return value indicates whether or not
// the field is in the map.
bool get_field(value* v, value_map const& r, string const& field);

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

struct value
{
    // CONSTRUCTORS

    // Default construction creates a nil value.
    value() { set(nil); }

    // Construct a value from one of the base types.
    value(nil_t v) { set(v); }
    value(bool v) { set(v); }
    value(integer v) { set(v); }
    value(double v) { set(v); }
    value(string const& v) { set(v); }
    value(string&& v) { set(std::move(v)); }
    value(char const* v) { set(string(v)); }
    value(blob const& v) { set(v); }
    value(blob&& v) { set(std::move(v)); }
    value(boost::posix_time::ptime const& v) { set(v); }
    value(boost::posix_time::ptime&& v) { set(std::move(v)); }
    value(value_list const& v) { set(v); }
    value(value_list&& v) { set(std::move(v)); }
    value(value_map const& v) { set(v); }
    value(value_map&& v) { set(std::move(v)); }

    // Construct from an initializer list.
    value(std::initializer_list<value> list);

    // GETTERS

    // Get the type of value stored here.
    value_type type() const { return type_; }

    // Get the value.
    // Requesting the wrong type will result in a type_mismatch exception.
    // If the type matches, *v will point to the value.
    void get(bool const** v) const;
    void get(integer const** v) const;
    void get(double const** v) const;
    void get(string const** v) const;
    void get(blob const** v) const;
    void get(boost::posix_time::ptime const** v) const;
    void get(value_list const** v) const;
    void get(value_map const** v) const;

 private:
    void set(nil_t _);
    void set(bool v);
    void set(integer v);
    void set(double v);
    void set(string const& v);
    void set(string&& v);
    void set(char const* v) { set(string(v)); }
    void set(blob const& v);
    void set(blob&& v);
    void set(boost::posix_time::ptime const& v);
    void set(boost::posix_time::ptime&& v);
    void set(value_list const& v);
    void set(value_list&& v);
    void set(value_map const& v);
    void set(value_map&& v);

    friend void swap(value& a, value& b);

    value_type type_;
    any value_;
};

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
        *info = construct_api_type_info_with_dynamic_type(api_dynamic_type());
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
