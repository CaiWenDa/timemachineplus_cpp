// stdio_server.cpp - stdin/stdout JSON 行协议服务
// 每行一个 JSON 对象：请求 {id, method, params} → 响应 {id, result|error}
// 日志输出走 stderr，避免污染 stdout
#include "stdio_server.h"

#include <json.hpp>

#include <atomic>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

using json = nlohmann::json;

namespace
{

// 全局互斥锁：串行化所有请求，与 runApiServer 同理
std::mutex g_stdioMutex;
std::atomic<bool> g_shutdown{false};

// 归一化路径分隔符
std::string normalizePathSlashes(std::string s)
{
    for (char& c : s) { if (c == '/') c = '\\'; }
    return s;
}

// 从路径串开头剥掉所有斜杠或反斜杠
void stripLeadingSlashes(std::string& s)
{
    while (!s.empty() && (s.front() == '/' || s.front() == '\\'))
        s.erase(s.begin());
}

// ==================== 核心请求处理 ====================

json handleStatus(ServiceRun& svc)
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
    return body;
}

json handleBackups(ServiceRun& svc)
{
    json arr = json::array();
    if (auto stmt = svc.sqliteHelper().prepareQuery(
            "select id,begintime,endtime,filecopycount,datacopycount "
            "from tb_backup order by id desc"))
    {
        while (stmt->executeStep())
        {
            json row = json::object();
            const int nCols = stmt->getColumnCount();
            for (int i = 0; i < nCols; ++i)
            {
                const std::string colName = stmt->getColumnName(i);
                const auto& col = stmt->getColumn(i);
                const int t = col.getType();
                if (t == SQLite::INTEGER)      row[colName] = col.getInt64();
                else if (t == SQLite::FLOAT)   row[colName] = col.getDouble();
                else if (t == SQLite::TEXT || t == SQLite::BLOB) row[colName] = col.getText();
                else                           row[colName] = nullptr;
            }
            arr.push_back(row);
        }
    }
    json body; body["backups"] = arr;
    return body;
}

json handleBackupFiles(ServiceRun& svc, const json& params)
{
    int backupId = params.value("backupId", 0);
    json arr = json::array();
    if (auto stmt = svc.sqliteHelper().prepareQuery(
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
            json row;
            row["historyId"] = stmt->getColumn("historyId").getInt64();
            row["filepath"]  = stmt->getColumn("filepath").getText();
            row["filesize"]  = stmt->getColumn("filesize").getInt64();
            row["motifytime"]= stmt->getColumn("motifytime").getInt64();
            row["md5"]       = stmt->getColumn("md5").getText();
            arr.push_back(row);
        }
    }
    json body; body["backupId"] = backupId; body["files"] = arr;
    return body;
}

json handleSearch(ServiceRun& svc, const json& params)
{
    const std::string kw = params.value("name", "");
    if (kw.empty()) { throw std::invalid_argument("missing 'name'"); }

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::string kwLower = lower(kw);
    const std::string like = "%" + normalizePathSlashes(kw) + "%";

    json arr = json::array();
    if (auto stmt = svc.sqliteHelper().prepareQuery(
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
            std::string filepath = stmt->getColumn("filepath").getText();
            const auto pos = filepath.find_last_of("/\\");
            const std::string basename =
                pos == std::string::npos ? filepath : filepath.substr(pos + 1);
            if (lower(basename).find(kwLower) == std::string::npos) continue;

            json row;
            row["historyId"]      = stmt->getColumn("historyId").getInt64();
            row["filepath"]       = filepath;
            row["backupId"]       = stmt->getColumn("backupid").getInt();
            row["backupBeginTime"]= stmt->getColumn("begintime").getText();
            row["filesize"]       = stmt->getColumn("filesize").getInt64();
            row["motifytime"]     = stmt->getColumn("motifytime").getInt64();
            arr.push_back(row);
        }
    }
    json body; body["keyword"] = kw; body["results"] = arr;
    return body;
}

