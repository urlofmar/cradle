#ifndef CRADLE_SERVICES_REQUESTS_H
#define CRADLE_SERVICES_REQUESTS_H

#include <functional>
#include <memory>
#include <optional>
#include <tuple>

#include <cradle/background/execution_pool.h>
#include <cradle/caching/immutable.h>
#include <cradle/core/flags.h>
#include <cradle/core/id.h>
#include <cradle/io/http_executor.h>
#include <cradle/utilities/functional.h>

namespace cradle {

struct request_resolution_system;

template<class Value>
struct request_resolution_context;

template<class Value>
struct request_interface
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

struct request_resolution_system
{
    cradle::immutable_cache cache;

    background_execution_pool execution_pool;

    http_request_system http_system;
    background_execution_pool http_pool;
};

} // namespace detail

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
report_value(request_resolution_context<Value> const& ctx, Value value)
{
    ctx.callback(std::move(value));
}

template<class Request>
void
report_continuation(
    request_resolution_context<request_value_type_t<Request>> ctx,
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

    void
    dispatch(request_resolution_context<Value> ctx) override
    {
        report_value(ctx, value_);
    }

 private:
    Value value_;
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
    std::optional<request_value_type_t<Request>> value;

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

    void
    dispatch(request_resolution_context<Value> ctx) override
    {
        ctx_ = std::move(ctx);
        std::apply([&](auto&... args) { (..., this->post_arg(args)); }, args_);
    }

 private:
    constexpr static std::size_t arg_count = sizeof...(Args);

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
            static_cast<Derived&>(*this).execute(
                ctx_,
                function_,
                std::apply(
                    [](auto... x) { return std::make_tuple(*x.value...); },
                    args_));
        }
    }

    Function function_;
    std::tuple<detail::arg_storage<Args>...> args_;
    int ready_arg_count_ = 0;
    request_resolution_context<Value> ctx_;
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

    template<class ArgTuple>
    void
    execute(
        request_resolution_context<value_type>& ctx,
        Function& function,
        ArgTuple&& args)
    {
        report_value(ctx, std::apply(function, std::move(args)));
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

template<class Result, class Function, class ArgTuple>
struct async_request_job : background_job_interface
{
    request_resolution_context<Result> ctx;
    Function function;
    ArgTuple arg_tuple;

    async_request_job(
        request_resolution_context<Result> ctx,
        Function function,
        ArgTuple arg_tuple)
        : ctx(std::move(ctx)),
          function(std::move(function)),
          arg_tuple(std::move(arg_tuple))
    {
    }

    void
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter) override
    {
        report_value(ctx, std::apply(function, std::move(arg_tuple)));
    }
};

template<class Function, class... Args>
struct async_request
    : detail::invoking_request<
          async_request<Function, Args...>,
          detail::request_application_result_t<Function, Args...>,
          Function,
          Args...>
{
    using async_request::invoking_request::invoking_request;

    using value_type = detail::request_application_result_t<Function, Args...>;

    template<class ArgTuple>
    void
    execute(
        request_resolution_context<value_type>& ctx,
        Function& function,
        ArgTuple&& args)
    {
        using job_type = async_request_job<value_type, Function, ArgTuple>;
        auto job_ptr = std::unique_ptr<job_type>(new job_type(
            std::move(ctx), std::move(function), std::move(args)));
        controller_ = detail::add_background_job(
            ctx.system->impl_->execution_pool, std::move(job_ptr));
    }

 private:
    background_job_controller controller_;
};

namespace rq {

template<class Function, class... Args>
auto
async(Function function, Args... args)
{
    return async_request<Function, Args...>(
        std::move(function), std::move(args)...);
}

} // namespace rq

template<class Request>
struct meta_request
    : request_interface<request_value_type_t<request_value_type_t<Request>>>
{
    typedef request_value_type_t<request_value_type_t<Request>> value_type;

    meta_request(Request&& request) : request_(std::move(request))
    {
    }

    void
    dispatch(request_resolution_context<value_type> ctx) override
    {
        auto& system = *ctx.system;
        post_request(
            system,
            request_,
            [ctx = std::move(ctx)](request_value_type_t<Request> generated) {
                report_continuation(ctx, std::move(generated));
            });
    }

 private:
    Request request_;
};

