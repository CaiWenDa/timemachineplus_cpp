// api_server.cpp - REST API server（方案 B 后端层）
// 将 ServiceRun + SQLite 查询能力暴露为 HTTP REST 端点
#include "api_server.h"

#include <httplib.h>
#include <json.hpp>

#include <filesystem>
#include <iostream>

using json = nlohmann::json;

namespace
{

// ==================== 辅助函数 ====================

// SQLite::Row → json 对象
json rowToJson(SQLite::Statement& stmt)
{
    json obj = json::object();
    const int nCols = stmt.getColumnCount();
    for (int i = 0; i < nCols; ++i)
    {
        const std::string colName = stmt.getColumnName(i);
        const auto& col = stmt.getColumn(i);
        const int t = col.getType();
        if (t == SQLite::INTEGER) {
            obj[colName] = col.getInt64();
        } else if (t == SQLite::FLOAT) {
            obj[colName] = col.getDouble();
        } else if (t == SQLite::TEXT || t == SQLite::BLOB) {
            obj[colName] = col.getText();
        } else {
            obj[colName] = nullptr;
        }
    }
    return obj;
}

void error(httplib::Response& res, int code, const std::string& msg)
{
    json body;
    body["error"] = msg;
    res.status = code;
    res.set_content(body.dump(), "application/json");
}

void ok(httplib::Response& res)
{
    res.status = 200;
    res.set_content("{}", "application/json");
}

void ok(httplib::Response& res, const json& body)
{
    res.status = 200;
    res.set_content(body.dump(), "application/json");
}

void noContent(httplib::Response& res)
{
    res.status = 204;
}

// ==================== Handler 集合 ====================

void handleStatus(httplib::Response& res, ServiceRun& svc)
{
    svc.loadBackupRoot();
    json body = json::object();
    json srcArr = json::array();
    for (const auto& root : svc.backupRoots())
    {
        json row;
        row["id"] = root.id;
        row["path"] = root.rootpath;
        srcArr.push_back(row);
    }
    body["sources"] = srcArr;

    json tgtArr = json::array();
    for (const auto& target : svc.backupTargets())
    {
        json row;
        row["id"] = target.id;
        row["path"] = target.targetrootpath;
        row["dir"] = target.targetrootdir;
        row["spaceRemain"] = target.spaceRemain;
        tgtArr.push_back(row);
    }
    body["targets"] = tgtArr;
    ok(res, body);
}

void handleBackups(httplib::Response& res, ServiceRun& svc)
{
    json arr = json::array();
    if (auto stmt = svc.sqliteHelper().prepareQuery(
            "select id,begintime,endtime,filecopycount,datacopycount "
            "from tb_backup order by id desc"))
    {
        while (stmt->executeStep())
        {
            arr.push_back(rowToJson(*stmt));
        }
    }
    json body;
    body["backups"] = arr;
    ok(res, body);
}

void handlePostBackup(httplib::Response& res, ServiceRun& svc)
{
    svc.loadBackupRoot();
    if (svc.backupRoots().empty())
    {
        error(res, 400, "no source paths configured");
        return;
    }
    std::thread([&svc]() {
        try { svc.XCopy(); }
        catch (const std::exception& e)
        {
            std::cerr << "backup thread error: " << e.what() << std::endl;
        }
    }).detach();

    json body;
    body["status"] = "started";
    ok(res, body);
}

void handleSources(httplib::Response& res, ServiceRun& svc)
{
    svc.loadBackupRoot();
    json arr = json::array();
    for (const auto& root : svc.backupRoots())
    {
        json row;
        row["id"] = root.id;
        row["path"] = root.rootpath;
        arr.push_back(row);
    }
    json body;
    body["sources"] = arr;
    ok(res, body);
}

void handlePostSources(const httplib::Request& req, httplib::Response& res,
                       ServiceRun& svc)
{
    try
    {
        auto body = json::parse(req.body);
        if (!body.contains("path") || !body["path"].is_string())
        {
            error(res, 400, "missing 'path'");
            return;
        }
        const std::string path = body["path"].get<std::string>();
        if (svc.addSourcePath(path)) { ok(res); }
        else { error(res, 500, "addSourcePath failed"); }
    }
    catch (const std::exception& e) { error(res, 400, e.what()); }
}

void handleDeleteSource(const httplib::Request& req, httplib::Response& res,
                        ServiceRun& svc)
{
    // 手动解析 /api/sources/NNN 里的 id
    const auto pos = req.path.find_last_of('/');
    if (pos != std::string::npos)
    {
        std::string idStr = req.path.substr(pos + 1);
        int id = 0;
        try { id = std::stoi(idStr); }
        catch (...) { error(res, 400, "invalid id"); return; }

        if (auto stmt = svc.sqliteHelper().prepareQuery(
                "select rootpath from tb_backuproot where id=:id"))
        {
            stmt->bind(":id", id);
            if (stmt->executeStep())
            {
                if (svc.removeSourcePath(stmt->getColumn("rootpath").getString()))
                {
                    noContent(res);
                    return;
                }
            }
        }
    }
    error(res, 404, "source not found");
}

void handleTargets(httplib::Response& res, ServiceRun& svc)
{
    svc.loadBackupRoot();
    json arr = json::array();
    for (const auto& target : svc.backupTargets())
    {
        json row;
        row["id"] = target.id;
        row["path"] = target.targetrootpath;
        row["dir"] = target.targetrootdir;
        row["spaceRemain"] = target.spaceRemain;
        arr.push_back(row);
    }
    json body;
    body["targets"] = arr;
    ok(res, body);
}

void handlePostTargets(const httplib::Request& req, httplib::Response& res,
                       ServiceRun& svc)
{
    try
    {
        auto body = json::parse(req.body);
        if (!body.contains("path")) { error(res, 400, "missing 'path'"); return; }
        if (svc.addTargetPath(body["path"].get<std::string>())) { ok(res); }
        else { error(res, 500, "addTargetPath failed"); }
    }
    catch (const std::exception& e) { error(res, 400, e.what()); }
}

// 归一化路径：把所有 / 和 \ 都统一成 \，便于跨平台 DB 路径比较
std::string normalizePathSlashes(std::string s)
{
    for (char& c : s)
    {
        if (c == '/') c = '\\';
    }
    return s;
}

void handleFileHistory(const httplib::Request& req, httplib::Response& res,
                       ServiceRun& svc)
{
    svc.loadBackupRoot();  // 确保 m_backupTargetRootList 缓存是最新的
    const std::string rawPath = req.get_param_value("path");
    if (rawPath.empty()) { error(res, 400, "missing 'path' query param"); return; }

    // DB 里可能存的是 / 或 \，统一归一化后再查
    const std::string normPath = normalizePathSlashes(rawPath);

    int64_t fileId = 0;
    if (auto sel = svc.sqliteHelper().prepareQuery(
            "select id from tb_backfiles "
            "where replace(filepath,'/','\\')=:p "
            "order by id desc limit 1"))
    {
        sel->bind(":p", normPath);
        if (sel->executeStep()) { fileId = sel->getColumn("id").getInt64(); }
    }
    if (fileId == 0) { error(res, 404, "file not found in backup db"); return; }

    json arr = json::array();
    if (auto stmt = svc.sqliteHelper().prepareQuery(
            "select h.id as historyId, h.motifytime, h.filesize, h.md5,"
            " h.backuptargetpath, h.backuptargetrootid,"
            " h.backupid, b.begintime from tb_backfilehistory h"
            " left join tb_backup b on h.backupid = b.id"
            " where h.backupfileid=:fid"
            " order by h.id desc"))
    {
        stmt->bind(":fid", fileId);
        while (stmt->executeStep())
        {
            json row;
            row["historyId"] = stmt->getColumn("historyId").getInt64();
            row["motifytime"] = stmt->getColumn("motifytime").getInt64();
            row["filesize"] = stmt->getColumn("filesize").getInt64();
            row["md5"] = stmt->getColumn("md5").getText();
            row["backuptargetpath"] = stmt->getColumn("backuptargetpath").getText();
            int rootId = stmt->getColumn("backuptargetrootid").getInt();
            row["backuptargetrootid"] = rootId;
            row["backupid"] = stmt->getColumn("backupid").getInt();
            row["backupBeginTime"] = stmt->getColumn("begintime").getText();

            std::string rootPath = svc.getTargetrootPath(rootId);
            std::string relPath = row["backuptargetpath"].get<std::string>();
            // DB 里 relPath 可能以 / 或 \ 开头（类 Unix 根路径），
            // 在 Windows std::filesystem 里这会被当作驱动器根覆盖 rootPath，
            // 必须先剥掉开头的分隔符再拼接。
            while (!relPath.empty() &&
                   (relPath.front() == '/' || relPath.front() == '\\'))
            {
                relPath.erase(relPath.begin());
            }
            std::string fullPath =
                (std::filesystem::path(rootPath) / relPath).u8string();
            row["fullBackupPath"] = fullPath;
            row["backupFileExists"] = std::filesystem::exists(fullPath);

            arr.push_back(row);
        }
    }

    json body;
    body["filepath"] = rawPath;
    body["fileId"] = fileId;
    body["history"] = arr;
    ok(res, body);
}

void handleRestore(const httplib::Request& req, httplib::Response& res,
                   ServiceRun& svc)
{
    svc.loadBackupRoot();  // 确保 target root 缓存是最新的
    try
    {
        auto body = json::parse(req.body);
        if (!body.contains("historyId") || !body.contains("destPath"))
        {
            error(res, 400, "missing 'historyId' or 'destPath'");
            return;
        }
        const int64_t historyId = body["historyId"].get<int64_t>();
        const std::string destPath = body["destPath"].get<std::string>();

        std::string targetRoot, targetRelPath;
        if (auto stmt = svc.sqliteHelper().prepareQuery(
                "select h.backuptargetpath, h.backuptargetrootid"
                " from tb_backfilehistory h where h.id=:hid"))
        {
            stmt->bind(":hid", historyId);
            if (!stmt->executeStep()) { error(res, 404, "history record not found"); return; }
            int rootId = stmt->getColumn("backuptargetrootid").getInt();
            targetRoot = svc.getTargetrootPath(rootId);
            targetRelPath = stmt->getColumn("backuptargetpath").getText();
        }

        // 剥掉开头的 / 或 \，避免 Windows std::filesystem 把它当根路径
        while (!targetRelPath.empty() &&
               (targetRelPath.front() == '/' || targetRelPath.front() == '\\'))
        {
            targetRelPath.erase(targetRelPath.begin());
        }
        const std::string fullBackupPath =
            (std::filesystem::path(targetRoot) / targetRelPath).u8string();

        if (!std::filesystem::exists(fullBackupPath))
        {
            error(res, 410, "backup file missing on disk");
            return;
        }

        std::filesystem::path dest(destPath);
        if (dest.has_parent_path())
        {
            std::filesystem::create_directories(dest.parent_path());
        }
        std::filesystem::copy(fullBackupPath, dest,
                              std::filesystem::copy_options::overwrite_existing);

        json resp;
        resp["ok"] = true;
        resp["restoredTo"] = destPath;
        ok(res, resp);
    }
    catch (const std::exception& e) { error(res, 500, e.what()); }
}

}  // namespace

