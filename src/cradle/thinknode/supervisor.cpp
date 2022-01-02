#include <cradle/io/asio.h>

#include <chrono>
#include <mutex>
#include <thread>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <spdlog/spdlog.h>

#include <cradle/thinknode/supervisor.h>

#include <cradle/core/monitoring.h>
#include <cradle/encodings/json.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/service/core.h>
#include <cradle/thinknode/ipc.h>
#include <cradle/thinknode/messages.h>
#include <cradle/utilities/environment.h>
#include <cradle/utilities/errors.h>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace cradle {

uint8_t static const ipc_version = 1;

static string
extract_tag(thinknode_provider_image_info const& image)
{
    switch (get_tag(image))
    {
        case thinknode_provider_image_info_tag::TAG:
            return as_tag(image);
        case thinknode_provider_image_info_tag::DIGEST:
            return as_digest(image);
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("thinknode_provider_image_info_tag")
                << enum_value_info(static_cast<int>(get_tag(image))));
    }
}

enum class docker_service_type
{
    WINDOWS,
    LINUX,
    WSL
};

static http_request
make_docker_request(
    docker_service_type service_type,
    http_request_method method,
    string path,
    http_header_list headers,
    http_body body = blob())
{
    switch (service_type)
    {
        case docker_service_type::WINDOWS:
            return make_http_request(
                method, "http://localhost:2375" + path, headers, body);
        case docker_service_type::LINUX:
        case docker_service_type::WSL:
            return http_request{
                method,
                "http://localhost" + path,
                headers,
                body,
                some(string("/var/run/docker.sock"))};
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("docker_service_type")
                << enum_value_info(static_cast<int>(service_type)));
    }
}

static docker_service_type
detect_docker(http_connection_interface& connection)
{
    // Try Linux.
    try
    {
        auto query = make_docker_request(
            docker_service_type::LINUX,
            http_request_method::GET,
            "/v1.38/info",
            http_header_list());
        null_check_in check_in;
        null_progress_reporter reporter;
        auto response = connection.perform_request(check_in, reporter, query);

        // Check which OS the actual Docker server is running on. It's possible
        // that we are running inside WSL, where the server runs in Windows but
        // the client runs inside Linux and can still connect in the Linux way.
        auto info = parse_json_response(response);
        if (cast<std::string>(
                get_field(cast<dynamic_map>(info), "KernelVersion"))
                .find("microsoft")
            != std::string::npos)
        {
            return docker_service_type::WSL;
        }

        return docker_service_type::LINUX;
    }
    catch (...)
    {
    }

    // If the Linux way didn't work, assume Windows (but check it).
    auto query = make_docker_request(
        docker_service_type::WINDOWS,
        http_request_method::GET,
        "/v1.38/info",
        http_header_list());
    null_check_in check_in;
    null_progress_reporter reporter;
    connection.perform_request(check_in, reporter, query);

    return docker_service_type::WINDOWS;
}

struct local_calculation_service
{
    docker_service_type
    get_docker_type(http_connection_interface& connection);

 private:
    std::optional<docker_service_type> docker_type_;
    std::mutex mutex_;
};

docker_service_type
local_calculation_service::get_docker_type(
    http_connection_interface& connection)
{
    std::scoped_lock<std::mutex> lock(mutex_);
    if (!docker_type_)
        docker_type_ = detect_docker(connection);
    return *docker_type_;
}

static void
pull_image(
    docker_service_type service_type,
    http_connection_interface& connection,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image)
{
    auto query = make_docker_request(
        service_type,
        http_request_method::POST,
        "/v1.38/images/create?fromImage=registry-mgh.thinknode.com/" + account
            + "/" + app + "&tag=" + extract_tag(image),
        {{"X-Registry-Auth",
          get_environment_variable("CRADLE_THINKNODE_DOCKER_AUTH")}});
    null_check_in check_in;
    null_progress_reporter reporter;
    connection.perform_request(check_in, reporter, query);
}

