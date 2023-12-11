#ifndef PUBLIC_H
#define PUBLIC_H

#pragma once

// server和client的公共文件

enum EnMsgType {
    LOGIN_MSG = 1, // 登录
    LOGIN_MSG_ACK, // 登录响应
    REG_MSG, // 注册
    REG_MSG_ACK, // 注册响应

    ONE_CHAT_MSG, // 聊天消息
    ADD_FRIEND_MSG, // 添加好友

    CREATE_GROUP_MSG, // 创建群
    CREATE_GROUP_MSG_ACK, // 创建群响应
    ADD_GROUP_MSG, // 加入群
    GROUP_CHAT_MSG, // 群聊天

    LOGOUT_MSG, // 注销
};

#endif