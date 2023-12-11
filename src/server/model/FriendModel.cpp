#include "FriendModel.h"
#include "database.h"
#include <cstdlib>

// 添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "insert into friend values(%d, %d)",
        userid, friendid);

    MySQL mysql;
    if (mysql.connect()) {
        mysql.update(sql);
    }

}

// 返回用户好友列表
std::vector<User> FriendModel::query(int userid)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "select a.id,a.name,a.state from user as a inner join friend as b on b.friendid=a.id where b.userid=%d",
        userid);

    std::vector<User> friendVec;
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            // 把userid用户的所有离线消息放入offlineMessage数组中保存
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                friendVec.push_back(user);
            }

            mysql_free_result(res);
        }
    }

    return friendVec;
}