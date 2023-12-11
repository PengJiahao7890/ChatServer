#ifndef CHATSERVER_H
#define CHATSERVER_H

#pragma once

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
using namespace muduo;
using namespace muduo::net;

class ChatServer {
public:
    ChatServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const string& nameArg);

    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    void start() { server_.start(); }

private:
    // 连接后使用的回调函数
    void onConnection(const TcpConnectionPtr&);

    // 上报读写事件相关信息的回调函数
    void onMessage(const TcpConnectionPtr&,
        Buffer*,
        Timestamp);

    TcpServer server_;  // muduo库提供的服务器对象
    EventLoop* loop_;   // 事件循环对象的指针
};

#endif