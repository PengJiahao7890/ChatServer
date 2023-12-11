#include "ChatService.h"
#include "public.h"

#include <muduo/base/Logging.h>

#include <functional>
#include <mutex>
#include <string>
#include <vector>
using namespace muduo;

// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 私有构造函数
// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    msgHandlerMap_.insert({ LOGIN_MSG,
        std::bind(&ChatService::login, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    msgHandlerMap_.insert({ REG_MSG,
        std::bind(&ChatService::reg, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    msgHandlerMap_.insert({ LOGOUT_MSG,
        std::bind(&ChatService::logout, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    msgHandlerMap_.insert({ ONE_CHAT_MSG,
        std::bind(&ChatService::oneChat, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    msgHandlerMap_.insert({ ADD_FRIEND_MSG,
        std::bind(&ChatService::addFriend, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    msgHandlerMap_.insert({ CREATE_GROUP_MSG,
        std::bind(&ChatService::createGroup, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    msgHandlerMap_.insert({ ADD_GROUP_MSG,
        std::bind(&ChatService::addGroup, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    msgHandlerMap_.insert({ GROUP_CHAT_MSG,
        std::bind(&ChatService::groupChat, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });

    // 连接redis服务器
    if (redis_.connect()) {
        // 设置上报消息的回调
        redis_.init_notify_handel(std::bind(&ChatService::handleRedisSubscribeMessage,
            this, std::placeholders::_1, std::placeholders::_2));
    }
}

// 获取消息对应的Handler
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志
    auto it = msgHandlerMap_.find(msgid);
    if (it == msgHandlerMap_.end()) {
        // 返回默认处理器，空操作
        return [=](const TcpConnectionPtr& conn,
                   json& js,
                   Timestamp receivetime) { LOG_ERROR << "msgid: " << msgid << "can not find handler!"; };
    } else {
        return msgHandlerMap_[msgid];
    }
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn,
    json& js,
    Timestamp receivetime)
{
    int id = js["id"].get<int>();
    std::string pwd = js["password"];

    User user = userModel_.query(id);
    // 验证密码是否正确
    if (user.getId() == id && user.getPwd() == pwd) {
        if (user.getState() == "offline") {
            // 登录成功，记录用户连接信息
            {
                // lock_guard构造函数加锁 析构函数解锁 出作用域即解锁
                std::lock_guard<std::mutex> lock(connMutex_);
                userConnMap_.insert({ id, conn });
            }

            // 用户登录成功后，向redis订阅Channel，即id号
            redis_.subscribe(id);

            // 登录成功，更新用户状态信息
            user.setState("online");
            userModel_.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0; // 为0表示没有错误
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息
            std::vector<std::string> offlineMessage = offlineMessageModel_.query(id);
            if (!offlineMessage.empty()) {

                response["offlinemsg"] = offlineMessage;

                // 读取完离线消息后，把用户的所有离线消息删除
                offlineMessageModel_.remove(id);
            }

            // 查询该用户的好友信息并返回
            std::vector<User> friendVec = friendModel_.query(id);
            if (!friendVec.empty()) {
                // 将User类成员转成string
                std::vector<std::string> vec;
                for (User& user : friendVec) {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec.push_back(js.dump());
                }
                response["friends"] = vec;
            }

            // 查询用户群组信息
            std::vector<Group> groupUserVec = groupModel_.queryGroups(id);
            if (!groupUserVec.empty()) {
                std::vector<std::string> groupVec;
                for (Group& group : groupUserVec) {
                    json groupJs;
                    groupJs["groupId"] = group.getId();
                    groupJs["groupName"] = group.getName();
                    groupJs["groupDesc"] = group.getDesc();

                    std::vector<std::string> userVec;
                    for (GroupUser& user : group.getUser()) {
                        json userJs;
                        userJs["id"] = user.getId();
                        userJs["name"] = user.getName();
                        userJs["state"] = user.getState();
                        userJs["role"] = user.getRole();

                        userVec.push_back(userJs.dump());
                    }

                    groupJs["users"] = userVec;
                    groupVec.push_back(groupJs.dump());
                }

                response["groups"] = groupVec;
            }

            conn->send(response.dump());

        } else if (user.getState() == "online") {
            // 用户已经登录，不允许再次登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2; // 为0表示没有错误
            response["errmsg"] = "该用户已登录";
            conn->send(response.dump());
        }

    } else {
        // 用户不存在或者密码错误 登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1; // 为0表示没有错误
        response["errmsg"] = "用户名或密码错误";
        conn->send(response.dump());
    }
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn,
    json& js,
    Timestamp receivetime)
{
    std::string name = js["name"];
    std::string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);

    if (userModel_.insert(user)) {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0; // 为0表示没有错误
        response["id"] = user.getId();
        conn->send(response.dump());

    } else {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1; // 非0表示有错误
        response["errmsg"] = "register failed";
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::logout(const TcpConnectionPtr& conn,
    json& js,
    Timestamp receivetime)
{
    int userid = js["id"].get<int>();

    {
        // 注销后把连接总连接表中清除
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end()) {
            userConnMap_.erase(it);
        }
    }

    // 用户下线时，在redis中取消订阅通道
    redis_.unsubscribe(userid);

    // 更新用户状态信息，updateState只更改对应id的state
    User user(userid, "", "", "offline");
    userModel_.updateState(user);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    User user;

    {
        // 对userConnMap_表操作，加锁保证线程安全
        std::lock_guard<std::mutex> lock(connMutex_);

        // 遍历userConnMap_表
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it) {
            if (it->second == conn) {
                // 从map表中删除用户连接
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }

    // 用户下线时，在redis中取消订阅通道
    redis_.unsubscribe(user.getId());

    // 更新用户的状态信息
    if (user.getId() != -1) {
        user.setState("offline");
        userModel_.updateState(user);
    }
}

// 处理服务器异常退出
void ChatService::reset()
{
    // 把online状态的用户重置成offline
    userModel_.resetState();
}

// 一对一聊天业务
//  格式--msgid fromId fromName toId msg
//
void ChatService::oneChat(const TcpConnectionPtr& conn,
    json& js,
    Timestamp time)
{

    int toId = js["toId"].get<int>();

    {
        std::lock_guard<std::mutex> lock(connMutex_);

        auto it = userConnMap_.find(toId);
        if (it != userConnMap_.end()) {
            // toId在线，转发消息，（用户在同一台服务器中）
            // 服务器主动推送消息给toId用户 it->second就是连接
            it->second->send(js.dump());
            return;
        }
    }

    // 可能在其他服务器中在线
    // 在数据库中查询在线状态
    User user = userModel_.query(toId);
    if (user.getState() == "online") {
        // 通过redis发布消息，跨服务器通信
        redis_.publish(toId, js.dump());
        return;
    }

    // toId不在线，存储离线消息
    offlineMessageModel_.insert(toId, js.dump());
}

// 添加好友业务 msgid id friendId
void ChatService::addFriend(const TcpConnectionPtr& conn,
    json& js,
    Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendId"].get<int>();

    // 存储好友信息
    friendModel_.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn,
    json& js,
    Timestamp time)
{
    int userid = js["id"].get<int>();
    std::string name = js["groupName"];
    std::string desc = js["groupDesc"];

    // 保存群组信息
    Group group(-1, name, desc);

    if (groupModel_.createGroup(group)) {
        // 在allgroup表创建群组成功则把用户加入群组，角色为创建者，存如groupuser表
        groupModel_.addGroup(userid, group.getId(), "creator");

        json response;
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 0; // 为0表示没有错误
        response["groupId"] = group.getId();

        conn->send(response.dump());
    } else {
        // 创建群聊失败
        json response;
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 1; // 非0表示有错误
        response["errmsg"] = "create group failed";
        conn->send(response.dump());
    }
}

// 用户加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn,
    json& js,
    Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupId"].get<int>();

    groupModel_.addGroup(userid, groupid, "normal");
}

// 群聊业务
void ChatService::groupChat(const TcpConnectionPtr& conn,
    json& js,
    Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupId"].get<int>();

    // 找到所在群组其他人的id
    std::vector<int> useridVec = groupModel_.queryGroupsUsers(userid, groupid);

    std::lock_guard<std::mutex> lock(connMutex_);
    for (int id : useridVec) {
        // 在userConnMap_连接表中找到相应id的连接，用于转发消息
        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end()) {
            // 在线直接转发
            it->second->send(js.dump());
        } else {
            // 可能在其他服务器中在线
            // 在数据库中查询在线状态
            User user = userModel_.query(id);
            if (user.getState() == "online") {
                // 通过redis发布消息，跨服务器通信
                redis_.publish(id, js.dump());
            } else {
                // 离线则存到离线消息表中
                offlineMessageModel_.insert(id, js.dump());
            }
        }
    }
}

// redis上报消息的回调 在observer_channel_message中调用
// 有消息之后redis会上报userid对应服务器
// 在对应服务器中找到连接并发送消息
void ChatService::handleRedisSubscribeMessage(int userid, std::string msg)
{
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end()) {
        it->second->send(msg);
        return;
    }

    // 不在线，存储用户离线消息
    offlineMessageModel_.insert(userid, msg);
}