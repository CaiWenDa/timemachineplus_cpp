#include "sqlite_helper.h"

SQLiteHelper::SQLiteHelper(const std::string & dbname)
    : m_dbname(dbname), m_dataBase(dbname, SQLite::OPEN_READWRITE)
{

}

SQLiteHelper::~SQLiteHelper()
{
    close();
}

bool SQLiteHelper::valid()
{
    return m_dataBase.getHandle() != nullptr;
}

bool SQLiteHelper::execSql(const std::string &sql)
{
    if (!valid())
    {
        return false;
    }
    else
    {
        SQLite::Statement query(m_dataBase, sql);
        return query.exec();
    }

    return true;
}

std::unique_ptr<SQLite::Statement> SQLiteHelper::querySql(const std::string &sql)
{
    if (!valid())
    {
        return nullptr;
    }
    else
    {
        return std::make_unique<SQLite::Statement>(m_dataBase, sql);
    }
}

void SQLiteHelper::close()
{
    ;
}
