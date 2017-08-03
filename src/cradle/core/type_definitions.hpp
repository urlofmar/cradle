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

}

#endif
