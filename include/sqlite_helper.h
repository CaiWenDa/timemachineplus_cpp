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

    // 直接执行 SQL（支持多语句，适合执行 .sql 脚本文件内容）
    bool execSql(const std::string& sql);

    // 读取并执行一个 .sql 脚本文件
    bool execScriptFile(const std::string& path);

    // 返回准备好的语句（注意：Statement 持有对 Database 的引用，确保 Database 未被 close）
    std::unique_ptr<SQLite::Statement> prepareQuery(const std::string& sql);

    // 关闭并释放数据库资源
    void close() noexcept;

    // 暴露底层 Database（供 Transaction 等高级用法）
    SQLite::Database* database() noexcept { return m_dataBase.get(); }

private:
    std::string m_dbname;
    std::unique_ptr<SQLite::Database> m_dataBase;
};
