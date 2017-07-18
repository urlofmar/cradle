#ifndef CRADLE_CORE_TYPE_INTERFACES_HPP
#define CRADLE_CORE_TYPE_INTERFACES_HPP

#include <map>
#include <vector>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cradle/core/value.hpp>

// This file provides implementations of the CRADLE Regular interface
// for all the core CRADLE types.

namespace cradle {

// NIL

bool static inline
operator==(nil_t a, nil_t b)
{ return true; }

bool static inline
operator!=(nil_t a, nil_t b)
{ return false; }

bool static inline
operator<(nil_t a, nil_t b)
{ return false; }

template<>
struct type_info_query<nil_t>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_nil_type(api_nil_type());
    }
};

size_t static inline
deep_sizeof(nil_t)
{
    return 0;
}

struct value;

// Note that we don't have to do anything here because callers of to_value are required to
// provide an uninitialized value, which defaults to :nil.
void static inline
to_value(value* v, nil_t n)
{}

void static inline
from_value(nil_t* n, value const& v)
{}

size_t static inline
hash_value(nil_t x)
{
    return 0;
}

// BOOL

template<>
struct type_info_query<bool>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_boolean_type(api_boolean_type());
    }
};

size_t static inline
deep_sizeof(bool)
{
    return sizeof(bool);
}

void
to_value(value* v, bool x);

void
from_value(bool* x, value const& v);

// INTEGERS AND FLOATS

#define CRADLE_DECLARE_NUMBER_INTERFACE(T) \
    void \
    to_value(value* v, T x); \
    \
    void \
    from_value(T* x, value const& v); \
    \
    size_t static inline \
    deep_sizeof(T) { return sizeof(T); }

#define CRADLE_DECLARE_INTEGER_INTERFACE(T) \
    template<> \
    struct type_info_query<T> \
    { \
        void static \
        get(api_type_info* info) \
        { \
            *info = make_api_type_info_with_integer_type(api_integer_type()); \
        } \
    }; \
    \
    integer \
    to_integer(T x); \
    \
    void \
    from_integer(T* x, integer n); \
    \
    CRADLE_DECLARE_NUMBER_INTERFACE(T)

CRADLE_DECLARE_INTEGER_INTERFACE(signed char)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned char)
CRADLE_DECLARE_INTEGER_INTERFACE(signed short)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned short)
CRADLE_DECLARE_INTEGER_INTERFACE(signed int)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned int)
CRADLE_DECLARE_INTEGER_INTERFACE(signed long)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned long)
CRADLE_DECLARE_INTEGER_INTERFACE(signed long long)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned long long)

#define CRADLE_DECLARE_FLOAT_INTERFACE(T) \
    template<> \
    struct type_info_query<T> \
    { \
        void static \
        get(api_type_info* info) \
        { \
            *info = make_api_type_info_with_float_type(api_float_type()); \
        } \
    }; \
    CRADLE_DECLARE_NUMBER_INTERFACE(T)

CRADLE_DECLARE_FLOAT_INTERFACE(float)
CRADLE_DECLARE_FLOAT_INTERFACE(double)

// STRING

template<>
struct type_info_query<string>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_string_type(api_string_type());
    }
};

size_t static inline
deep_sizeof(string const& x)
{
    return sizeof(string) + sizeof(char) * x.length();
}

void
to_value(value* v, string const& x);

void
from_value(string* x, value const& v);

// DATE

using boost::gregorian::date;

// Get the preferred CRADLE string representation of a date (YYYY-MM-DD).
string to_string(date const& d);

void
to_value(value* v, date const& x);

void
from_value(date* x, value const& v);

template<>
struct type_info_query<date>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_string_type(api_string_type());
    }
};

size_t static inline
deep_sizeof(date)
{
    return sizeof(date);
}

}

namespace boost { namespace gregorian {

size_t static inline
hash_value(date const& x)
{
    return cradle::invoke_hash(cradle::to_string(x));
}

}}

namespace cradle {

// PTIME

using boost::posix_time::ptime;

// Get the preferred user-readable string representation of a ptime.
string to_string(ptime const& t);

// Get the preferred representation for encoding a ptime as a string.
// (This preserves milliseconds.)
string
to_value_string(ptime const& t);

void
to_value(value* v, ptime const& x);

void
from_value(ptime* x, value const& v);

template<>
struct type_info_query<ptime>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_datetime_type(api_datetime_type());
    }
};

size_t static inline
deep_sizeof(ptime)
{
    return sizeof(ptime);
}

}

namespace boost { namespace posix_time {

size_t static inline
hash_value(ptime const& x)
{
    return cradle::invoke_hash(cradle::to_string(x));
}

}}

namespace cradle {

// BLOB

bool
operator==(blob const& a, blob const& b);

bool static inline
operator!=(blob const& a, blob const& b)
{ return !(a == b); }

bool
operator<(blob const& a, blob const& b);

template<>
struct type_info_query<blob>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_blob_type(api_blob_type());
    }
};

size_t static inline
deep_sizeof(blob const& b)
{
    // This ignores the size of the ownership holder, but that's not a big deal.
    return sizeof(blob) + b.size;
}

struct value;

void
to_value(value* v, blob const& x);

void
from_value(blob* x, value const& v);

size_t
hash_value(blob const& x);

