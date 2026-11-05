// BackendBridge - QML 与 C++ 后端（ServiceRun）的同进程桥接层
// 设计：
//   - BackendWorker 跑在独立 QThread，持有 ServiceRun，所有槽函数串行执行（FIFO，天然无并发问题）
//   - BackendBridge 住在主线程，暴露 Q_INVOKABLE 给 QML，内部 QueuedConnection 投递给 worker
//   - worker 完成工作后 emit 结果信号，跨线程自动排队回主线程，bridge 转发给 QML
// 字段命名与 electron-ui/src/api/tmb.ts 对齐（camelCase），保证两端协议一致
#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QThread>
#include <memory>

class ServiceRun;

// ---------------- Worker（后端线程） ----------------
class BackendWorker : public QObject
{
    Q_OBJECT
public:
    explicit BackendWorker(QObject* parent = nullptr);

public slots:
    void health();
    void refreshBackups();
    void loadBackupFiles(int backupId);
    void search(const QString& keyword);
    void loadConfig();
    void addSource(const QString& path);
    void removeSource(int id);
    void addTarget(const QString& path);
    void removeTarget(int id);
    void triggerBackup();
    void restore(qint64 historyId, const QString& destPath);

signals:
    void healthReady(bool ok, const QString& text);
    void backupsReady(const QVariantList& backups);
    void backupFilesReady(int backupId, const QVariantList& files);
    void searchReady(const QString& keyword, const QVariantList& results);
    void configReady(const QVariantList& sources, const QVariantList& targets);
    void opNotice(const QString& type, const QString& text);   // type: "ok" | "err"
    void restoreResult(qint64 historyId, bool ok, const QString& msg);

private:
    QVariantMap backupRow(int id, const char* begin, const char* end,
                          qint64 fileCnt, qint64 dataCnt);
    std::unique_ptr<ServiceRun> m_svc;   // 只在 worker 线程构造/使用/销毁
};

// ---------------- Bridge（主线程，暴露给 QML） ----------------
class BackendBridge : public QObject
{
    Q_OBJECT
public:
    explicit BackendBridge(QObject* parent = nullptr);
    ~BackendBridge() override;

    Q_INVOKABLE void health();
    Q_INVOKABLE void refreshBackups();
    Q_INVOKABLE void loadBackupFiles(int backupId);
    Q_INVOKABLE void search(const QString& keyword);
    Q_INVOKABLE void loadConfig();
    Q_INVOKABLE void addSource(const QString& path);
    Q_INVOKABLE void removeSource(int id);
    Q_INVOKABLE void addTarget(const QString& path);
    Q_INVOKABLE void removeTarget(int id);
    Q_INVOKABLE void triggerBackup();
    Q_INVOKABLE void restore(qint64 historyId, const QString& destPath);

signals:
    void healthReady(bool ok, const QString& text);
    void backupsReady(const QVariantList& backups);
    void backupFilesReady(int backupId, const QVariantList& files);
    void searchReady(const QString& keyword, const QVariantList& results);
    void configReady(const QVariantList& sources, const QVariantList& targets);
    void opNotice(const QString& type, const QString& text);
    void restoreResult(qint64 historyId, bool ok, const QString& msg);

private:
    QThread m_thread;
    BackendWorker* m_worker;   // 属于 m_thread
};
