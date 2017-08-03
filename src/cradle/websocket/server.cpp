#include <cradle/websocket/server.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <cradle/disk_cache.hpp>
#include <cradle/io/json_io.hpp>
#include <cradle/websocket/messages.hpp>

typedef websocketpp::server<websocketpp::config::asio> ws_server_type;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace cradle {

struct client_connection
{
    std::string name;
};

struct websocket_server_impl
{
    int next_session_id = 1;
    ws_server_type ws;
    std::map<connection_hdl,client_connection,std::owner_less<connection_hdl>> connections;
    disk_cache cache;
};

void static
send(websocket_server_impl& server, connection_hdl hdl, websocket_server_message const& message)
{
    auto json = value_to_json(to_value(message));
    websocketpp::lib::error_code ec;
    server.ws.send(hdl, json, websocketpp::frame::opcode::text, ec);
    if (ec)
    {
        CRADLE_THROW(
            websocket_server_error() <<
                internal_error_message_info(ec.message()));
    }
}

void static
on_open(websocket_server_impl& server, connection_hdl hdl)
{
    server.connections[hdl] = client_connection();
}

void static
on_close(websocket_server_impl& server, connection_hdl hdl)
{
    server.connections.erase(hdl);
}

void static
on_message(
    websocket_server_impl& server,
    connection_hdl hdl,
    ws_server_type::message_ptr raw_message)
{
    auto& client = server.connections.at(hdl);
    websocket_client_message message;
    from_value(&message, parse_json_value(raw_message->get_payload()));
    switch (get_tag(message))
    {
     case websocket_client_message_tag::KILL:
      {
        server.ws.close(hdl, websocketpp::close::status::going_away, "killed");
        server.ws.stop();
        break;
      }
     case websocket_client_message_tag::REGISTRATION:
      {
        client.name = as_registration(message).name;
        break;
      }
     case websocket_client_message_tag::TEST:
      {
        websocket_test_response response;
        response.name = client.name;
        response.message = as_test(message).message;
        send(server, hdl, make_websocket_server_message_with_test(response));
        break;
      }
     case websocket_client_message_tag::CACHE_INSERT:
      {
        auto& insertion = as_cache_insert(message);
        server.cache.insert(insertion.key, insertion.value);
        break;
      }
     case websocket_client_message_tag::CACHE_QUERY:
      {
        auto const& key = as_cache_query(message);
        auto entry = server.cache.find(key);
        send(server, hdl,
            make_websocket_server_message_with_cache_response(
                make_websocket_cache_response(key, entry ? entry->value : none)));
        break;
      }
    }
}

void static
initialize(websocket_server_impl& server)
{
    server.cache.reset(make_disk_cache_config(none, 0x1'00'00'00'00));

    server.ws.clear_access_channels(websocketpp::log::alevel::all);
    server.ws.init_asio();
    server.ws.set_open_handler(
        [&](connection_hdl hdl)
        {
            on_open(server, hdl);
        });
    server.ws.set_close_handler(
        [&](connection_hdl hdl)
        {
            on_close(server, hdl);
        });
    server.ws.set_message_handler(
        [&](connection_hdl hdl, ws_server_type::message_ptr message)
        {
            on_message(server, hdl, message);
        });
}

websocket_server::websocket_server()
{
    impl_ = new websocket_server_impl;
    initialize(*impl_);
}

websocket_server::~websocket_server()
{
    delete impl_;
}

void
websocket_server::listen(uint16_t port)
{
    auto& server = *impl_;
    server.ws.listen(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("::1"), port));
    server.ws.start_accept();
}

void
websocket_server::run()
{
    auto& server = *impl_;
    server.ws.run();
}

}