static string
spawn_provider(
    docker_service_type service_type,
    http_connection_interface& connection,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image,
    uint16_t port,
    string const& pid)
{
    null_check_in check_in;
    null_progress_reporter reporter;

    // Create the container.
    string id;
    {
        auto request = make_docker_request(
            service_type,
            http_request_method::POST,
            "/v1.38/containers/create",
            {{"Content-Type", "application/json"},
             {"X-Registry-Auth",
              get_environment_variable("CRADLE_THINKNODE_DOCKER_AUTH")}},
            value_to_json_blob(dynamic(
                {{"Image",
                  "registry-mgh.thinknode.com/" + account + "/" + app + "@"
                      + extract_tag(image)},
                 {"Env",
                  {(service_type == docker_service_type::WINDOWS
                    || service_type == docker_service_type::WSL)
                       ? "THINKNODE_HOST=host.docker.internal"
                       : "THINKNODE_HOST=localhost",
                   "THINKNODE_PORT=" + boost::lexical_cast<string>(port),
                   "THINKNODE_PID=" + pid}},
                 {"HostConfig", {{"NetworkMode", "host"}}}})));
        auto response
            = connection.perform_request(check_in, reporter, request);
        id = cast<string>(
            get_field(cast<dynamic_map>(parse_json_response(response)), "Id"));
    }

    // Start it.
    {
        auto request = make_docker_request(
            service_type,
            http_request_method::POST,
            "/v1.38/containers/" + id + "/start",
            http_header_list());
        connection.perform_request(check_in, reporter, request);
    }

    return id;
}

static void
stop_provider(
    docker_service_type service_type,
    http_connection_interface& connection,
    string const& docker_container_id)
{
    spdlog::get("cradle")->info("[super] stop_provider");

    null_check_in check_in;
    null_progress_reporter reporter;

    try
    {
        auto request = make_docker_request(
            service_type,
            http_request_method::POST,
            "/v1.38/containers/" + docker_container_id + "/stop",
            {{"Content-Type", "application/json"},
             {"X-Registry-Auth",
              get_environment_variable("CRADLE_THINKNODE_DOCKER_AUTH")}},
            blob());
        connection.perform_request(check_in, reporter, request);
    }
    catch (...)
    {
        // If something goes wrong, we may have an orphaned container running
        // in Docker. Ideally we should do something about this, but this is
        // pretty low priority at the moment and any effort expended on this
        // right now might need to be redone anyway as this functionality
        // matures.
    }

    spdlog::get("cradle")->info("[super] (provider stopped)");
}

struct local_function_invocation
{
    string function_name;
    std::vector<dynamic> args;
};

template<class Value>
struct cv_value
{
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<Value> value;
};

template<class Value>
void
produce(cv_value<Value>& cvv, Value&& value)
{
    std::unique_lock<std::mutex> lock(cvv.mutex);
    cvv.value = std::move(value);
    cvv.cv.notify_one();
}

template<class Value>
Value
consume(cv_value<Value>& cvv)
{
    std::unique_lock<std::mutex> lock(cvv.mutex);
    cvv.cv.wait(lock, [&] { return cvv.value ? true : false; });
    Value value = std::move(*cvv.value);
    cvv.value.reset();
    return value;
}

template<class Value>
void
reset(cv_value<Value>& cvv)
{
    std::unique_lock<std::mutex> lock(cvv.mutex);
    cvv.value.reset();
}

enum class local_supervisor_state
{
    IDLE,
    AWAITING_REGISTRATION,
    AWAITING_RESULT,
    TERMINATED
};

struct local_provider_info
{
    string account;
    string app;
    string image_tag;
    string docker_id;
    string pid;
};

struct local_supervisor_data
{
    service_core& service;

    asio::io_service io_service;
    tcp::acceptor acceptor;
    std::unique_ptr<tcp::socket> socket;
    std::mutex socket_write_mutex;

    docker_service_type docker_type;

    std::atomic<local_supervisor_state> state;

    // valid if state_ is AWAITING_REGISTRATION
    optional<local_function_invocation> active_request;

