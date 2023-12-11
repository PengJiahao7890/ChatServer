#ifndef GROUP_H
#define GROUP_H

#include "GroupUser.h"

#include <string>
#include <vector>
#pragma once

class Group {
public:
    Group(int id = -1, std::string name = "", std::string desc = "")
        : id_(id)
        , name_(name)
        , desc_(desc)
    {
    }

    void setId(int id) { id_ = id; }
    void setName(std::string name) { name_ = name; }
    void setDesc(std::string desc) { desc_ = desc; }

    int getId() { return id_; }
    std::string getName() { return name_; }
    std::string getDesc() { return desc_; }
    std::vector<GroupUser>& getUser() { return user_; }

private:
    int id_; // 组id
    std::string name_; // 组名
    std::string desc_; // 组描述
    std::vector<GroupUser> user_; // 组成员
};

#endif