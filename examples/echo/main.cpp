#include "echo/echo.h"

#include <mini_muduo/inet_address.h>

using namespace mini_muduo;

int main() {
    EventLoop loop;
    const InetAddress listenAddr(2007);

    EchoServer server(&loop, listenAddr);

    server.start();
    loop.loop();

    return 0;
}
