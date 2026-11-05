// BackendBridge.cpp - 实现见头文件注释
#include "BackendBridge.h"

#include "service_run.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <filesystem>
#include <cctype>

namespace
{

QString humanErr(const std::exception& e) { return QString::fromUtf8(e.what()); }

// 与 stdio_server.cpp 相同的工具：路径分隔符归一化
std::string normSlashes(std::string s)
{
    for (char& c : s) { if (c == '/') c = '\\'; }
    return s;
}

void stripLeadingSlashes(std::string& s)
{
    while (!s.empty() && (s.front() == '/' || s.front() == '\\'))
        s.erase(s.begin());
}

}  // namespace

// ==================== BackendWorker ====================

BackendWorker::BackendWorker(QObject* parent) : QObject(parent)
{
    // 在 worker 线程里通过 invokeMethod 延迟构造，保证 ServiceRun/SQLite 全程单线程
    QMetaObject::invokeMethod(this, [this]() {
        m_svc = std::make_unique<ServiceRun>();
        m_svc->init();
    }, Qt::QueuedConnection);
}

QVariantMap BackendWorker::backupRow(int id, const char* begin, const char* end,
                                     qint64 fileCnt, qint64 dataCnt)
{
    QVariantMap m;
    m["id"] = id;
    m["begintime"] = begin ? QString::fromUtf8(begin) : QVariant();
    m["endtime"] = end ? QString::fromUtf8(end) : QVariant();
    m["filecopycount"] = fileCnt;
    m["datacopycount"] = dataCnt;
    return m;
}

void BackendWorker::health()
{
    emit healthReady(true, QStringLiteral("ok"));
}

void BackendWorker::refreshBackups()
{
    QVariantList list;
    try
    {
        if (auto stmt = m_svc->sqliteHelper().prepareQuery(
                "select id,begintime,endtime,filecopycount,datacopycount "
                "from tb_backup order by id desc"))
        {
            while (stmt->executeStep())
            {
                auto col = [&](const char* name) { return stmt->getColumn(name); };
                list << backupRow(
                    col("id").getInt(),
                    col("begintime").isNull() ? nullptr : col("begintime").getText(),
                    col("endtime").isNull()   ? nullptr : col("endtime").getText(),
                    col("filecopycount").isNull() ? 0 : col("filecopycount").getInt64(),
                    col("datacopycount").isNull() ? 0 : col("datacopycount").getInt64());
            }
        }
    }
    catch (const std::exception& e)
    {
        emit opNotice(QStringLiteral("err"), humanErr(e));
    }
    emit backupsReady(list);
}

void BackendWorker::loadBackupFiles(int backupId)
{
    QVariantList list;
    try
    {
        if (auto stmt = m_svc->sqliteHelper().prepareQuery(
                "select f.filepath, h.id as historyId, h.filesize,"
                " h.motifytime, h.md5"
                " from tb_backfiles f"
                " join tb_backfilehistory h on h.backupfileid = f.id"
                " and h.id = (select max(h2.id) from tb_backfilehistory h2"
                "             where h2.backupfileid = f.id"
                "             and h2.backupid <= :bid)"
                " order by f.filepath"))
        {
            stmt->bind(":bid", backupId);
            while (stmt->executeStep())
            {
                QVariantMap m;
                m["historyId"]  = stmt->getColumn("historyId").getInt64();
                m["filepath"]   = QString::fromUtf8(stmt->getColumn("filepath").getText());
                m["filesize"]   = stmt->getColumn("filesize").getInt64();
                m["motifytime"] = stmt->getColumn("motifytime").getInt64();
                m["md5"]        = QString::fromUtf8(stmt->getColumn("md5").getText());
                list << m;
            }
        }
    }
    catch (const std::exception& e)
    {
        emit opNotice(QStringLiteral("err"), humanErr(e));
    }
    emit backupFilesReady(backupId, list);
}

void BackendWorker::search(const QString& keyword)
{
    QVariantList list;
    const std::string kw = keyword.toStdString();
    if (kw.empty()) { emit searchReady(keyword, list); return; }

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::string kwLower = lower(kw);
    const std::string like = "%" + normSlashes(kw) + "%";

    try
    {
        if (auto stmt = m_svc->sqliteHelper().prepareQuery(
                "select h.id as historyId, f.filepath, h.backupid, b.begintime,"
                " h.filesize, h.motifytime"
                " from tb_backfilehistory h"
                " join tb_backfiles f on h.backupfileid = f.id"
                " left join tb_backup b on h.backupid = b.id"
                " where replace(f.filepath,'/','\\') like :kw"
                " order by h.id desc"))
        {
            stmt->bind(":kw", like);
            while (stmt->executeStep())
            {
                const std::string filepath = stmt->getColumn("filepath").getText();
                const auto pos = filepath.find_last_of("/\\");
                const std::string basename =
                    pos == std::string::npos ? filepath : filepath.substr(pos + 1);
                // 与 stdio_server 相同：LIKE 粗筛后按文件名（不含目录）精确过滤
                if (lower(basename).find(kwLower) == std::string::npos) continue;

                QVariantMap m;
                m["historyId"] = stmt->getColumn("historyId").getInt64();
                m["filepath"]  = QString::fromStdString(filepath);
                m["backupId"]  = stmt->getColumn("backupid").getInt();
                m["backupBeginTime"] =
                    stmt->getColumn("begintime").isNull()
                        ? QVariant()
                        : QString::fromUtf8(stmt->getColumn("begintime").getText());
                m["filesize"]   = stmt->getColumn("filesize").getInt64();
                m["motifytime"] = stmt->getColumn("motifytime").getInt64();
                list << m;
            }
        }
    }
    catch (const std::exception& e)
    {
        emit opNotice(QStringLiteral("err"), humanErr(e));
    }
    emit searchReady(keyword, list);
}

