#ifndef CRADLE_BACKGROUND_REQUESTS_H
#define CRADLE_BACKGROUND_REQUESTS_H

#include <functional>
#include <memory>
#include <optional>
#include <tuple>

#include <cradle/core/flags.h>
#include <cradle/core/id.h>
#include <cradle/utilities/functional.h>

namespace cradle {

struct untyped_request_interface
{
    virtual bool
    is_resolved() const = 0;

    virtual id_interface const&
    value_id() const = 0;
};

template<class Value>
struct request_interface : untyped_request_interface
{
    typedef Value value_type;

    virtual void
    dispatch(std::function<void(Value)> const& callback)
        = 0;
};

template<class Request>
struct request_value_type
{
    typedef typename Request::value_type type;
};

template<class T>
using request_ptr = std::unique_ptr<request_interface<T>>;

namespace detail {

struct request_resolution_system;

}

struct request_resolution_system
{
    ~request_resolution_system();

    std::unique_ptr<detail::request_resolution_system> impl_;
};

template<class Value>
struct value_request : request_interface<Value>
{
    value_request(Value value) : value_(std::move(value))
    {
    }

    bool
    is_resolved() const override
    {
        return true;
    }

    id_interface const&
    value_id() const override
    {
        id_ = make_id_by_reference(value_);
        return id_;
    }

    void
    dispatch(std::function<void(Value)> const& callback) override
    {
        callback(value_);
    }

 private:
    Value value_;
    simple_id_by_reference<Value> mutable id_;
};

namespace rq {

template<class Value>
value_request<Value>
value(Value value)
{
    return value_request<Value>(std::move(value));
}

} // namespace rq

namespace detail {

template<class Request>
struct arg_storage
{
    Request request;
    std::optional<typename request_value_type<Request>::type> value;

    arg_storage(Request request) : request(std::move(request))
    {
    }
};

} // namespace detail

CRADLE_DEFINE_FLAG_TYPE(function)
CRADLE_DEFINE_FLAG(function, 0b01, FUNCTION_CACHEABLE)

struct function_info
{
    std::string name;
    function_flag_set flags;
};

template<class Function, class... Args>
struct apply_request : request_interface<std::invoke_result_t<
                           Function,
                           typename request_value_type<Args>::type...>>
{
    typedef typename request_value_type<apply_request>::type result_type;

    constexpr static std::size_t arg_count = sizeof...(Args);

    apply_request(Function&& function, Args&&... args)
        : function_(std::forward<Function>(function)),
          args_(std::forward<Args>(args)...)
    {
    }

    bool
    is_resolved() const override
    {
        return false;
    }

    id_interface const&
    value_id() const override
    {
        id_ = typed_value_id();
        return id_;
    }

    void
    dispatch(std::function<void(result_type)> const& callback) override
    {
        callback_ = callback;
        std::apply(
            [&](auto&... args) { (..., this->dispatch_arg(args)); }, args_);
    }

 private:
    auto
    typed_value_id() const
    {
        return std::apply(
            [](auto&... args) {
                return combine_ids(ref(args.request.value_id())...);
            },
            args_);
    }

    template<class Arg>
    void
    dispatch_arg(detail::arg_storage<Arg>& arg)
    {
        arg.request.dispatch([&](auto value) {
            arg.value = value;
            ++this->ready_arg_count_;
            this->apply_if_ready();
        });
    }

    void
    apply_if_ready()
    {
        if (this->ready_arg_count_ == arg_count)
        {
            callback_(std::apply(
                function_,
                std::apply(
                    [](auto... x) { return std::make_tuple(*x.value...); },
                    args_)));
        }
    }

    Function function_;
    std::function<void(result_type)> callback_;
    std::tuple<detail::arg_storage<Args>...> args_;
    int ready_arg_count_ = 0;
    mutable decltype(combine_ids(ref(std::declval<Args>().value_id())...)) id_;
};

namespace rq {

template<class Function, class... Args>
auto
apply(Function function, Args... args)
{
    return apply_request<Function, Args...>(
        std::move(function), std::move(args)...);
}

} // namespace rq

} // namespace cradle

#endif