// Make a blob that holds the contents of the given string.
blob
make_string_blob(string const& s);
blob
make_string_blob(string&& s);

// STD::VECTOR

template<class T>
void
to_value(value* v, std::vector<T> const& x)
{
    value_list l;
    size_t n_elements = x.size();
    l.resize(n_elements);
    for (size_t i = 0; i != n_elements; ++i)
    {
        to_value(&l[i], x[i]);
    }
    *v = std::move(l);
}

template<class T>
void
from_value(std::vector<T>* x, value const& v)
{
    value_list const& l = cast<value_list>(v);
    size_t n_elements = l.size();
    x->resize(n_elements);
    for (size_t i = 0; i != n_elements; ++i)
    {
        try
        {
            from_value(&(*x)[i], l[i]);
        }
        catch (boost::exception& e)
        {
            add_dynamic_path_element(e, integer(i));
            throw;
        }
    }
}

template<class T>
struct type_info_query<std::vector<T>>
{
    void static
    get(api_type_info* info)
    {
        api_array_info array_info;
        array_info.element_schema = get_type_info<T>();
        array_info.size = none;
        *info = make_api_type_info_with_array_type(array_info);
    }
};

template<class T>
size_t
deep_sizeof(std::vector<T> const& x)
{
    size_t size = sizeof(std::vector<T>);
    for (auto const& i : x)
        size += deep_sizeof(i);
    return size;
}

// STD::ARRAY

template<class T, size_t N>
void
to_value(value* v, std::array<T,N> const& x)
{
    value_list l;
    l.resize(N);
    for (size_t i = 0; i != N; ++i)
    {
        to_value(&l[i], x[i]);
    }
    *v = std::move(l);
}

template<class T, size_t N>
void
from_value(std::array<T,N>* x, value const& v)
{
    value_list const& l = cast<value_list>(v);
    check_array_size(N, l.size());
    for (size_t i = 0; i != N; ++i)
    {
        try
        {
            from_value(&(*x)[i], l[i]);
        }
        catch (boost::exception& e)
        {
            add_dynamic_path_element(e, integer(i));
            throw;
        }
    }
}

template<class T, size_t N>
struct type_info_query<std::array<T,N>>
{
    void static
    get(api_type_info* info)
    {
        api_array_info array_info;
        array_info.element_schema = get_type_info<T>();
        array_info.size = some(N);
        *info = make_api_type_info_with_array_type(array_info);
    }
};

template<class T, size_t N>
size_t
deep_sizeof(std::array<T,N> const& x)
{
    size_t size = 0;
    for (auto const& i : x)
        size += deep_sizeof(i);
    return size;
}

// STD::MAP

template<class Key, class Value>
void
to_value(value* v, std::map<Key,Value> const& x)
{
    value_map record;
    for (auto const& i : x)
        to_value(&record[to_value(i.first)], i.second);
    *v = std::move(record);
}

template<class Key, class Value>
void
from_value(std::map<Key,Value>* x, value const& v)
{
    x->clear();
    value_map const& record = cast<value_map>(v);
    for (auto const& i : record)
    {
        try
        {
            from_value(&(*x)[from_value<Key>(i.first)], i.second);
        }
        catch (boost::exception& e)
        {
            add_dynamic_path_element(e, i.first);
            throw;
        }
    }
}

template<class Key, class Value>
struct type_info_query<std::map<Key,Value>>
{
    void static
    get(api_type_info* info)
    {
        api_map_info map_info;
        map_info.key_schema = get_type_info<Key>();
        map_info.value_schema = get_type_info<Value>();
        *info = make_api_type_info_with_map_type(map_info);
    }
};

template<class Key, class Value>
size_t
deep_sizeof(std::map<Key,Value> const& x)
{
    size_t size = sizeof(std::map<Key,Value>);
    for (auto const& i : x)
        size += deep_sizeof(i.first) + deep_sizeof(i.second);
    return size;
}

// OPTIONAL

template<class T>
struct type_info_query<optional<T>>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_optional_type(get_type_info<T>());
    }
};

template<class T>
size_t
deep_sizeof(optional<T> const& x)
{
    using cradle::deep_sizeof;
    return sizeof(optional<T>) + (x ? deep_sizeof(*x) : 0);
}

template<class T>
void
to_value(cradle::value* v, optional<T> const& x)
{
    cradle::value_map record;
    if (x)
    {
        to_value(&record[cradle::value("some")], *x);
    }
    else
    {
        to_value(&record[cradle::value("none")], cradle::nil);
    }
    *v = std::move(record);
}

CRADLE_DEFINE_EXCEPTION(invalid_optional_type)
CRADLE_DEFINE_ERROR_INFO(string, optional_type_tag)

template<class T>
void
from_value(optional<T>* x, cradle::value const& v)
{
    cradle::value_map const& record = cradle::cast<cradle::value_map>(v);
    string type;
    from_value(&type, cradle::get_union_value_type(record));
    if (type == "some")
    {
        T t;
        from_value(&t, cradle::get_field(record, "some"));
        *x = t;
    }
    else if (type == "none")
    {
        *x = none;
    }
    else
    {
        CRADLE_THROW(invalid_optional_type() << optional_type_tag_info(type));
    }
}

}

namespace boost {

template<class T>
size_t
hash_value(optional<T> const& x)
{
    return x ? cradle::invoke_hash(x.get()) : 0;
}

}

#endif