void BackendWorker::loadConfig()
{
    QVariantList sources, targets;
    try
    {
        m_svc->loadBackupRoot();
        for (const auto& r : m_svc->backupRoots())
        {
            QVariantMap m;
            m["id"] = r.id;
            m["path"] = QString::fromStdString(r.rootpath);
            sources << m;
        }
        for (const auto& t : m_svc->backupTargets())
        {
            QVariantMap m;
            m["id"] = t.id;
            m["path"] = QString::fromStdString(t.targetrootpath);
            m["dir"]  = QString::fromStdString(t.targetrootdir);
            m["spaceRemain"] = static_cast<qint64>(t.spaceRemain);
            targets << m;
        }
    }
    catch (const std::exception& e)
    {
        emit opNotice(QStringLiteral("err"), humanErr(e));
    }
    emit configReady(sources, targets);
}

void BackendWorker::addSource(const QString& path)
{
    const std::string p = path.toStdString();
    if (!std::filesystem::exists(std::filesystem::u8path(p)))
    {
        emit opNotice(QStringLiteral("err"),
                      QStringLiteral("路径不存在：%1").arg(path));
        return;
    }
    if (!m_svc->addSourcePath(p))
    {
        emit opNotice(QStringLiteral("err"), QStringLiteral("添加源路径失败"));
        return;
    }
    emit opNotice(QStringLiteral("ok"),
                  QStringLiteral("✅ 源路径已保存，即时生效无需重启"));
    loadConfig();
}

void BackendWorker::removeSource(int id)
{
    bool ok = false;
    if (auto stmt = m_svc->sqliteHelper().prepareQuery(
            "select rootpath from tb_backuproot where id=:id"))
    {
        stmt->bind(":id", id);
        if (stmt->executeStep())
            ok = m_svc->removeSourcePath(stmt->getColumn("rootpath").getString());
    }
    emit opNotice(ok ? QStringLiteral("ok") : QStringLiteral("err"),
                  ok ? QStringLiteral("✅ 源路径已删除，即时生效")
                     : QStringLiteral("删除失败：源路径不存在"));
    loadConfig();
}

void BackendWorker::addTarget(const QString& path)
{
    const std::string p = path.toStdString();
    if (!std::filesystem::exists(std::filesystem::u8path(p)))
    {
        emit opNotice(QStringLiteral("err"),
                      QStringLiteral("路径不存在：%1").arg(path));
        return;
    }
    if (!m_svc->addTargetPath(p))
    {
        emit opNotice(QStringLiteral("err"), QStringLiteral("添加目标路径失败"));
        return;
    }
    emit opNotice(QStringLiteral("ok"),
                  QStringLiteral("✅ 目标路径已保存，即时生效无需重启"));
    loadConfig();
}

void BackendWorker::removeTarget(int id)
{
    bool ok = false;
    if (auto stmt = m_svc->sqliteHelper().prepareQuery(
            "select targetrootpath from tb_backuptargetroot where id=:id"))
    {
        stmt->bind(":id", id);
        if (stmt->executeStep())
            ok = m_svc->removeTargetPath(
                stmt->getColumn("targetrootpath").getString());
    }
    emit opNotice(ok ? QStringLiteral("ok") : QStringLiteral("err"),
                  ok ? QStringLiteral("✅ 目标路径已删除，即时生效")
                     : QStringLiteral("删除失败：目标路径不存在"));
    loadConfig();
}

void BackendWorker::triggerBackup()
{
    m_svc->loadBackupRoot();
    if (m_svc->backupRoots().empty())
    {
        emit opNotice(QStringLiteral("err"),
                      QStringLiteral("尚未配置备份源路径，请先在设置中添加"));
        return;
    }
    try
    {
        // 与 Electron 版行为一致：本槽在 worker 线程串行执行，
        // 阻塞期间后续请求排队等待（单线程 worker 的 FIFO 特性）
        m_svc->XCopy();
        emit opNotice(QStringLiteral("ok"), QStringLiteral("✅ 备份完成"));
    }
    catch (const std::exception& e)
    {
        emit opNotice(QStringLiteral("err"),
                      QStringLiteral("备份失败：%1").arg(humanErr(e)));
    }
    refreshBackups();
}

