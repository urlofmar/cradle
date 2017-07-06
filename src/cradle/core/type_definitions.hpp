#ifndef CRADLE_CORE_TYPE_DEFINITIONS_HPP
#define CRADLE_CORE_TYPE_DEFINITIONS_HPP

#include <cradle/core/utilities.hpp>

namespace cradle {

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
