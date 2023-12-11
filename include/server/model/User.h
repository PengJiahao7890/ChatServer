#ifndef USER_H
#define USER_H

#include <string>
#pragma once

class User {
public:
    User(int id = -1, std::string name = "", std::string pwd = "", std::string state = "offline")
        : id_(id)
        , name_(name)
        , password_(pwd)
        , state_(state)
    {
    }

    void setId(int id) { id_ = id; }
    void setName(std::string name) { name_ = name; }
    void setPwd(std::string pwd) { password_ = pwd; }
    void setState(std::string state) { state_ = state; }

    int getId() { return id_; }
    std::string getName() { return name_; }
    std::string getPwd() { return password_; }
    std::string getState() { return state_; }

private:
    int id_; // 用于id
    std::string name_; // 用户名
    std::string password_; // 用户密码
    std::string state_; // 当前登录状态
};

#endif