#include <cradle/websocket/client.h>
#include <cradle/websocket/server.h>

#include <thread>

#include <cradle/utilities/testing.h>

#include <cradle/encodings/base64.h>
#include <cradle/fs/utilities.h>
#include <cradle/websocket/messages.hpp>

using namespace cradle;

// TEST_CASE("websocket client/server", "[ws]")
// {
//     reset_directory("websocket_test_cache");
//     auto config = make_server_config(
//         service_config(
//             immutable_cache_config(0x40'00'00'00),
//             disk_cache_config("websocket_test_cache", 0x40'00'00'00),
//             2,
//             2,
//             2),
//         false,
//         41072);
//     websocket_server server(config);
//     server.listen();
//     std::thread server_thread([&]() { server.run(); });

//     optional<websocket_test_response> test_response;

//     {
//         websocket_client client;
//         client.set_message_handler([&](websocket_server_message const&
//                                            message) {
//             switch (get_tag(message.content))
//             {
//                 case
//                 server_message_content_tag::REGISTRATION_ACKNOWLEDGEMENT:
//                     client.send(make_websocket_client_message(
//                         "id0",
//                         make_client_message_content_with_cache_insert(
//                             make_websocket_cache_insert(
//                                 "test_key", "test_value"))));
//                     break;
//                 case
//                 server_message_content_tag::CACHE_INSERT_ACKNOWLEDGEMENT:
//                     REQUIRE(message.request_id == "id0");
//                     client.send(make_websocket_client_message(
//                         "id1",
//                         make_client_message_content_with_cache_query(
//                             "test_key")));
//                     break;
//                 case server_message_content_tag::CACHE_RESPONSE: {
//                     REQUIRE(message.request_id == "id1");
//                     auto response = as_cache_response(message.content);
//                     REQUIRE(response.key == "test_key");
//                     REQUIRE(response.value.has_value());
//                     REQUIRE(*response.value == "test_value");
//                     client.send(make_websocket_client_message(
//                         "id2",
//                         make_client_message_content_with_test(
//                             websocket_test_query{"Hello, Patches!"})));
//                     break;
//                 }
//                 case server_message_content_tag::TEST: {
//                     REQUIRE(message.request_id == "id2");
//                     test_response = as_test(message.content);
//                     client.send(make_websocket_client_message(
//                         "id3", make_client_message_content_with_kill(nil)));
//                     client.close();
//                     break;
//                 }
//                 default:
//                     FAIL("unexpected message");
//             }
//         });
//         client.set_open_handler([&]() {
//             client.send(make_websocket_client_message(
//                 "no_id",
//                 make_client_message_content_with_registration(
//                     make_websocket_registration_message(
//                         "Kasey", make_thinknode_session("abc", "def")))));
//         });
//         client.connect("ws://localhost:41072");
//         client.run();
//     }

//     server_thread.join();

//     REQUIRE(test_response);
//     REQUIRE(test_response->name == "Kasey");
//     REQUIRE(test_response->message == "Hello, Patches!");
// }
