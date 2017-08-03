#include <cradle/websocket/server.hpp>

using namespace cradle;

int main()
{
    websocket_server server;
    server.listen(41071);
    server.run();
    return 0;
}
