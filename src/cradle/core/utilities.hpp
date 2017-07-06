#ifndef CRADLE_CORE_UTILITIES_HPP
#define CRADLE_CORE_UTILITIES_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <boost/any.hpp>
#include <boost/exception/all.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>

namespace cradle {

using std::string;

using boost::lexical_cast;

using boost::optional;
using boost::none;

typedef int64_t integer;

// some(x) creates a boost::optional of the proper type with the value of :x.
template<class T>
optional<T>
some(T&& x)
{
    return optional<T>(std::forward<T>(x));
}

using boost::any;

template<typename T>
struct array_deleter
{
   void operator()(T* p)
   {
      delete [] p;
   }
};

// ownership_holder is meant to express polymorphic ownership of a resource. The idea is
// that the resource may be owned in many different ways, and we don't care what way. We
// only want an object that will provide ownership of the resource until it's destructed.
// We can achieve this by using an any object to hold the ownership object.
typedef any ownership_holder;

// Invoke the standard hash function for a value.
template<class T>
size_t invoke_hash(T const& x)
{ return boost::hash<T>()(x); }

// CRADLE_LAMBDIFY(f) produces a lambda that calls f, which is essentially a version of f
// that can be passed as an argument and still allows normal overload resolution.
#define CRADLE_LAMBDIFY(f) [ ](auto&& ...args) { return f(args...); }

// The following macros are simple wrappers around Boost.Exception to codify how that
// library should be used within CRADLE.

#define CRADLE_DEFINE_EXCEPTION(id) \
    struct id : virtual boost::exception, virtual std::exception \
    { \
        char const* what() const noexcept \
        { \
            return boost::diagnostic_information_what(*this); \
        } \
    };

#define CRADLE_DEFINE_ERROR_INFO(T, id) \
    typedef boost::error_info<struct id##_info_tag,T> id##_info;

#define CRADLE_THROW(x) BOOST_THROW_EXCEPTION(x)

using boost::get_error_info;

// get_required_error_info is just like get_error_info except that it requires the info to
// be present and returns a const reference to it. If the info is missing, it throws its
// own exception.
CRADLE_DEFINE_EXCEPTION(missing_error_info)
CRADLE_DEFINE_ERROR_INFO(string, error_info_id)
CRADLE_DEFINE_ERROR_INFO(string, wrapped_exception_diagnostics)
template<class ErrorInfo, class Exception>
typename ErrorInfo::error_info::value_type const&
get_required_error_info(Exception const& e)
{
    typename ErrorInfo::error_info::value_type const* info = get_error_info<ErrorInfo>(e);
    if (!info)
    {
        CRADLE_THROW(
            missing_error_info() <<
                error_info_id_info(typeid(ErrorInfo).name()) <<
                wrapped_exception_diagnostics_info(boost::diagnostic_information(e)));
    }
    return *info;
}

// Check that an index is in bounds.
// :index must be nonnegative and strictly less than :upper_bound to pass.
void check_index_bounds(string const& label, size_t index, size_t upper_bound);

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(index_out_of_bounds)
CRADLE_DEFINE_ERROR_INFO(string, index_label)
CRADLE_DEFINE_ERROR_INFO(size_t, index_value)
CRADLE_DEFINE_ERROR_INFO(size_t, index_upper_bound)

// Check that an array size matches an expected size.
void check_array_size(size_t expected_size, size_t actual_size);

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(array_size_mismatch)
CRADLE_DEFINE_ERROR_INFO(size_t, expected_size)
CRADLE_DEFINE_ERROR_INFO(size_t, actual_size)

// invalid_enum_value is thrown when an enum's raw (integer) value is invalid.
CRADLE_DEFINE_EXCEPTION(invalid_enum_value)
CRADLE_DEFINE_ERROR_INFO(string, enum_id)
CRADLE_DEFINE_ERROR_INFO(int, enum_value)

// invalid_enum_string is thrown when attempting to convert a string value to an enum
// and the string doesn't match any of the enum's cases.
CRADLE_DEFINE_EXCEPTION(invalid_enum_string)
// Note that this also uses the enum_id info declared above.
CRADLE_DEFINE_ERROR_INFO(string, enum_string)

// If a simple parsing operation fails, this exception can be thrown.
CRADLE_DEFINE_EXCEPTION(parsing_error)
CRADLE_DEFINE_ERROR_INFO(string, expected_format)
CRADLE_DEFINE_ERROR_INFO(string, parsed_text)
CRADLE_DEFINE_ERROR_INFO(string, parsing_error)

// If an error occurs internally within library that provides its own
// error messages, this is used to convey that message.
CRADLE_DEFINE_ERROR_INFO(string, internal_error_message)

}

#endif
