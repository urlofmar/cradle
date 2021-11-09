#ifndef CRADLE_ENCODINGS_SHA256_HASH_ID_H
#define CRADLE_ENCODINGS_SHA256_HASH_ID_H

#include <picosha2.h>

#include <cradle/core/id.h>
#include <cradle/encodings/native.h>

namespace cradle {

namespace detail {

template<class Value>
void
fold_into_sha256(picosha2::hash256_one_by_one& hasher, Value const& value)
{
    auto natively_encoded = write_natively_encoded_value(to_dynamic(value));
    hasher.process(natively_encoded.begin(), natively_encoded.end());
}

inline void
fold_into_sha256(
    picosha2::hash256_one_by_one& hasher, std::string const& value)
{
    hasher.process(value.begin(), value.end());
}

inline void
fold_into_sha256(picosha2::hash256_one_by_one& hasher, char const* value)
{
    hasher.process(value, value + strlen(value));
}

} // namespace detail

template<class... Args>
struct sha256_hashed_id : id_interface
{
    sha256_hashed_id()
    {
    }

    sha256_hashed_id(std::tuple<Args...> args) : args_(std::move(args))
    {
    }

    id_interface*
    clone() const override
    {
        return new sha256_hashed_id(args_);
    }

    bool
    equals(id_interface const& other) const override
    {
        sha256_hashed_id const& other_id
            = static_cast<sha256_hashed_id const&>(other);
        return args_ == other_id.args_;
    }

    bool
    less_than(id_interface const& other) const override
    {
        sha256_hashed_id const& other_id
            = static_cast<sha256_hashed_id const&>(other);
        return args_ < other_id.args_;
    }

    void
    deep_copy(id_interface* copy) const override
    {
        *static_cast<sha256_hashed_id*>(copy) = *this;
    }

    void
    stream(std::ostream& o) const override
    {
        picosha2::hash256_one_by_one hasher;
        std::apply(
            [&hasher](auto... args) {
                (detail::fold_into_sha256(hasher, args), ...);
            },
            args_);
        hasher.finish();
        picosha2::byte_t hashed[32];
        hasher.get_hash_bytes(hashed, hashed + 32);
        picosha2::output_hex(hashed, hashed + 32, o);
    }

    size_t
    hash() const override
    {
        return std::apply(
            [](auto... args) { return combine_hashes(invoke_hash(args)...); },
            args_);
    }

 private:
    std::tuple<Args...> args_;
};

template<class... Args>
sha256_hashed_id<Args...>
make_sha256_hashed_id(Args... args)
{
    return sha256_hashed_id<Args...>(std::make_tuple(std::move(args)...));
}

} // namespace cradle

#endif
