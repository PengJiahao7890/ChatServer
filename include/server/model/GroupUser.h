#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "User.h"
#include <string>
#pragma once

class GroupUser : public User {
public:
    void setRole(std::string role) { role_ = role; }
    std::string getRole() { return role_; }

private:
    std::string role_;
};

#endif