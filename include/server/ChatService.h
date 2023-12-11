#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "FriendModel.h"
#include "GroupModel.h"
#include "OfflineMessageModel.h"
#include "UserModel.h"
#include "redis.h"

#include <json.hpp>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TcpConnection.h>

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#pragma once

using json = nlohmann::json;
using namespace muduo;
using namespace muduo::net;

// 处理消息事件的回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr& conn,
    json& js,
    Timestamp receivetime)>;

// 聊天服务器业务类
// 业务操作集成在这个类中，采用单例模式
class ChatService {
public:
    // 获取单例对象的接口函数
    static ChatService* instance();

    // 处理登录业务
    void login(const TcpConnectionPtr& conn,
        json& js,
        Timestamp receivetime);

    // 处理注册业务
    void reg(const TcpConnectionPtr& conn,
        json& js,
        Timestamp receivetime);

    // 处理注销业务
    void logout(const TcpConnectionPtr& conn,
        json& js,
        Timestamp receivetime);

    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn,
        json& js,
        Timestamp time);

    // 添加好友业务
    void addFriend(const TcpConnectionPtr& conn,
        json& js,
        Timestamp time);

    // 创建群组业务
    void createGroup(const TcpConnectionPtr& conn,
        json& js,
        Timestamp time);

    // 用户加入群组业务
    void addGroup(const TcpConnectionPtr& conn,
        json& js,
        Timestamp time);

    // 群聊业务
    void groupChat(const TcpConnectionPtr& conn,
        json& js,
        Timestamp time);

    // 获取消息对应的Handler
    MsgHandler getHandler(int msgid);

    // redis上报消息的回调
    void handleRedisSubscribeMessage(int userid, std::string msg);

    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);

    // 处理服务器异常退出
    void reset();

private:
    ChatService(); // 单例模式构造函数私有化

    // 存储消息id和对应的业务处理方法
    std::unordered_map<int, MsgHandler> msgHandlerMap_;

    // 存储在线用户的通信连接
    // 一个客户端发送消息后由服务器将消息推送给对象，因此要保存登录用户的连接
    // 多线程环境用执行要解决线程安全问题
    std::unordered_map<int, TcpConnectionPtr> userConnMap_;

    // 定义互斥锁，保证userConnMap_的线程安全
    std::mutex connMutex_;

    // 数据操作类对象
    UserModel userModel_;
    OfflineMessageModel offlineMessageModel_;
    FriendModel friendModel_;
    GroupModel groupModel_;

    // redis操作对象
    Redis redis_;
};

#endif