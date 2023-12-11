#ifndef USERMODEL_H
#define USERMODEL_H

#include "User.h"

#pragma once

// User类的数据操作方法类
class UserModel
{
public:

    // User表的增加成员
    bool insert(User& user);

    // 根据用户id查询用户信息
    User query(int id);

    // 更新用户信息
    bool updateState(User user);

    // 重置用户的状态信息
    void resetState();

};

#endif