#ifndef CRADLE_CORE_TYPE_DEFINITIONS_HPP
#define CRADLE_CORE_TYPE_DEFINITIONS_HPP

#include <cradle/core/utilities.hpp>

#ifdef CRADLE_USING_TAGGED_CONSTRUCTORS
#include <boost/hana.hpp>
#endif

namespace cradle {

// The following utilities are used by the generated tagged constructors.
#ifdef CRADLE_USING_TAGGED_CONSTRUCTORS
template<class Arg>
struct is_hana_pair
{
    bool static const value = false;
};
template<class First, class Second>
struct is_hana_pair<boost::hana::pair<First,Second>>
{
    bool static const value = true;
};
template<class ...Args>
struct has_hana_pair
{
    bool static const value = false;
};
template<class Arg, class ...Rest>
struct has_hana_pair<Arg,Rest...>
{
    bool static const value =
        is_hana_pair<Arg>::value ||
        has_hana_pair<Rest...>::value;
};
#endif

// nil_t is a unit type. It has only one possible value, :nil.
struct nil_t {};
static nil_t nil;

struct blob
{
    ownership_holder ownership;
    void const* data;
    std::size_t size;

    blob() : data(0), size(0) {}

    blob(
        ownership_holder const& ownership,
        void const* data,
        std::size_t size)
      : ownership(ownership), data(data), size(size)
    {}
};

// type_info_query<T>::get(&info) should fill info with the CRADLE type info for the type T.
// All CRADLE regular types must provide a specialization of this.
template<class T>
struct type_info_query
{
};

struct api_type_info;

struct value;

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

// Lists are represented as std::vectors and can be manipulated as such.
typedef std::vector<value> value_list;

// Maps are represented as std::maps and can be manipulated as such.
typedef std::map<value,value> value_map;

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

}

#endif
