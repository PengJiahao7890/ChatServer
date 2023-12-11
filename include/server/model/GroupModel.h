#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "Group.h"
#include <string>
#include <vector>
#pragma once

class GroupModel {
public:
    // 创建群组
    bool createGroup(Group& group);

    // 用户加入群组
    void addGroup(int userid, int groupid, std::string role);

    // 查询用户所在群组信息
    std::vector<Group> queryGroups(int userid);

    // 根据指定的groupid查询群组用户id列表，除了userid本身
    // 用于群聊业务给群组其他成员转发消息
    std::vector<int> queryGroupsUsers(int userid, int groupid);
};

#endif