#pragma once

#include <string>
#include <SQLiteCpp/SQLiteCpp.h>

class SQLiteHelper {
public:
    SQLiteHelper(const std::string &dbname);
    ~SQLiteHelper();

    bool valid();
    bool execSql(const std::string &sql);
    std::unique_ptr<SQLite::Statement> querySql(const std::string &sql);
    void close();

private:
    std::string m_dbname;
    SQLite::Database m_dataBase;
};
