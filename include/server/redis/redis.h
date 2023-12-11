#ifndef REDIS_H
#define REDIS_H

#include <functional>
#include <hiredis/hiredis.h>
#include <string>

#pragma once

class Redis {
public:
    Redis();
    ~Redis();

    // 连接Redis服务器
    bool connect();

    // 向Redis指定通道Channel发布消息
    bool publish(int channel, std::string message);

    // 向Redis指定通道Channel订阅消息
    bool subscribe(int channel);

    // 向Redis指定通道Channel取消订阅
    bool unsubscribe(int channel);

    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();

    // 初始化 向service上报通道消息的回调
    void init_notify_handel(std::function<void(int, std::string)> func);

private:
    // hiredis同步上下文对象，处理publish消息
    redisContext* publish_context_;

    // hiredis同步上下文对象，处理subscribe消息
    redisContext* subscribe_context_;

    // 回调操作，收到订阅的消息，给service上报
    std::function<void(int, std::string)> notify_message_handler_;
};

#endif