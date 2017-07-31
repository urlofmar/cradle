#include <cradle/websocket/client.hpp>
#include <cradle/websocket/server.hpp>

#include <thread>
#include <chrono>

#include <cradle/io/base64.hpp>

using namespace cradle;

TEST_CASE("websocket client/server", "[disk_cache]")
{
    std::thread server_thread([](){ run_websocket_server(41072); });
    // Give the server time to start.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    optional<websocket_test_response> response;

    {
        websocket_client client;
        client.set_message_handler(
            [&](websocket_server_message const& message)
            {
                response = as_test(message);
                client.send(
                    make_websocket_client_message_with_kill(nil));
                client.close();
            });
        client.set_open_handler(
            [&]()
            {
                client.send(
                    make_websocket_client_message_with_registration(
                        websocket_registration_message{"Kasey"}));
                client.send(
                    make_websocket_client_message_with_test(
                        websocket_test_query{"Hello, Patches!"}));
            });
        client.connect("ws://localhost:41072");
        client.run();
    }

    server_thread.join();

    REQUIRE(response);
    REQUIRE(response->name == "Kasey");
    REQUIRE(response->message == "Hello, Patches!");
}
