#include "Group.h"
#include "GroupUser.h"
#include "User.h"
#include "json.hpp"
#include "public.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <semaphore.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
std::vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
std::vector<Group> g_currentUserGroupList;
// 控制主菜单页面程序
bool isMainMenuRunnig = false;
// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
std::atomic_bool g_isLoginSuccess(false);
// 记录子线程启动状态
bool isSubThreadRunning = false;

// 接收线程操作函数
void readTaskHandler(int clientfd);
// 获取系统时间，用于给聊天信息添加时间戳
std::string getCurrentTime();
// 主聊天页面程序
void mainMenu(int);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "command invalid! example: ./ChatClient 127.0.0.1 9000" << std::endl;
        exit(-1);
    }

    // 解析命令传入的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) {
        std::cerr << "socket create error" << std::endl;
        exit(-1);
    }

    // 填写client需要连接的server信息 ip port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip); // 点分十进制ip转换为网络字节序

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in))) {
        std::cerr << "connect server error" << std::endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功，启动接收子线程，启动一次
    if (!isSubThreadRunning) {
        std::thread readTask(readTaskHandler, clientfd); // -> pthread_create
        readTask.detach(); // -> pthread_detach
        isSubThreadRunning = true;
    }

    // main线程用于接收用户输入，负责发送数据
    while (true) {
        // 显示首页菜单 登录、注册、退出
        std::cout << "=======================" << std::endl;
        std::cout << "1. login" << std::endl;
        std::cout << "2. register" << std::endl;
        std::cout << "3. quit" << std::endl;
        std::cout << "=======================" << std::endl;
        std::cout << "choice(number): ";
        int choice = 0;
        std::cin >> choice;
        std::cin.get(); // cin只读取了choice数字，没有读取缓冲区的回车，这里读取掉回车

        switch (choice) {
        case 1: { // login
            int id = 0;
            char pwd[50] = { 0 };
            std::cout << "userid: ";
            std::cin >> id;
            std::cin.get();
            std::cout << "userpassword: ";
            std::cin.getline(pwd, 50); // 可以输入空格，以回车结束

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            std::string request = js.dump(); // js序列化成string类型

            g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1) {
                std::cerr << "send login msg error: " << request << std::endl;
            }

            // 等待信号量，子线程处理完登录的响应信息后通知
            sem_wait(&rwsem);

            if (g_isLoginSuccess) {
                // 成功登录，进入聊天主菜单
                isMainMenuRunnig = true;
                mainMenu(clientfd);
            }
        } break;
        case 2: { // register
            char name[50] = { 0 };
            char pwd[50] = { 0 };
            std::cout << "username: ";
            std::cin.getline(name, 50); // 可以输入空格，以回车结束
            std::cout << "userpassword: ";
            std::cin.getline(pwd, 50); // 可以输入空格，以回车结束

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            std::string request = js.dump(); // js序列化成string类型

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1) {
                std::cerr << "send login msg error: " << request << std::endl;
            }

            // 等待信号量，子线程处理完注册的响应信息后通知
            sem_wait(&rwsem);
        } break;
        case 3: // quit
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            std::cerr << "invalid input!" << std::endl;
            break;
        }
    }

    return 0;
}

// 处理注册的响应
void doRegResponse(json& responsejs)
{
    if (0 != responsejs["errno"].get<int>()) {
        // 注册失败
        std::cerr << "用户名已存在，注册失败！" << std::endl;
    } else {
        // 注册成功
        std::cout << "注册成功，您的userid为: " << responsejs["id"] << std::endl;
    }
}

// 处理注册的响应
void doCreateGroupResponse(json& responsejs)
{
    if (0 != responsejs["errno"].get<int>()) {
        // 创建群失败
        std::cerr << "群名已被占用，群聊创建失败！" << std::endl;
    } else {
        // 注册成功
        std::cout << "创建群聊成功，您的groupId为: " << responsejs["groupId"] << std::endl;
    }
}

