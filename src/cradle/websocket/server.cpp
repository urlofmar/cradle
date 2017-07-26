#include <cradle/websocket/server.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <cradle/io/json_io.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace cradle {

struct client_connection
{
    std::string name;
};

struct websocket_server
{
    websocket_server()
    {
        server_.clear_access_channels(websocketpp::log::alevel::all);
        server_.init_asio();
        server_.set_open_handler(bind(&websocket_server::on_open, this, _1));
        server_.set_close_handler(bind(&websocket_server::on_close, this, _1));
        server_.set_message_handler(bind(&websocket_server::on_message, this, _1, _2));
    }

    void
    on_open(connection_hdl hdl)
    {
        connections_[hdl] = client_connection();
    }

    void
    on_close(connection_hdl hdl)
    {
        connections_.erase(hdl);
    }

    void
    on_message(connection_hdl hdl, server::message_ptr raw_message)
    {
        auto& client = connections_.at(hdl);
        websocket_client_message message;
        from_value(&message, parse_json_value(raw_message->get_payload()));
        switch (get_tag(message))
        {
         case websocket_client_message_tag::KILL:
          {
            server_.close(hdl, websocketpp::close::status::going_away, "killed");
            server_.stop();
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
            this->send(hdl, construct_websocket_server_message_with_test(response));
            break;
          }
        }
    }

    void
    send(connection_hdl hdl, websocket_server_message const& message)
    {
        auto json = value_to_json(to_value(message));
        websocketpp::lib::error_code ec;
        server_.send(hdl, json, websocketpp::frame::opcode::text, ec);
        if (ec)
        {
            CRADLE_THROW(
                websocket_server_error() <<
                    internal_error_message_info(ec.message()));
        }
    }

    void
    run(uint16_t port)
    {
        server_.listen(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("::1"), port));
        server_.start_accept();
        server_.run();
    }

 private:
    int next_session_id_ = 1;
    server server_;
    std::map<connection_hdl,client_connection,std::owner_less<connection_hdl>> connections_;
};

void
run_websocket_server(uint16_t port)
{
    websocket_server server;
    server.run(port);
}

}
