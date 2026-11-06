// main.cpp - Time Machine+ QML UI 入口
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFont>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QQuickWindow>
#include <QStandardPaths>

#include "BackendBridge.h"
#include "config.h"

int main(int argc, char* argv[])
{
    // 高 DPI：Qt 5.15 需要显式开启（Qt6 默认开启）
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    // 关键：Qt5 默认 RoundPreferFloor 会把 125%/150% 的缩放因子向下取整为 1.0，
    // 导致整个界面比系统其他窗口小一截。PassThrough = 使用精确的分数缩放（1.25），
    // 场景图按真实 DPR 渲染，文字经距离场渲染保持清晰。
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    app.setApplicationName("TimeMachinePlus");     // 用于数据/缓存目录命名
    app.setApplicationDisplayName("Time Machine+"); // 用于窗口/UI 显示
    app.setOrganizationName("timemachineplus");

    // 窗口/任务栏图标：用 :/ 前缀（Qt C++ 资源系统规范路径，比 qrc: 更可靠）
    QIcon appIcon(":/qml/app_icon.png");
    if (appIcon.isNull())
        qWarning("[main] app_icon.png 加载失败，请检查 qrc 资源路径");
    else
        app.setWindowIcon(appIcon);

    // 全局默认字体栈：与 Theme.fontUi 保持一致，覆盖未显式设置 font.family 的
    // 控件（QtQuick.Controls 内部文本等），防止中文回退到宋体。
    {
        QFont appFont;
        appFont.setFamilies({QStringLiteral("Segoe UI"),
                             QStringLiteral("Microsoft YaHei UI"),
                             QStringLiteral("Microsoft YaHei"),
                             QStringLiteral("PingFang SC"),
                             QStringLiteral("Noto Sans CJK SC")});
        appFont.setPixelSize(13);
        QGuiApplication::setFont(appFont);
    }

    // ---- 运行期路径策略（区分安装态与开发态） ----
    // 安装态：exe 同目录存在 timemachine.sql（安装包随包分发 schema）
    // 开发态：build 目录内，需向上最多 5 层查找项目根
    // 数据文件（timemachine.db / log.log）必须落在可写目录：
    //   安装态 → %LOCALAPPDATA%\TimeMachinePlus（装到 Program Files 时不可写）
    //   开发态 → 项目根（沿用既有数据库，避免历史备份数据"消失"）
    QDir exeDir(QCoreApplication::applicationDirPath());
    QDir probe = exeDir;
    QString schemaPath;
    bool installedLayout = false;

    if (QFileInfo::exists(probe.filePath(timemachineplus::config::kSchemaFile)))
    {
        schemaPath = probe.filePath(timemachineplus::config::kSchemaFile);
        installedLayout = true;
    }
    else
    {
        for (int i = 0; i < 5; ++i)
        {
            if (!probe.cdUp()) break;
            if (QFileInfo::exists(probe.filePath(timemachineplus::config::kSchemaFile)))
            {
                schemaPath = probe.filePath(timemachineplus::config::kSchemaFile);
                break;
            }
        }
    }

    QString dataDir;
    if (installedLayout)
    {
        const QString base =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        // 目录名避开 Qt 的 CacheLocation（%LOCALAPPDATA%\timemachineplus\TimeMachinePlus，
        // Windows 不区分大小写，与 "TimeMachinePlus" 同名会导致 QML 缓存嵌进数据目录）
        dataDir = QDir(base).filePath(QStringLiteral("TimeMachinePlusData"));
        if (!QDir().mkpath(dataDir) && !QDir(dataDir).exists())
            dataDir = exeDir.absolutePath();  // 只读环境兜底
    }
    else if (!schemaPath.isEmpty())
    {
        dataDir = QFileInfo(schemaPath).absolutePath();
    }
    else
    {
        dataDir = exeDir.absolutePath();
    }

    // 工作目录固定在 exe 同目录（日志等运行期文件落在这里），
    // 数据库已通过 TIMEMACHINE_DB 指向绝对路径，无需依赖 CWD。
    // 不与数据目录共用，避免 Qt 运行期文件混入用户备份数据库目录。
    QDir::setCurrent(exeDir.absolutePath());
    qputenv("TIMEMACHINE_DB",
            QFile::encodeName(QDir(dataDir).filePath(
                QString::fromUtf8(timemachineplus::config::kDbName))));
    if (!schemaPath.isEmpty())
        qputenv("TIMEMACHINE_SCHEMA", QFile::encodeName(schemaPath));

    BackendBridge backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    // 直接给根窗口设置图标：Qt Quick 中 setWindowIcon 只设默认值，
    // 任务栏/标题栏图标需要显式设到 QQuickWindow 上才稳定生效。
    if (!appIcon.isNull())
    {
        int winCount = 0;
        for (QObject* obj : engine.rootObjects())
        {
            QQuickWindow* win = qobject_cast<QQuickWindow*>(obj);
            if (win)
            {
                win->setIcon(appIcon);
                ++winCount;
            }
        }
        qInfo("[main] 图标已设置到 %d 个窗口", winCount);
    }

    return app.exec();
}
