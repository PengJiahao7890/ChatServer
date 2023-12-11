#include "ChatServer.h"
#include "ChatService.h"
#include "json.hpp"

#include <functional>
#include <string>

using json = nlohmann::json;

ChatServer::ChatServer(EventLoop* loop,
    const InetAddress& listenAddr,
    const string& nameArg)
    : server_(loop, listenAddr, nameArg)
    , loop_(loop)
{
    // 注册连接回调
    server_.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, std::placeholders::_1));

    // 注册消息回调
    server_.setMessageCallback(
        std::bind(&ChatServer::onMessage, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

// 连接后使用的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    // 客户端断开连接，关闭连接释放资源，底层调用connectionDestory
    if (!conn->connected()) {
        // 客户端异常关闭处理
        ChatService::instance()->clientCloseException(conn);

        // 关闭连接
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn,
    Buffer* buffer,
    Timestamp receivetime)
{
    std::string buf = buffer->retrieveAllAsString();

    // 数据反序列化
    json js = json::parse(buf);

    // 通过回调函数，完全解耦网络模块的代码和业务模块的代码
    // js中不同的数据都绑定不同的回调函数，有数据到来，调用回调函数
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息类型绑定好的事件处理器，处理相应的业务
    msgHandler(conn, js, receivetime);
}