int runApiServer(ServiceRun& serviceRun, int port)
{
    httplib::Server svr;

    svr.set_pre_routing_handler([](const httplib::Request& req,
                                   httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS")
        {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        json j; j["status"] = "ok"; ok(res, j);
    });

    svr.Get("/api/status", [&](const httplib::Request&, httplib::Response& res) {
        handleStatus(res, serviceRun);
    });

    svr.Get("/api/backups", [&](const httplib::Request&, httplib::Response& res) {
        handleBackups(res, serviceRun);
    });
    svr.Post("/api/backup", [&](const httplib::Request&, httplib::Response& res) {
        handlePostBackup(res, serviceRun);
    });

    svr.Get("/api/sources", [&](const httplib::Request&, httplib::Response& res) {
        handleSources(res, serviceRun);
    });
    svr.Post("/api/sources", [&](const httplib::Request& req,
                                  httplib::Response& res) {
        handlePostSources(req, res, serviceRun);
    });
    svr.Delete("/api/sources/.*", [&](const httplib::Request& req,
                                       httplib::Response& res) {
        handleDeleteSource(req, res, serviceRun);
    });

    svr.Get("/api/targets", [&](const httplib::Request&, httplib::Response& res) {
        handleTargets(res, serviceRun);
    });
    svr.Post("/api/targets", [&](const httplib::Request& req,
                                  httplib::Response& res) {
        handlePostTargets(req, res, serviceRun);
    });

    svr.Get("/api/files", [&](const httplib::Request& req,
                               httplib::Response& res) {
        handleFileHistory(req, res, serviceRun);
    });

    svr.Post("/api/restore", [&](const httplib::Request& req,
                                  httplib::Response& res) {
        handleRestore(req, res, serviceRun);
    });

    svr.set_mount_point("/", "./electron-ui/dist");

    std::cout << "[API] timemachineplus HTTP server starting on port " << port
              << " ..." << std::endl;
    std::cout << "[API] UI (if built) at http://localhost:" << port << "/" << std::endl;

    return svr.listen("127.0.0.1", port) ? 0 : 1;
}
