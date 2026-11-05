// main.cpp - Time Machine+ QML UI 入口
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFont>
#include <QDir>
#include <QFileInfo>

#include "BackendBridge.h"

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
    app.setApplicationName("Time Machine+");
    app.setOrganizationName("timemachineplus");

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

    // 定位项目根目录（含 timemachine.sql 的目录），设置 DB 路径环境变量
    // 查找顺序：exe 目录向上 5 层 → 当前工作目录
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 5; ++i)
    {
        if (QFileInfo::exists(dir.filePath("timemachine.sql")))
            break;
        if (!dir.cdUp()) break;
    }
    if (QFileInfo::exists(dir.filePath("timemachine.sql")))
    {
        qputenv("TIMEMACHINE_DB", QFile::encodeName(dir.filePath("timemachine.db")));
        qputenv("TIMEMACHINE_SCHEMA", QFile::encodeName(dir.filePath("timemachine.sql")));
        QDir::setCurrent(dir.absolutePath());
    }

    BackendBridge backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
