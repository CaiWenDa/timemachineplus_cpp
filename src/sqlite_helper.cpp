#include "sqlite_helper.h"

#include <iostream>

SQLiteHelper::SQLiteHelper(const std::string& dbname)
    : m_dbname(dbname), m_dataBase(nullptr)
{
    try
    {
        // 打开数据库，若不存在则创建
        m_dataBase = std::make_unique<SQLite::Database>(
            m_dbname, SQLite::OPEN_READWRITE);
    }
    catch (const std::exception& e)
    {
        // 构造失败时，m_dataBase 保持为空；调用方可通过 valid() 检查
        std::cerr << "SQLiteHelper: failed to open DB: " << e.what() << " ["
                  << m_dbname << "]\n";
        m_dataBase.reset();
    }
}

SQLiteHelper::~SQLiteHelper() { close(); }

bool SQLiteHelper::valid() const noexcept
{
    return m_dataBase != nullptr && m_dataBase->getHandle() != nullptr;
}

bool SQLiteHelper::execSql(const std::string& sql)
{
    if (!valid())
    {
        return false;
    }

    try
    {
        // SQLite::Database::exec 会抛出异常失败，捕获并返回 false
        m_dataBase->exec(sql);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "SQLiteHelper::execSql error: " << e.what() << " SQL:["
                  << sql << "]\n";
        throw;
    }
}

std::unique_ptr<SQLite::Statement> SQLiteHelper::prepareQuery(
    const std::string& sql)
{
    if (!valid())
    {
        return nullptr;
    }

    try
    {
        return std::make_unique<SQLite::Statement>(*m_dataBase, sql);
    }
    catch (const std::exception& e)
    {
        std::cerr << "SQLiteHelper::prepareQuery error: " << e.what() << " SQL:["
                  << sql << "]\n";
        return nullptr;
    }
}

void SQLiteHelper::close() noexcept
{
    // 释放数据库对象，触发 SQLite 关闭
    if (m_dataBase)
    {
        try
        {
            m_dataBase.reset();
        }
        catch (...)
        {
            // 保守处理，确保 no-throw
        }
    }
}
