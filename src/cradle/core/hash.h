#ifndef CRADLE_CORE_HASH_H
#define CRADLE_CORE_HASH_H

#include <functional>

#include <boost/functional/hash.hpp>

namespace cradle {

template<class T>
std::size_t
invoke_hash(T const& x)
{
    return boost::hash<T>()(x);
}

template<class... Hashes>
size_t
combine_hashes(Hashes... hashes)
{
    size_t seed = 0;
    (boost::hash_combine(seed, hashes), ...);
    return seed;
}

} // namespace cradle

#endif