    // the active Docker provider (limited to one per supervisor for now) -
    // This MUST be valid if state_ is AWAITING_<anything>, but MAY be valid
    // in the IDLE state as well.
    optional<local_provider_info> active_provider;

    cv_value<std::optional<dynamic>> result;

    local_supervisor_data(service_core& service)
        : service(service),
          io_service(),
          acceptor(
              io_service,
              tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 0)),
          state(local_supervisor_state::IDLE)
    {
        static local_calculation_service local_service;
        docker_type = local_service.get_docker_type(
            http_connection_for_thread(service));
    }
};

void
transmit_request(local_supervisor_data& supervisor)
{
    auto& request = *supervisor.active_request;
    supervisor.state = local_supervisor_state::AWAITING_RESULT;
    {
        std::scoped_lock<std::mutex> lock(supervisor.socket_write_mutex);
        write_message(
            *supervisor.socket,
            ipc_version,
            make_thinknode_supervisor_message_with_function(
                make_thinknode_supervisor_calculation_request(
                    request.function_name, std::move(request.args))));
    }
    supervisor.active_request.reset();
    spdlog::get("cradle")->info(
        "[super] {}: ARGUMENTS SENT!", (void*) &supervisor);
}

void
process_messages(std::shared_ptr<local_supervisor_data> data)
{
    local_supervisor_data& supervisor = *data;
    while (supervisor.state != local_supervisor_state::TERMINATED)
    {
        spdlog::get("cradle")->info("[super] supervisor accepting...");
        supervisor.socket
            = std::make_unique<tcp::socket>(supervisor.io_service);
        supervisor.acceptor.accept(*supervisor.socket);
        spdlog::get("cradle")->info(
            "[super] {}: ACCEPTED!", (void*) &supervisor);

        // TODO: Need better logic for this.
        bool on_same_provider = true;
        while (on_same_provider)
        {
            try
            {
                auto message = read_message<thinknode_provider_message>(
                    *supervisor.socket, ipc_version);

                switch (get_tag(message))
                {
                    case thinknode_provider_message_tag::REGISTRATION:
                        if (supervisor.state
                            == local_supervisor_state::AWAITING_REGISTRATION)
                        {
                            assert(
                                supervisor.active_provider
                                && supervisor.active_request);
                            if (as_registration(message).pid
                                == supervisor.active_provider->pid)
                            {
                                spdlog::get("cradle")->info(
                                    "{}: REGISTRATION!", (void*) &supervisor);
                                transmit_request(supervisor);
                            }
                        }
                        break;

                    case thinknode_provider_message_tag::RESULT:
                        spdlog::get("cradle")->info(
                            "{}: RESULT!", (void*) &supervisor);
                        supervisor.state = local_supervisor_state::IDLE;
                        produce<optional<dynamic>>(
                            supervisor.result,
                            some(as_result(std::move(message))));
                        break;

                    case thinknode_provider_message_tag::FAILURE:
                        spdlog::get("cradle")->info(
                            "{}: FAILURE!", (void*) &supervisor);
                        supervisor.state = local_supervisor_state::IDLE;
                        produce<optional<dynamic>>(supervisor.result, none);
                        break;

                    default:
                        break;
                }
            }
            catch (...)
            {
                // Assume the error is related to the provider, so
                // terminate it and signal an error.
                spdlog::get("cradle")->info("[super] (IMPLICIT) FAILURE!");
                // try
                // {
                //     this->stop_provider();
                // }
                // catch (...)
                // {
                // }
                on_same_provider = false;
                if (supervisor.state != local_supervisor_state::IDLE
                    && supervisor.state != local_supervisor_state::TERMINATED)
                {
                    produce<optional<dynamic>>(supervisor.result, none);
                    supervisor.state = local_supervisor_state::IDLE;
                }
            }
        }
    }
}

void
stop_provider(local_supervisor_data& supervisor)
{
    if (supervisor.active_provider)
    {
        cradle::stop_provider(
            supervisor.docker_type,
            http_connection_for_thread(supervisor.service),
            supervisor.active_provider->docker_id);
        supervisor.active_provider.reset();
    }
}

