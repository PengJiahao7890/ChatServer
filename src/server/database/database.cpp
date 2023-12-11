#include "database.h"

#include <muduo/base/Logging.h>
#include <mysql/mysql.h>
using namespace muduo;

// 数据库配置信息
static std::string server = "127.0.0.1";
static std::string user = "root";
static std::string password = "123456";
static std::string dbname = "chatServer";

// 初始化数据库连接
MySQL::MySQL()
{
    conn_ = mysql_init(nullptr);
}

// 释放数据库连接资源
MySQL::~MySQL()
{
    if (conn_ != nullptr)
        mysql_close(conn_);
}

// 连接数据库
bool MySQL::connect()
{
    MYSQL* p = mysql_real_connect(conn_, server.c_str(), user.c_str(),
        password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr) {
        // C/C++代码默认编码字符为ASCII，如果不设置，从Mysql上拉下来的中文字符显示问号
        // mysql_query(conn_, "set names gbk");  // mysql 8.0自带的utf-8即可
        LOG_INFO << __FILE__ << ":" << __LINE__ << ": connect mysql success!";
    } else {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ": connect mysql failed!";
    }
    return p;
}

// 更新操作
bool MySQL::update(std::string sql)
{
    if (mysql_query(conn_, sql.c_str())) {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "更新失败:" << mysql_error(conn_);
        return false;
    }
    return true;
}

// 查询操作
MYSQL_RES* MySQL::query(std::string sql)
{
    if (mysql_query(conn_, sql.c_str())) {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "查询失败:" << mysql_error(conn_);
        return nullptr;
    }
    return mysql_use_result(conn_);
}

MYSQL* MySQL::getConnection()
{
    return conn_;
}