#pragma once

#include <memory>
#include <string>
#include <optional>
#include <SQLiteCpp/SQLiteCpp.h>

class SQLiteHelper
{
public:
    explicit SQLiteHelper(const std::string& dbname);
    ~SQLiteHelper();

    // 检查数据库是否已成功打开
    bool valid() const noexcept;

    // 直接执行 SQL（建议仅用于不带参数的简单 SQL）
    bool execSql(const std::string& sql);

    // 返回准备好的语句（注意：Statement 持有对 Database 的引用，确保 Database 未被 close）
    std::unique_ptr<SQLite::Statement> prepareQuery(const std::string& sql);

    // 关闭并释放数据库资源
    void close() noexcept;

private:
    std::string m_dbname;
    std::unique_ptr<SQLite::Database> m_dataBase;
};
