#include <cradle/core/type_interfaces.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <cradle/core/value.hpp>

namespace cradle {

// BOOL

void to_value(value* v, bool x)
{
    *v = x;
}
void from_value(bool* x, value const& v)
{
    *x = cast<bool>(v);
}

// STRING

void to_value(value* v, string const& x)
{
    *v = x;
}
void from_value(string* x, value const& v)
{
    // TODO: Fix this logic elsewhere.
    // Strings are also used to encode datetimes in JSON, so it's possible we
    // might misinterpret a string as a datetime.
    //if (v.type() == value_type::DATETIME)
    //    *x = to_value_string(cast<time>(v));
    //else
        *x = cast<string>(v);
}

// INTEGERS

#define CRADLE_DEFINE_INTEGER_INTERFACE(T) \
    void to_value(value* v, T x) \
    { \
        *v = boost::numeric_cast<integer>(x); \
    } \
    void from_value(T* x, value const& v) \
    { \
        /* Floats can also be acceptable as integers if they convert properly. */ \
        if (v.type() == value_type::FLOAT) \
            *x = boost::numeric_cast<T>(cast<double>(v)); \
        else \
            *x = boost::numeric_cast<T>(cast<integer>(v)); \
    }

CRADLE_DEFINE_INTEGER_INTERFACE(signed char)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned char)
CRADLE_DEFINE_INTEGER_INTERFACE(signed short)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned short)
CRADLE_DEFINE_INTEGER_INTERFACE(signed int)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned int)
CRADLE_DEFINE_INTEGER_INTERFACE(signed long)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned long)
CRADLE_DEFINE_INTEGER_INTERFACE(signed long long)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned long long)

// FLOATS

#define CRADLE_DEFINE_FLOAT_INTERFACE(T) \
    void to_value(value* v, T x) \
    { \
        *v = double(x); \
    } \
    void from_value(T* x, value const& v) \
    { \
        /* Integers can also acceptable as floats if they convert properly. */ \
        if (v.type() == value_type::INTEGER) \
            *x = boost::numeric_cast<T>(cast<integer>(v)); \
        else \
            *x = boost::numeric_cast<T>(cast<double>(v)); \
    }

CRADLE_DEFINE_FLOAT_INTERFACE(double)
CRADLE_DEFINE_FLOAT_INTERFACE(float)

// DATE

date static
parse_date(std::string const& s)
{
    namespace bg = boost::gregorian;
    std::istringstream is(s);
    is.imbue(
        std::locale(std::locale::classic(),
            new bg::date_input_facet("%Y-%m-%d")));
    try
    {
        date d;
        is >> d;
        if (d != date() && !is.fail() &&
            is.peek() == std::istringstream::traits_type::eof())
        {
            return d;
        }
    }
    catch (...)
    {
    }
    CRADLE_THROW(parsing_error() << expected_format_info("date") << parsed_text_info(s));
}

string
to_string(date const& d)
{
    namespace bg = boost::gregorian;
    std::ostringstream os;
    os.imbue(
        std::locale(std::cout.getloc(),
            new bg::date_facet("%Y-%m-%d")));
    os << d;
    return os.str();
}

void to_value(value* v, date const& x)
{
    *v = to_string(x);
}
void from_value(date* x, value const& v)
{
    *x = parse_date(cast<string>(v));
}

// PTIME

string to_string(ptime const& t)
{
    namespace bt = boost::posix_time;
    std::ostringstream os;
    os.imbue(
        std::locale(std::cout.getloc(),
            new bt::time_facet("%Y-%m-%d %X")));
    os << t;
    return os.str();
}

void to_value(value* v, ptime const& x)
{
    *v = x;
}
void from_value(ptime* x, value const& v)
{
    *x = cast<ptime>(v);
}

// BLOB

bool
operator==(blob const& a, blob const& b)
{
    return
        a.size == b.size &&
       (a.data == b.data || std::memcmp(a.data, b.data, a.size) == 0);
}

bool
operator<(blob const& a, blob const& b)
{
    return
        a.size < b.size ||
       (a.size == b.size && a.data != b.data && std::memcmp(a.data, b.data, a.size) < 0);
}

void
to_value(value* v, blob const& x)
{
    *v = x;
}

void
from_value(blob* x, value const& v)
{
    *x = cast<blob>(v);
}

size_t
hash_value(blob const& x)
{
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(x.data);
    return boost::hash_range(bytes, bytes + x.size);
}

template<class String>
blob
generic_make_string_blob(String&& s)
{
    blob b;
    b.ownership = std::forward<String>(s);
    string const& owned_string = boost::any_cast<string const&>(b.ownership);
    b.data = reinterpret_cast<void const*>(owned_string.c_str());
    b.size = owned_string.length();
    return b;
}

blob
make_string_blob(string const& s)
{
    return generic_make_string_blob(s);
}

blob
make_string_blob(string&& s)
{
    return generic_make_string_blob(s);
}

}
