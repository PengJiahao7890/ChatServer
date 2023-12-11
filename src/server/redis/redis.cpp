#include "redis.h"
#include <cstdlib>
#include <hiredis/hiredis.h>
#include <iostream>
#include <thread>

Redis::Redis()
    : publish_context_(nullptr)
    , subscribe_context_(nullptr)
{
}

Redis::~Redis()
{
    if (publish_context_ != nullptr) {
        redisFree(publish_context_);
    }
    if (subscribe_context_ != nullptr) {
        redisFree(subscribe_context_);
    }
}

// 连接Redis服务器
bool Redis::connect()
{
    publish_context_ = redisConnect("127.0.0.1", 6379);
    if (nullptr == publish_context_) {
        std::cerr << "connect redis failed! publish_context_ failed." << std::endl;
        return false;
    }

    subscribe_context_ = redisConnect("127.0.0.1", 6379);
    if (nullptr == subscribe_context_) {
        std::cerr << "connect redis failed! subscribe_context_ failed." << std::endl;
        return false;
    }

    // 在单独的线程中专门监听通道上的事件，有消息给service上报
    std::thread t([&]() { observer_channel_message(); });
    t.detach();

    std::cout << "connect redis-server success!" << std::endl;

    return true;
}

// 向Redis指定通道Channel发布消息
bool Redis::publish(int channel, std::string message)
{
    redisReply* reply = (redisReply*)redisCommand(publish_context_,
        "PUBLISH %d %s", channel, message.c_str());
    if (nullptr == reply) {
        std::cerr << "publish command failed!" << std::endl;
        return false;
    }
    // 发布完消息即可释放
    freeReplyObject(reply);
    return true;
}

// 向Redis指定通道Channel订阅消息
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    // 最后redisGetReply是阻塞接收，放到单独线程中进行
    if (REDIS_ERR == redisAppendCommand(this->subscribe_context_, "SUBSCRIBE %d", channel)) {
        std::cerr << "subscribe command failed!" << std::endl;
        return false;
    }

    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->subscribe_context_, &done)) {
            std::cerr << "subscribe command failed!" << std::endl;
            return false;
        }
    }

    return true;
}

// 向Redis指定通道Channel取消订阅
bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(this->subscribe_context_, "UNSUBSCRIBE %d", channel)) {
        std::cerr << "unsubscribe command failed!" << std::endl;
        return false;
    }

    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->subscribe_context_, &done)) {
            std::cerr << "unsubscribe command failed!" << std::endl;
            return false;
        }
    }

    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply* reply = nullptr;
    while (REDIS_OK == redisGetReply(this->subscribe_context_, (void**)&reply)) {
        // 收到的reply是一个三元素的数组
        // 类似message channel "message"
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr) {
            // 给service上报通道发生的消息
            notify_message_handler_(std::atoi(reply->element[1]->str), reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    std::cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<" << std::endl;
}

// 向service上报通道消息的回调
void Redis::init_notify_handel(std::function<void(int, std::string)> func)
{
    this->notify_message_handler_ = func;
}