dynamic
supervise_calculation(
    local_supervisor_data& supervisor,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image,
    string const& function_name,
    std::vector<dynamic> args)
{
    spdlog::get("cradle")->info("[super] LOCAL CALC: {}", function_name);

    string const image_tag = extract_tag(image);

    // If we have an active provider, but it's not the right one (or it's
    // not in the right state), stop it.
    if (supervisor.active_provider
        && (supervisor.state != local_supervisor_state::IDLE
            || supervisor.active_provider->account != account
            || supervisor.active_provider->app != app
            || supervisor.active_provider->image_tag != image_tag))
    {
        spdlog::get("cradle")->info(
            "[super] INTERNAL ERROR: inconsistent provider");
        CRADLE_THROW(
            internal_check_failed() << internal_error_message_info(
                "INTERNAL ERROR: inconsistent provider"));
        // this->stop_provider();
    }

    supervisor.active_request = local_function_invocation{
        .function_name = function_name, .args = std::move(args)};

    reset(supervisor.result);

    // If we have an active provider, we can just send the message.
    if (supervisor.active_provider)
    {
        try
        {
            spdlog::get("cradle")->info(
                "{}: active provider", (void*) &supervisor);
            transmit_request(supervisor);
        }
        catch (...)
        {
            stop_provider(supervisor);
        }
    }

    // Otherwise, we need to spawn one and wait for registration.
    if (!supervisor.active_provider)
    {
        boost::uuids::random_generator gen;
        boost::uuids::uuid uuid = gen();
        string pid = boost::lexical_cast<std::string>(uuid);
        pid.erase(std::remove(pid.begin(), pid.end(), '-'), pid.end());

        supervisor.state = local_supervisor_state::AWAITING_REGISTRATION;

        supervisor.active_provider = local_provider_info{
            .account = account,
            .app = app,
            .image_tag = image_tag,
            .docker_id = "",
            .pid = pid};

        pull_image(
            supervisor.docker_type,
            http_connection_for_thread(supervisor.service),
            account,
            app,
            image);

        supervisor.active_provider->docker_id = spawn_provider(
            supervisor.docker_type,
            http_connection_for_thread(supervisor.service),
            account,
            app,
            image,
            supervisor.acceptor.local_endpoint().port(),
            pid);
    }

    optional<dynamic> result = consume(supervisor.result);
    if (!result)
        CRADLE_THROW(local_calculation_failure());
    spdlog::get("cradle")->info("[super] {}: got result", (void*) &supervisor);
    if (supervisor.state != local_supervisor_state::IDLE)
    {
        spdlog::get("cradle")->info(
            "[super] INTERNAL ERROR: inconsistent provider");
        CRADLE_THROW(
            internal_check_failed() << internal_error_message_info(
                "INTERNAL ERROR: inconsistent provider"));
    }
    return std::move(*result);
}

struct local_supervisor : noncopyable
{
    local_supervisor(service_core& service)
        : data_(std::make_shared<local_supervisor_data>(service)),
          message_thread_([&]() { process_messages(data_); })
    {
    }

    ~local_supervisor()
    {
        data_->state = local_supervisor_state::TERMINATED;
        stop_provider(*data_);
        message_thread_.detach();
    }

    dynamic
    supervise(
        string const& account,
        string const& app,
        thinknode_provider_image_info const& image,
        string const& function_name,
        std::vector<dynamic> args)
    {
        return supervise_calculation(
            *data_, account, app, image, function_name, std::move(args));
    }

 private:
    std::shared_ptr<local_supervisor_data> data_;
    std::thread message_thread_;
};

local_supervisor&
local_supervisor_for_thread(service_core& service)
{
    thread_local local_supervisor the_supervisor(service);
    return the_supervisor;
}

dynamic
supervise_thinknode_calculation(
    service_core& service,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image,
    string const& function_name,
    std::vector<dynamic> args)
{
    return local_supervisor_for_thread(service).supervise(
        account, app, image, function_name, std::move(args));
}

} // namespace cradle
