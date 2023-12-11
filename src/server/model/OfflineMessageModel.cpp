#include "OfflineMessageModel.h"
#include "database.h"

#include <mysql/mysql.h>

// 存储用户的离线消息
bool OfflineMessageModel::insert(int userid, std::string msg)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "insert into offlinemessage values(%d, '%s')",
        userid, msg.c_str());

    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }

    return false;
}

// 删除用户的离线消息
bool OfflineMessageModel::remove(int userid)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "delete from offlinemessage where userid=%d",
        userid);

    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            // 获取插入成功的用户数据生成的id
            return true;
        }
    }

    return false;
}

// 查询用户的离线消息
std::vector<std::string> OfflineMessageModel::query(int userid)
{
    // 组装sql语句
    char sql[1024] = { 0 };
    sprintf(sql,
        "select message from offlinemessage where userid=%d",
        userid);

    std::vector<std::string> offlineMessage;
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            // 把userid用户的所有离线消息放入offlineMessage数组中保存
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                offlineMessage.push_back(row[0]);
            }

            mysql_free_result(res);
        }
    }

    return offlineMessage;
}