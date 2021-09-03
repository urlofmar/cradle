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

// CRADLE_DEFINE_FLAG_TYPE(request)
// CRADLE_DEFINE_FLAG(function, 0b01, REQUEST_CACHEABLE)

// struct function_info
// {
//     std::string name;
//     function_flag_set flags;
// };

struct untyped_request_interface
{
    virtual id_interface const&
    value_id() const = 0;
};

struct request_resolution_system;

template<class Value>
struct request_resolution_context;

template<class Value>
struct request_interface : untyped_request_interface
{
    typedef Value value_type;

    virtual void
    dispatch(request_resolution_context<Value> ctx)
        = 0;
};

template<class Value>
using request_ptr = std::unique_ptr<request_interface<Value>>;

template<class Request>
struct request_value_type
{
    typedef typename Request::value_type type;
};

template<class Value>
using request_value_type_t = typename request_value_type<Value>::type;

namespace detail {

struct request_resolution_system;

}

struct request_resolution_system
{
    request_resolution_system();
    ~request_resolution_system();

    std::unique_ptr<detail::request_resolution_system> impl_;
};

template<class Value>
struct request_resolution_context
{
    request_resolution_system* system;

    std::function<void(Value)> callback;
};

template<class Request>
void
post_request(
    request_resolution_system& system,
    Request& request,
    std::function<void(request_value_type_t<Request>)> callback)
{
    request.dispatch(request_resolution_context<request_value_type_t<Request>>{
        &system, std::move(callback)});
}

template<class Value>
void
report_value(request_resolution_context<Value>& ctx, Value value)
{
    ctx.callback(std::move(value));
}

template<class Request>
void
report_continuation(
    request_resolution_context<request_value_type_t<Request>>& ctx,
    Request request)
{
    request.dispatch(std::move(ctx));
}

template<class Value>
struct value_request : request_interface<Value>
{
    value_request(Value value) : value_(std::move(value))
    {
    }

    id_interface const&
    value_id() const override
    {
        id_ = make_id_by_reference(value_);
        return id_;
    }

    void
    dispatch(request_resolution_context<Value> ctx) override
    {
        report_value(ctx, value_);
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

template<class Derived, class Value, class Function, class... Args>
struct invoking_request : request_interface<Value>
{
    invoking_request(Function&& function, Args&&... args)
        : function_(std::forward<Function>(function)),
          args_(std::forward<Args>(args)...)
    {
    }

    id_interface const&
    value_id() const override
    {
        id_ = typed_value_id();
        return id_;
    }

    void
    dispatch(request_resolution_context<Value> ctx) override
    {
        ctx_ = std::move(ctx);
        std::apply([&](auto&... args) { (..., this->post_arg(args)); }, args_);
    }

 private:
    constexpr static std::size_t arg_count = sizeof...(Args);

    auto
    typed_value_id() const
    {
        // TODO: Factor in function ID.
        return std::apply(
            [](auto&... args) {
                return combine_ids(ref(args.request.value_id())...);
            },
            args_);
    }

    template<class Arg>
    void
    post_arg(detail::arg_storage<Arg>& arg)
    {
        post_request(*ctx_.system, arg.request, [&](auto value) {
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
            static_cast<Derived&>(*this).report_result(
                ctx_,
                std::apply(
                    function_,
                    std::apply(
                        [](auto... x) { return std::make_tuple(*x.value...); },
                        args_)));
        }
    }

    Function function_;
    std::tuple<detail::arg_storage<Args>...> args_;
    int ready_arg_count_ = 0;
    request_resolution_context<Value> ctx_;
    mutable decltype(combine_ids(ref(std::declval<Args>().value_id())...)) id_;
};

template<class Function, class... Args>
using request_application_result_t = std::
    invoke_result_t<Function, typename request_value_type<Args>::type...>;

} // namespace detail

template<class Function, class... Args>
struct apply_request
    : detail::invoking_request<
          apply_request<Function, Args...>,
          detail::request_application_result_t<Function, Args...>,
          Function,
          Args...>
{
    using apply_request::invoking_request::invoking_request;

    using value_type = detail::request_application_result_t<Function, Args...>;

    void
    report_result(
        request_resolution_context<value_type>& ctx, value_type&& result)
    {
        report_value(ctx, std::move(result));
    }
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

template<class Function, class... Args>
struct meta_request
    : detail::invoking_request<
          meta_request<Function, Args...>,
          request_value_type_t<
              detail::request_application_result_t<Function, Args...>>,
          Function,
          Args...>
{
    using meta_request::invoking_request::invoking_request;

    using generated_request_type
        = detail::request_application_result_t<Function, Args...>;
    using value_type = request_value_type_t<generated_request_type>;

    void
    report_result(
        request_resolution_context<value_type>& ctx,
        generated_request_type&& generated)
    {
        report_continuation(ctx, std::move(generated));
    }
};

namespace rq {

template<class Function, class... Args>
auto
meta(Function function, Args... args)
{
    return meta_request<Function, Args...>(
        std::move(function), std::move(args)...);
}

} // namespace rq

} // namespace cradle

#endif