json handleFileHistory(ServiceRun& svc, const json& params)
{
    svc.loadBackupRoot();
    const std::string rawPath = params.value("path", "");
    if (rawPath.empty()) { throw std::invalid_argument("missing 'path'"); }

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
    if (fileId == 0) { throw std::runtime_error("file not found in backup db"); }

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
            row["historyId"]      = stmt->getColumn("historyId").getInt64();
            row["motifytime"]     = stmt->getColumn("motifytime").getInt64();
            row["filesize"]       = stmt->getColumn("filesize").getInt64();
            row["md5"]            = stmt->getColumn("md5").getText();
            row["backuptargetpath"]= stmt->getColumn("backuptargetpath").getText();
            int rootId = stmt->getColumn("backuptargetrootid").getInt();
            row["backuptargetrootid"] = rootId;
            row["backupId"]       = stmt->getColumn("backupid").getInt();
            row["backupBeginTime"]= stmt->getColumn("begintime").getText();

            std::string rootPath = svc.getTargetrootPath(rootId);
            std::string relPath  = row["backuptargetpath"].get<std::string>();
            stripLeadingSlashes(relPath);
            std::string fullPath =
                (std::filesystem::path(rootPath) / relPath).u8string();
            row["fullBackupPath"]   = fullPath;
            row["backupFileExists"] = std::filesystem::exists(fullPath);

            arr.push_back(row);
        }
    }

    json body;
    body["filepath"] = rawPath;
    body["fileId"]   = fileId;
    body["history"]  = arr;
    return body;
}

json handleAddSource(ServiceRun& svc, const json& params)
{
    const std::string path = params.value("path", "");
    if (path.empty()) { throw std::invalid_argument("missing 'path'"); }
    if (!std::filesystem::exists(std::filesystem::u8path(path)))
        { throw std::invalid_argument("path does not exist: " + path); }
    if (!svc.addSourcePath(path)) { throw std::runtime_error("addSourcePath failed"); }
    return json::object();
}

json handleDeleteSource(ServiceRun& svc, const json& params)
{
    int id = params.value("id", 0);
    if (id <= 0) { throw std::invalid_argument("invalid id"); }

    if (auto stmt = svc.sqliteHelper().prepareQuery(
            "select rootpath from tb_backuproot where id=:id"))
    {
        stmt->bind(":id", id);
        if (stmt->executeStep())
        {
            if (svc.removeSourcePath(stmt->getColumn("rootpath").getString()))
                return json::object();
        }
    }
    throw std::runtime_error("source not found");
}

json handleAddTarget(ServiceRun& svc, const json& params)
{
    const std::string path = params.value("path", "");
    if (path.empty()) { throw std::invalid_argument("missing 'path'"); }
    if (!std::filesystem::exists(std::filesystem::u8path(path)))
        { throw std::invalid_argument("path does not exist: " + path); }
    if (!svc.addTargetPath(path)) { throw std::runtime_error("addTargetPath failed"); }
    return json::object();
}

json handleDeleteTarget(ServiceRun& svc, const json& params)
{
    int id = params.value("id", 0);
    if (id <= 0) { throw std::invalid_argument("invalid id"); }

    if (auto stmt = svc.sqliteHelper().prepareQuery(
            "select targetrootpath from tb_backuptargetroot where id=:id"))
    {
        stmt->bind(":id", id);
        if (stmt->executeStep())
        {
            if (svc.removeTargetPath(
                    stmt->getColumn("targetrootpath").getString()))
                return json::object();
        }
    }
    throw std::runtime_error("target not found");
}

json handleTriggerBackup(ServiceRun& svc)
{
    svc.loadBackupRoot();
    if (svc.backupRoots().empty())
        { throw std::runtime_error("no source paths configured"); }
    std::thread([&svc]() {
        std::lock_guard<std::mutex> lk(g_stdioMutex);
        try { svc.XCopy(); }
        catch (const std::exception& e)
        {
            std::cerr << "[backup thread] error: " << e.what() << std::endl;
        }
    }).detach();
    json body; body["status"] = "started";
    return body;
}

