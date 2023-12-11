#include "GroupModel.h"
#include "database.h"
#include <cstdlib>
#include <mysql/mysql.h>
#include <vector>

// 创建群组
bool GroupModel::createGroup(Group& group)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "insert into allgroup(groupname, groupdesc) values('%s', '%s')",
        group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

// 用户加入群组
void GroupModel::addGroup(int userid, int groupid, std::string role)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "insert into groupuser values(%d, %d, '%s')",
        groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect()) {
        mysql.update(sql);
    }
}

// 查询用户所在群组信息
std::vector<Group> GroupModel::queryGroups(int userid)
{
    // 1. 先根据userid在groupuser表中查询出该用户所属的群组信息
    // 2. 再根据群组信息，查询属于该群组的所有用户的userid，同时进行user表的联合查询，取出用户详细信息

    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "select a.id,a.groupname,a.groupdesc from allgroup as a \
        inner join \
        groupuser as b on a.id = b.groupid \
        where userid=%d",
        userid);

    std::vector<Group> groupVec;

    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row;

            // 已经查询出userid所在所有群组的信息
            while ((row = mysql_fetch_row(res)) != nullptr) {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 查询每个群组的用户信息
    for (Group& group : groupVec) {
        sprintf(sql,
            "select a.id,a.name,a.state,b.grouprole from user as a \
            inner join \
            groupuser as b on a.id = b.userid \
            where groupid=%d",
            group.getId());

        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row;

            // 已经查询出userid所在所有群组的信息
            while ((row = mysql_fetch_row(res)) != nullptr) {
                GroupUser groupUser;
                groupUser.setId(atoi(row[0]));
                groupUser.setName(row[1]);
                groupUser.setState(row[2]);
                groupUser.setRole(row[3]);

                // 把查询出来的信息放在Group中的GroupUser数组中
                group.getUser().push_back(groupUser);
            }
            mysql_free_result(res);
        }
    }

    // 保存了群信息以及一个群成员信息数组
    return groupVec;
}

// 根据指定的groupid查询群组用户id列表，除了userid本身
// 用于群聊业务给群组其他成员转发消息
std::vector<int> GroupModel::queryGroupsUsers(int userid, int groupid)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "select userid from groupuser where groupid=%d and userid!=%d",
        groupid, userid);

    std::vector<int> useridVec;

    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(res)) != nullptr) {
                useridVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }

    // 保存了除查询的userid本身外的其他群成员id
    return useridVec;
}