// 处理登录的响应
void doLoginResponse(json& responsejs)
{
    if (0 != responsejs["errno"].get<int>()) {
        std::cerr << responsejs["errmsg"] << std::endl;
        g_isLoginSuccess = false;
    } else { // 登录成功
        // 记录当前用户的id和name
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // 记录当前用户的好友列表信息
        if (responsejs.contains("friends")) {
            g_currentUserFriendList.clear();

            std::vector<std::string> friendsVec = responsejs["friends"];
            for (std::string& friendsStr : friendsVec) {
                json js = json::parse(friendsStr); // string反序列化为js
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if (responsejs.contains("groups")) {
            g_currentUserGroupList.clear();

            std::vector<std::string> groupsVec = responsejs["groups"];
            for (std::string& groupsStr : groupsVec) {
                json groupjs = json::parse(groupsStr); // string反序列化为js
                Group group;

                // 群组信息
                group.setId(groupjs["groupId"].get<int>());
                group.setName(groupjs["groupName"]);
                group.setDesc(groupjs["groupDesc"]);

                // 群组所有用户信息
                std::vector<std::string> groupUserVec = groupjs["users"];
                for (std::string& userStr : groupUserVec) {
                    GroupUser user;
                    json js = json::parse(userStr);
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUser().push_back(user);
                }

                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息
        if (responsejs.contains("offlinemsg")) {
            std::vector<std::string> offmsgVec = responsejs["offlinemsg"];
            for (std::string& str : offmsgVec) {
                json js = json::parse(str);
                // 格式
                // time [id] name said: msg
                if (ONE_CHAT_MSG == js["msgid"].get<int>()) {
                    std::cout << "私聊 "
                              << js["time"].get<std::string>()
                              << " [" << js["fromId"] << "] "
                              << js["fromName"].get<std::string>()
                              << " said: "
                              << js["msg"].get<std::string>() << std::endl;
                } else {
                    std::cout << "群消息[" << js["groupId"] << "]:"
                              << js["time"].get<std::string>()
                              << " [" << js["id"] << " ]"
                              << " said: "
                              << js["msg"].get<std::string>() << std::endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 子线程用于接收数据
void readTaskHandler(int clientfd)
{
    while (true) {
        char buffer[1024] = { 0 };
        // 在此处阻塞等待消息传递
        int len = recv(clientfd, buffer, 1024, 0);
        if (-1 == len || 0 == len) {
            close(clientfd);
            exit(-1);
        }

        if (len > 0) {

            // 接收ChatServer转发过来的数据，反序列化生成json对象
            json js = json::parse(buffer);

            try {
                int msgtype = js["msgid"].get<int>();

                if (ONE_CHAT_MSG == msgtype) {
                    std::cout << "私聊 "
                              << js["time"].get<std::string>()
                              << " [" << js["fromId"] << "] "
                              << js["fromName"].get<std::string>()
                              << " said: "
                              << js["msg"].get<std::string>() << std::endl;
                    continue;
                }

                if (GROUP_CHAT_MSG == msgtype) {
                    std::cout << "群消息[" << js["groupId"] << "]:"
                              << js["time"].get<std::string>()
                              << " [" << js["id"] << "]"
                              << " said: "
                              << js["msg"].get<std::string>() << std::endl;
                    continue;
                }

                if (CREATE_GROUP_MSG_ACK == msgtype) {
                    doCreateGroupResponse(js);
                    continue;
                }

                if (LOGIN_MSG_ACK == msgtype) {
                    // 处理登录响应
                    doLoginResponse(js);

                    // 通知主线程登录完成
                    sem_post(&rwsem);
                    continue;
                }

                if (REG_MSG_ACK == msgtype) {
                    doRegResponse(js);
                    sem_post(&rwsem);
                    continue;
                }

            } catch (const std::exception& e) {
                std::cerr << "JSON 解析错误: " << e.what() << std::endl;
            }
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    std::cout << "======================login user======================" << std::endl;
    std::cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << std::endl;
    std::cout << "----------------------friend list---------------------" << std::endl;
    if (!g_currentUserFriendList.empty()) {
        for (User& user : g_currentUserFriendList) {
            std::cout << user.getId() << " " << user.getName() << " " << user.getState() << std::endl;
        }
    }
    std::cout << "----------------------group list----------------------" << std::endl;
    if (!g_currentUserGroupList.empty()) {
        for (Group& group : g_currentUserGroupList) {
            std::cout << group.getId() << " " << group.getName() << " " << group.getDesc() << std::endl;
            for (GroupUser& user : group.getUser()) {
                std::cout << "  " << user.getId() << " " << user.getName() << " " << user.getState()
                          << " " << user.getRole() << std::endl;
            }
        }
    }
    std::cout << "======================================================" << std::endl;
}

// 通过命令行输入，调用函数实现对应功能
void help(int fd = 0, std::string str = "");
void chat(int, std::string);
void addFriend(int, std::string);
void createGroup(int, std::string);
void addGroup(int, std::string);
void groupChat(int, std::string);
void logout(int, std::string str);

// 客户端命令列表
std::unordered_map<std::string, std::string> commandMap = {
    { "help", "显示所有支持的命令，格式help" },
    { "chat", "一对一聊天，格式chat-friendId:message" },
    { "addFriend", "添加好友，格式addFriend-friendId" },
    { "createGroup", "创建群组，格式createGroup-groupName-groupDesc" },
    { "addGroup", "用户加入群组，格式addGroup-groupId" },
    { "groupChat", "群聊，格式groupChat-groupId:message" },
    { "logout", "注销，格式logout" }
};

// 客户端命令处理列表
std::unordered_map<std::string, std::function<void(int, std::string)>> commandHandlerMap = {
    { "help", help },
    { "chat", chat },
    { "addFriend", addFriend },
    { "createGroup", createGroup },
    { "addGroup", addGroup },
    { "groupChat", groupChat },
    { "logout", logout }
};

// 登录后启动聊天页面
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = { 0 };
    while (isMainMenuRunnig) {
        std::cin.getline(buffer, 1024);

        std::string commandBuf(buffer);
        std::string command; // 用于存储命令

        int idx = commandBuf.find("-");
        if (-1 == idx) {
            // 没有"-"的是help和logout
            command = commandBuf;
        } else {
            // "-"分隔命令
            command = commandBuf.substr(0, idx);
        }

        // 找到对应的处理函数
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end()) {
            std::cerr << "无效命令，请重新输入!" << std::endl;
            continue;
        }

        // 调用响应命令的事件处理函数回调
        it->second(clientfd, commandBuf.substr(idx + 1, commandBuf.size() - idx));
    }
}

void help(int fd, std::string str)
{
    std::cout << "show command list >>>" << std::endl;
    for (auto& p : commandMap) {
        std::cout << p.first << " : " << p.second << std::endl;
    }
    std::cout << std::endl;
}

void chat(int clientfd, std::string str)
{
    // str格式为 friendId:message
    int idx = str.find(":");
    if (-1 == idx) {
        std::cerr << "chat命令错误!" << std::endl;
        return;
    }

    int friendId = atoi(str.substr(0, idx).c_str());
    std::string message = str.substr(idx + 1, str.size() - idx);

    // 组装json
    // 格式--msgid fromId fromName toId msg
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["fromId"] = g_currentUser.getId();
    js["fromName"] = g_currentUser.getName();
    js["toId"] = friendId;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    std::string buffer = js.dump(); // 序列化成string

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        std::cerr << "发送消息失败:" << buffer << std::endl;
    }
}

void addFriend(int clientfd, std::string str)
{
    int friendId = atoi(str.c_str());

    // 组装json
    // msgid id friendId
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendId"] = friendId;
    std::string buffer = js.dump(); // 序列化成string

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        std::cerr << "添加好友失败:" << buffer << std::endl;
    }
}

void createGroup(int clientfd, std::string str)
{
    // str格式为 groupName - groupDesc
    int idx = str.find("-");
    if (-1 == idx) {
        std::cerr << "createGroup命令错误!" << std::endl;
        return;
    }

    std::string groupName = str.substr(0, idx);
    std::string groupDesc = str.substr(idx + 1, str.size() - idx);

    // 组装json
    // 格式--msgid id groupName groupDesc
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupName"] = groupName;
    js["groupDesc"] = groupDesc;
    std::string buffer = js.dump(); // 序列化成string

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        std::cerr << "创建群聊失败:" << buffer << std::endl;
    }
}

void addGroup(int clientfd, std::string str)
{
    int groupId = atoi(str.c_str());

    // 组装json
    // msgid id groupId
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupId"] = groupId;
    std::string buffer = js.dump(); // 序列化成string

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        std::cerr << "加入群聊失败:" << buffer << std::endl;
    }
}

void groupChat(int clientfd, std::string str)
{
    // str格式为 groupId:message
    int idx = str.find(":");
    if (-1 == idx) {
        std::cerr << "groupChat命令错误!" << std::endl;
        return;
    }

    int groupId = atoi(str.substr(0, idx).c_str());
    std::string message = str.substr(idx + 1, str.size() - idx);

    // 组装json
    // 格式--msgid id name groupId msg time
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupId"] = groupId;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    std::string buffer = js.dump(); // 序列化成string

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        std::cerr << "发送群消息失败:" << buffer << std::endl;
    }
}

void logout(int clientfd, std::string)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        std::cerr << "注销失败:" << buffer << std::endl;
    } else {
        isMainMenuRunnig = false;
    }
}

// 获取当前的系统时间
std::string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date[60] = { 0 };
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
        (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
        (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}