namespace rq {

template<class Request>
auto
meta(Request request)
{
    return meta_request<Request>(std::move(request));
}

} // namespace rq

template<class Value>
struct cached_request_watcher final : immutable_cache_entry_watcher
{
    cached_request_watcher(request_resolution_context<Value> ctx)
        : ctx_(std::move(ctx))
    {
    }

    void
    on_progress(float progress) override
    {
    }

    void
    on_failure() override
    {
    }

    void
    on_ready(untyped_immutable value) override
    {
        report_value(ctx_, *cast_immutable<Value>(value));
    }

    request_resolution_context<Value> ctx_;
};

template<class Request>
struct cached_request_posting_job : background_job_interface
{
    cached_request_posting_job(
        request_resolution_system& system,
        captured_id cache_id,
        Request request)
        : system_(system),
          request_(std::move(request)),
          cache_id_(std::move(cache_id))
    {
    }

    void
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter) override
    {
        post_request(
            system_,
            request_,
            [system = &system_,
             id = std::move(cache_id_)](request_value_type_t<Request> value) {
                set_immutable_cache_data(
                    system->impl_->cache,
                    *id,
                    make_immutable(std::move(value)));
            });
    }

 private:
    request_resolution_system& system_;
    Request request_;
    captured_id cache_id_;
};

template<class Request>
struct cached_request : request_interface<request_value_type_t<Request>>
{
    typedef request_value_type_t<Request> value_type;

    cached_request(id_interface const& id, Request&& request)
        : id_(id), request_(std::move(request))
    {
    }

    void
    dispatch(request_resolution_context<value_type> ctx) override
    {
        cache_handle_.reset(
            ctx.system->impl_->cache,
            *id_,
            [&]() {
                using job_type = cached_request_posting_job<Request>;
                auto job_ptr = std::unique_ptr<job_type>(new job_type(
                    *ctx.system, std::move(id_), std::move(request_)));
                return detail::add_background_job(
                    ctx.system->impl_->execution_pool, std::move(job_ptr));
            },
            std::make_shared<cached_request_watcher<value_type>>(ctx));
    }

 private:
    captured_id id_;
    Request request_;
    immutable_cache_entry_handle cache_handle_;
};

namespace rq {

template<class Request>
auto
cached(id_interface const& id, Request request)
{
    return cached_request<Request>(id, std::move(request));
}

} // namespace rq

template<class FunctionObject>
simple_id<FunctionObject*>
make_function_id(FunctionObject const& function_object)
{
    return simple_id<FunctionObject*>(nullptr);
}

template<class Result, class... Args>
simple_id<Result (*)(Args...)>
    make_function_id(Result (*function_pointer)(Args...))
{
    return make_id(function_pointer);
}

namespace detail {

struct http_job : http_request_job
{
    http_job(
        request_resolution_context<http_response> ctx, http_request request)
        : ctx_(ctx), request_(request)
    {
    }

    void
    execute(
        check_in_interface& check_in, progress_reporter_interface& reporter)
    {
        report_value(
            ctx_,
            this->connection->perform_request(check_in, reporter, request_));
    }

 private:
    request_resolution_context<http_response> ctx_;
    http_request request_;
};

} // namespace detail

template<class Request>
struct http_request_object : request_interface<http_response>
{
    http_request_object(Request&& request) : request_(std::move(request))
    {
    }

    void
    dispatch(request_resolution_context<http_response> ctx) override
    {
        post_request(
            *ctx.system,
            request_,
            [ctx = std::move(ctx)](http_request request) {
                auto& system = *ctx.system;
                auto job_ptr = std::unique_ptr<detail::http_job>(
                    new detail::http_job(std::move(ctx), std::move(request)));
                return detail::add_background_job(
                    system.impl_->http_pool, std::move(job_ptr));
            });
    }

 private:
    captured_id id_;
    Request request_;
    immutable_cache_entry_handle cache_handle_;
};

namespace rq {

template<class Request>
auto
http(Request request)
{
    return http_request_object<Request>(std::move(request));
}

} // namespace rq

} // namespace cradle

#endif
