#include "ChatServer.h"
#include "ChatService.h"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <signal.h>

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char** argv)
{
    // Ctrl+c结束服务器时处理
    signal(SIGINT, resetHandler);

    if (argc < 3) {
        std::cerr << "command invalid! example: ./ChatServer 127.0.0.1 9000" << std::endl;
        exit(-1);
    }
    // 解析命令传入的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    EventLoop loop;
    InetAddress listenAddr(ip, port);
    ChatServer server(&loop, listenAddr, "ChatServer");

    server.setThreadNum(8);
    server.start();
    loop.loop();

    return 0;
}