void BackendWorker::restore(qint64 historyId, const QString& destPath)
{
    try
    {
        m_svc->loadBackupRoot();

        std::string targetRoot, targetRelPath;
        bool found = false;
        if (auto stmt = m_svc->sqliteHelper().prepareQuery(
                "select h.backuptargetpath, h.backuptargetrootid"
                " from tb_backfilehistory h where h.id=:hid"))
        {
            stmt->bind(":hid", historyId);
            if (stmt->executeStep())
            {
                found = true;
                targetRoot =
                    m_svc->getTargetrootPath(stmt->getColumn("backuptargetrootid").getInt());
                targetRelPath = stmt->getColumn("backuptargetpath").getText();
            }
        }
        if (!found) throw std::runtime_error("history record not found");

        stripLeadingSlashes(targetRelPath);
        const std::string fullBackupPath =
            (std::filesystem::path(targetRoot) / targetRelPath).u8string();
        if (!std::filesystem::exists(fullBackupPath))
            throw std::runtime_error("备份文件已不存在于目标磁盘");

        std::filesystem::path dest(destPath.toStdString());
        if (dest.has_parent_path())
            std::filesystem::create_directories(dest.parent_path());
        std::filesystem::copy(fullBackupPath, dest,
                              std::filesystem::copy_options::overwrite_existing);

        emit restoreResult(historyId, true,
                           QStringLiteral("已恢复到 %1").arg(destPath));
    }
    catch (const std::exception& e)
    {
        emit restoreResult(historyId, false,
                           QStringLiteral("恢复失败：%1").arg(humanErr(e)));
    }
}

// ==================== BackendBridge ====================

BackendBridge::BackendBridge(QObject* parent) : QObject(parent)
{
    m_worker = new BackendWorker;
    m_worker->moveToThread(&m_thread);

    // worker 信号 → bridge 信号（跨线程自动 queued，QML 侧始终在 GUI 线程收到）
    connect(m_worker, &BackendWorker::healthReady,      this, &BackendBridge::healthReady);
    connect(m_worker, &BackendWorker::backupsReady,     this, &BackendBridge::backupsReady);
    connect(m_worker, &BackendWorker::backupFilesReady, this, &BackendBridge::backupFilesReady);
    connect(m_worker, &BackendWorker::searchReady,      this, &BackendBridge::searchReady);
    connect(m_worker, &BackendWorker::configReady,      this, &BackendBridge::configReady);
    connect(m_worker, &BackendWorker::opNotice,         this, &BackendBridge::opNotice);
    connect(m_worker, &BackendWorker::restoreResult,    this, &BackendBridge::restoreResult);

    m_thread.start();
}

BackendBridge::~BackendBridge()
{
    m_thread.quit();
    m_thread.wait(3000);
    delete m_worker;
}

// QML 调用 → 投递到 worker 线程串行执行
void BackendBridge::health()            { QMetaObject::invokeMethod(m_worker, "health", Qt::QueuedConnection); }
void BackendBridge::refreshBackups()    { QMetaObject::invokeMethod(m_worker, "refreshBackups", Qt::QueuedConnection); }
void BackendBridge::loadBackupFiles(int backupId)
{ QMetaObject::invokeMethod(m_worker, "loadBackupFiles", Qt::QueuedConnection, Q_ARG(int, backupId)); }
void BackendBridge::search(const QString& kw)
{ QMetaObject::invokeMethod(m_worker, "search", Qt::QueuedConnection, Q_ARG(QString, kw)); }
void BackendBridge::loadConfig()        { QMetaObject::invokeMethod(m_worker, "loadConfig", Qt::QueuedConnection); }
void BackendBridge::addSource(const QString& p)
{ QMetaObject::invokeMethod(m_worker, "addSource", Qt::QueuedConnection, Q_ARG(QString, p)); }
void BackendBridge::removeSource(int id)
{ QMetaObject::invokeMethod(m_worker, "removeSource", Qt::QueuedConnection, Q_ARG(int, id)); }
void BackendBridge::addTarget(const QString& p)
{ QMetaObject::invokeMethod(m_worker, "addTarget", Qt::QueuedConnection, Q_ARG(QString, p)); }
void BackendBridge::removeTarget(int id)
{ QMetaObject::invokeMethod(m_worker, "removeTarget", Qt::QueuedConnection, Q_ARG(int, id)); }
void BackendBridge::triggerBackup()     { QMetaObject::invokeMethod(m_worker, "triggerBackup", Qt::QueuedConnection); }
void BackendBridge::restore(qint64 id, const QString& dest)
{ QMetaObject::invokeMethod(m_worker, "restore", Qt::QueuedConnection, Q_ARG(qint64, id), Q_ARG(QString, dest)); }