json handleRestore(ServiceRun& svc, const json& params)
{
    svc.loadBackupRoot();
    const int64_t historyId = params.value("historyId", 0LL);
    const std::string destPath = params.value("destPath", "");
    if (historyId <= 0)     { throw std::invalid_argument("missing 'historyId'"); }
    if (destPath.empty())   { throw std::invalid_argument("missing 'destPath'"); }

    std::string targetRoot, targetRelPath;
    if (auto stmt = svc.sqliteHelper().prepareQuery(
            "select h.backuptargetpath, h.backuptargetrootid"
            " from tb_backfilehistory h where h.id=:hid"))
    {
        stmt->bind(":hid", historyId);
        if (!stmt->executeStep())
            { throw std::runtime_error("history record not found"); }
        int rootId = stmt->getColumn("backuptargetrootid").getInt();
        targetRoot = svc.getTargetrootPath(rootId);
        targetRelPath = stmt->getColumn("backuptargetpath").getText();
    }

    stripLeadingSlashes(targetRelPath);
    const std::string fullBackupPath =
        (std::filesystem::path(targetRoot) / targetRelPath).u8string();

    if (!std::filesystem::exists(fullBackupPath))
        { throw std::runtime_error("backup file missing on disk"); }

    std::filesystem::path dest(destPath);
    if (dest.has_parent_path())
        std::filesystem::create_directories(dest.parent_path());
    std::filesystem::copy(fullBackupPath, dest,
                          std::filesystem::copy_options::overwrite_existing);

    json resp; resp["ok"] = true; resp["restoredTo"] = destPath;
    return resp;
}

// ==================== 方法分发 ====================

json dispatch(ServiceRun& svc, const std::string& method, const json& params)
{
    if (method == "health")          { json r; r["status"] = "ok"; return r; }
    if (method == "status")          return handleStatus(svc);
    if (method == "backups")         return handleBackups(svc);
    if (method == "backup_files")    return handleBackupFiles(svc, params);
    if (method == "search")          return handleSearch(svc, params);
    if (method == "file_history")    return handleFileHistory(svc, params);
    if (method == "add_source")      return handleAddSource(svc, params);
    if (method == "delete_source")   return handleDeleteSource(svc, params);
    if (method == "add_target")      return handleAddTarget(svc, params);
    if (method == "delete_target")   return handleDeleteTarget(svc, params);
    if (method == "trigger_backup")  return handleTriggerBackup(svc);
    if (method == "restore")         return handleRestore(svc, params);
    throw std::invalid_argument("unknown method: " + method);
}

void writeLine(const std::string& s)
{
    std::cout << s << '\n';
    std::cout.flush();
}

}  // namespace

int runStdioServer(ServiceRun& serviceRun)
{
    std::cerr << "[stdio] backend ready, waiting for commands..." << std::endl;

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (g_shutdown.load()) break;
        if (line.empty()) continue;

        json request;
        try { request = json::parse(line); }
        catch (const std::exception& e)
        {
            json err;
            err["id"]    = nullptr;
            err["error"] = {{"code", 400}, {"message", std::string("invalid JSON: ") + e.what()}};
            writeLine(err.dump());
            continue;
        }

        // shutdown 指令：不持锁
        if (request.value("method", std::string{}) == "shutdown")
        {
            json resp;
            resp["id"]     = request.contains("id") ? request["id"] : json();
            resp["result"] = {{"ok", true}};
            writeLine(resp.dump());
            std::cerr << "[stdio] shutdown requested" << std::endl;
            break;
        }

        json response;
        response["id"] = request.contains("id") ? request["id"] : json();

        std::lock_guard<std::mutex> lk(g_stdioMutex);
        try
        {
            const std::string method = request.value("method", std::string{});
            const json& params = request.contains("params") ? request["params"] : json::object();
            json result = dispatch(serviceRun, method, params);
            response["result"] = result;
        }
        catch (const std::invalid_argument& e)
        {
            response["error"] = {{"code", 400}, {"message", e.what()}};
        }
        catch (const std::runtime_error& e)
        {
            response["error"] = {{"code", 404}, {"message", e.what()}};
        }
        catch (const std::exception& e)
        {
            response["error"] = {{"code", 500}, {"message", e.what()}};
        }
        catch (...)
        {
            response["error"] = {{"code", 500}, {"message", "unknown error"}};
        }

        writeLine(response.dump());
    }

    std::cerr << "[stdio] bye" << std::endl;
    return 0;
}
