#include "service_run.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "util.h"

namespace
{
// 统一路径转换（减少重复 std::filesystem::u8path(...) 写法）
inline std::filesystem::path u8path_from(const std::string& s)
{
    return std::filesystem::u8path(s);
}
}  // namespace

void ServiceRun::init()
{
    logger.info("init db...");
    if (!m_sqliteHelper.valid())
    {
        logger.error("SQLite init failed");
        return;
    }

    // 执行 schema 初始化脚本（幂等，可重复执行）
    if (!m_sqliteHelper.execScriptFile("timemachine.sql"))
    {
        logger.error("failed to init schema from timemachine.sql");
    }
}

void ServiceRun::loadBackupRoot()
{
    logger.info("loadBackupRoot");
    if (auto res = m_sqliteHelper.prepareQuery("select * from tb_backuproot"); res)
    {
        while (res->executeStep())
        {
            timemachine::Backuproot backuproot;
            backuproot.id = res->getColumn("id").getInt();
            backuproot.rootpath = res->getColumn("rootpath").getString();
            if (!std::filesystem::exists(u8path_from(backuproot.rootpath)))
            {
                logger.error("No found backup source: " + backuproot.rootpath);
            }
            else
            {
                m_backupRootList.emplace_back(std::move(backuproot));
            }
        }
    }

    if (auto res = m_sqliteHelper.prepareQuery("select * from tb_backuptargetroot"); res)
    {
        while (res->executeStep())
        {
            timemachine::Backuptargetroot backuptargetroot;
            backuptargetroot.id = res->getColumn("id").getInt();
            backuptargetroot.targetrootpath =
                res->getColumn("targetrootpath").getString();
            backuptargetroot.targetrootdir = targetBkDirName;

            auto u8path = u8path_from(backuptargetroot.targetrootpath);
            if (!std::filesystem::exists(u8path))
            {
                std::filesystem::create_directory(u8path);
            }
            if (!std::filesystem::exists(u8path))
            {
                logger.error("No found backup target: " +
                             backuptargetroot.targetrootpath);
            }
            else
            {
                try
                {
                    backuptargetroot.spaceRemain =
                        std::filesystem::space(u8path).available;
                }
                catch (const std::exception& e)
                {
                    logger.error(std::string("space() failed for: ") +
                                 backuptargetroot.targetrootpath + " -> " + e.what());
                    backuptargetroot.spaceRemain = 0;
                }
                m_backupTargetRootList.emplace_back(std::move(backuptargetroot));
            }
        }
    }
}

void ServiceRun::loadAllFiles(const std::string& pathName,
                              std::vector<std::string>& fileList)
{
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(u8path_from(pathName)))
    {
        if (!entry.is_directory())
        {
            const auto u8pathStr = entry.path().u8string();
            if (u8pathStr.find("/.") == std::string::npos &&
                u8pathStr.find("\\.") == std::string::npos)
            {
                fileList.emplace_back(u8pathStr);
            }
        }
    }
}

std::optional<timemachine::Backuptargetroot> ServiceRun::getAvailableTarget(
    uintmax_t needspace)
{
    std::vector<timemachine::Backuptargetroot> available;
    available.reserve(m_backupTargetRootList.size());
    for (auto& br : m_backupTargetRootList)
    {
        try
        {
            br.spaceRemain =
                std::filesystem::space(u8path_from(br.targetrootpath)).available;
        }
        catch (const std::exception& e)
        {
            logger.error(std::string("failed to query space for ") + br.targetrootpath +
                         " -> " + e.what());
            br.spaceRemain = 0;
        }

        if (br.spaceRemain > needspace)
        {
            available.emplace_back(br);
        }
    }

    if (!available.empty())
    {
        return *std::max_element(available.begin(), available.end(),
                                 [](const auto& a, const auto& b)
                                 { return a.spaceRemain < b.spaceRemain; });
    }
    return std::nullopt;
}

void ServiceRun::copyFile(const std::string& source, const std::string& dest)
{
    try
    {
        const auto destPath = u8path_from(dest);
        const auto destDir = destPath.parent_path();
        if (!std::filesystem::exists(destDir))
        {
            std::filesystem::create_directories(destDir);
        }
        std::filesystem::copy_file(u8path_from(source), destPath,
                                   std::filesystem::copy_options::overwrite_existing);
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
        throw;  // 保留原始异常信息，避免切片
    }
}

bool ServiceRun::exeCopy(const std::string& fileName, int64_t backupfileid)
{
    const auto filePath = u8path_from(fileName);
    const auto fileSize = std::filesystem::file_size(filePath);
    const auto lastWriteTime = Utils::getSysFileMilliTimeStamp(filePath);

    const auto backuptargetroot = getAvailableTarget(fileSize);
    if (!backuptargetroot)
    {
        logger.error("no space in all targetbackups! need:" + std::to_string(fileSize));
        return false;
    }

    std::string md5str;
    try
    {
        md5str = Utils::getFileMD5(fileName);
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
        // md5 失败仍可继续（会产生无 md5 的记录）
    }

    // 使用 filesystem::path 构造目标路径更稳健
    const auto targetPath =
        u8path_from(backuptargetroot->targetrootpath) / backuptargetroot->targetrootdir;
    const std::string name = md5str + "_" + std::to_string(Utils::getMilliTimeStamp());
    const auto targetFull = (targetPath / name).u8string();
    // 存入 DB 的 backuptargetpath 存相对 target root 的路径
    const auto targetSave =
        (std::filesystem::path("/") / backuptargetroot->targetrootdir / name).u8string();

    const auto begincopysingle = Utils::Date::getCurrentDateTime();
    try
    {
        copyFile(fileName, targetFull);
    }
    catch (const std::exception&)
    {
        logger.error("failed to copy file from " + fileName + " to " + targetFull);
        return false;
    }

    if (auto stmt = m_sqliteHelper.prepareQuery(
            "insert into tb_backfilehistory "
            "(backupfileid,backupid,motifytime,filesize,copystarttime,copyendtime,"
            "backuptargetpath,backuptargetrootid,md5) "
            "values(:fileid,:bid,:mt,:fs,:start,:end,:tpath,:trootid,:md5)");
        stmt)
    {
        stmt->bind(":fileid", backupfileid);
        stmt->bind(":bid", m_backupId);
        stmt->bind(":mt", lastWriteTime);
        stmt->bind(":fs", static_cast<int64_t>(fileSize));
        stmt->bind(":start", begincopysingle);
        stmt->bind(":end", Utils::Date::getCurrentDateTime());
        stmt->bind(":tpath", targetSave);
        stmt->bind(":trootid", backuptargetroot->id);
        stmt->bind(":md5", md5str);
        stmt->exec();
    }

    logger.info("copy file from " + fileName + " to " + targetFull);
    return true;
}

void ServiceRun::XCopy(const timemachine::Backuproot& backuproot)
{
    logger.info("Loading File list:" + backuproot.rootpath);
    std::vector<std::string> fileList;
    fileList.reserve(1024);
    std::map<std::string, int> mapFile;
    loadAllFiles(backuproot.rootpath, fileList);
    logger.info("Loading CurrentFile In Db:" + std::to_string(backuproot.id));

    if (auto stmt = m_sqliteHelper.prepareQuery(
            "select id,filepath from tb_backfiles where backuprootid=:rootid");
        stmt)
    {
        stmt->bind(":rootid", backuproot.id);
        while (stmt->executeStep())
        {
            const auto filepath = stmt->getColumn("filepath").getText();
            mapFile.emplace(filepath, stmt->getColumn("id").getInt());
        }
    }

    logger.info("begin xcopy! total:" + std::to_string(fileList.size()));
    std::size_t counter = 0;
    auto timestamp = Utils::getMilliTimeStamp() / 1000;
    for (const auto& file : fileList)
    {
        if (file.find("/.") != std::string::npos || file.find("\\.") != std::string::npos)
        {
            continue;
        }

        ++counter;
        const auto nowSec = Utils::getMilliTimeStamp() / 1000;
        if (nowSec != timestamp)
        {
            timestamp = nowSec;
            logger.info(
                "copy progress:" + std::to_string(counter * 100 / fileList.size()) +
                "%  " + std::to_string(counter) + "/" + std::to_string(fileList.size()));
        }

        int64_t id = 0;
        auto it = mapFile.find(file);
        if (it == mapFile.end())
        {
            // INSERT 新文件记录，然后回查拿到 id
            if (auto insertStmt = m_sqliteHelper.prepareQuery(
                    "insert into tb_backfiles "
                    "(backuprootid,filepath,versionhistorycnt,lastbackuptime) "
                    "values(:rootid,:filepath,0,datetime('now','localtime'))");
                insertStmt)
            {
                insertStmt->bind(":rootid", backuproot.id);
                insertStmt->bind(":filepath", file);
                insertStmt->exec();
            }

            if (auto selStmt = m_sqliteHelper.prepareQuery(
                    "select id from tb_backfiles "
                    "where filepath=:filepath and backuprootid=:rootid");
                selStmt)
            {
                selStmt->bind(":filepath", file);
                selStmt->bind(":rootid", backuproot.id);
                if (selStmt->executeStep())
                {
                    id = selStmt->getColumn("id").getInt();
                }
                else
                {
                    logger.error("内部错误！数据库异常，退出...");
                    break;
                }
            }
        }
        else
        {
            id = it->second;
        }

        if (auto histStmt = m_sqliteHelper.prepareQuery(
                "select * from tb_backfilehistory where backupfileid=:fileid "
                "order by id desc limit 1");
            histStmt)
        {
            histStmt->bind(":fileid", id);
            if (histStmt->executeStep())
            {
                const auto lastmotify = histStmt->getColumn("motifytime").getInt64();
                const auto filesize = histStmt->getColumn("filesize").getInt64();
                const std::string hash = histStmt->getColumn("md5").getString();
                const auto fidid = histStmt->getColumn("id").getInt();
                const auto fileU8 = u8path_from(file);
                const auto lastWriteTime = Utils::getSysFileMilliTimeStamp(fileU8);
                if (lastmotify == lastWriteTime &&
                    filesize == std::filesystem::file_size(fileU8))
                {
                    continue;
                }

                logger.info("motify time indb:" + std::to_string(lastmotify) +
                            " real:" + std::to_string(lastWriteTime) +
                            " filesize indb:" + std::to_string(filesize) +
                            " real:" + std::to_string(std::filesystem::file_size(fileU8)));

                if (lastmotify != lastWriteTime &&
                    filesize == std::filesystem::file_size(fileU8))
                {
                    const auto md5str = Utils::getFileMD5(file);
                    if (md5str == hash)
                    {
                        logger.info("historyfile id=[" + std::to_string(fidid) +
                                    "] backupid:[" + std::to_string(id) +
                                    "] not changed but motifytime diff, correcting...");
                        if (auto updStmt = m_sqliteHelper.prepareQuery(
                                "update tb_backfilehistory set motifytime=:mt "
                                "where backupfileid=:fileid and id=:histid");
                            updStmt)
                        {
                            updStmt->bind(":mt", lastWriteTime);
                            updStmt->bind(":fileid", id);
                            updStmt->bind(":histid", fidid);
                            updStmt->exec();
                        }
                        continue;
                    }
                    logger.info("hash indb:" + hash + " real:" + md5str);
                }
            }
        }

        if (!exeCopy(file, id))
        {
            logger.error("拷贝错误！退出...");
            break;
        }
        ++m_fileCopyCount;
        m_dataCopyCount += std::filesystem::file_size(u8path_from(file));
    }
}

int ServiceRun::beginbackup()
{
    m_sqliteHelper.execSql(
        "insert into tb_backup (begintime) values(datetime('now', 'localtime'))");
    if (auto ret = m_sqliteHelper.prepareQuery("select last_insert_rowid() as id");
        ret && ret->executeStep())
    {
        return ret->getColumn("id").getInt();
    }
    return -1;
}

void ServiceRun::finishbackup()
{
    if (auto stmt = m_sqliteHelper.prepareQuery(
            "update tb_backup set endtime=datetime('now','localtime'),"
            "filecopycount=:cnt,datacopycount=:bytes where id=:id");
        stmt)
    {
        stmt->bind(":cnt", m_fileCopyCount);
        stmt->bind(":bytes", m_dataCopyCount);
        stmt->bind(":id", m_backupId);
        stmt->exec();
    }
}

void ServiceRun::deleteByBackuprootid(int64_t rootid)
{
    std::size_t counter = 0;
    auto timestamp = Utils::getMilliTimeStamp() / 1000;
    logger.info("loading files backuprootid=" + std::to_string(rootid));
    if (auto ret = m_sqliteHelper.prepareQuery(
            "select id from tb_backfiles where backuprootid=:rootid");
        ret)
    {
        ret->bind(":rootid", rootid);
        std::vector<int64_t> tbbackfilesList;
        while (ret->executeStep())
        {
            tbbackfilesList.push_back(ret->getColumn("id").getInt());
        }

        for (const auto id : tbbackfilesList)
        {
            if (auto subret = m_sqliteHelper.prepareQuery(
                    "select id,backuptargetpath,backuptargetrootid "
                    "from tb_backfilehistory where backupfileid=:fileid");
                subret)
            {
                subret->bind(":fileid", id);
                while (subret->executeStep())
                {
                    // backuptargetpath 是相对于 target root 的路径，需要拼上 root 才能定位文件
                    const auto backuptargetrootid =
                        subret->getColumn("backuptargetrootid").getInt();
                    const auto fullPath =
                        (u8path_from(getTargetrootPath(backuptargetrootid)) /
                         u8path_from(subret->getColumn("backuptargetpath").getString()))
                            .u8string();
                    const auto u8path = u8path_from(fullPath);
                    if (std::filesystem::exists(u8path))
                    {
                        std::filesystem::remove(u8path);
                    }
                    else
                    {
                        logger.error("not found target path:" + fullPath);
                    }
                    if (auto delStmt = m_sqliteHelper.prepareQuery(
                            "delete from tb_backfilehistory where id=:histid");
                        delStmt)
                    {
                        delStmt->bind(":histid", subret->getColumn("id").getInt());
                        delStmt->exec();
                    }
                }
            }
            ++counter;
            const auto nowSec = Utils::getMilliTimeStamp() / 1000;
            if (nowSec != timestamp)
            {
                timestamp = nowSec;
                logger.info("delete progress:" +
                            std::to_string(static_cast<size_t>(counter) * 100 /
                                           tbbackfilesList.size()) +
                            "%  " + std::to_string(counter) + "/" +
                            std::to_string(tbbackfilesList.size()));
            }
        }
    }
    if (auto delStmt = m_sqliteHelper.prepareQuery(
            "delete from tb_backfiles where backuprootid=:rootid");
        delStmt)
    {
        delStmt->bind(":rootid", rootid);
        delStmt->exec();
    }
}

void ServiceRun::XCopy()
{
    try
    {
        m_backupId = beginbackup();
        if (m_backupId == -1)
        {
            return;
        }
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
        return;
    }

    for (const auto& backuproot : m_backupRootList)
    {
        try
        {
            XCopy(backuproot);
        }
        catch (const std::exception& e)
        {
            logger.error(e.what());
        }
    }
    finishbackup();
}

std::string ServiceRun::getTargetrootPath(int targetbkid)
{
    for (const auto& backuptargetroot : m_backupTargetRootList)
    {
        if (backuptargetroot.id == targetbkid)
        {
            return backuptargetroot.targetrootpath;
        }
    }
    return "";
}

void ServiceRun::removeWastedData(int64_t backupfilehistoryid,
                                  const std::string& backupfilefullpath)
{
    try
    {
        if (auto ret = m_sqliteHelper.prepareQuery(
                "select backupfileid from tb_backfilehistory where id=:histid");
            ret)
        {
            ret->bind(":histid", backupfilehistoryid);
            if (ret->executeStep())
            {
                const auto backupfileid = ret->getColumn("backupfileid").getInt64();
                if (auto delStmt = m_sqliteHelper.prepareQuery(
                        "delete from tb_backfilehistory where id=:histid");
                    delStmt)
                {
                    delStmt->bind(":histid", backupfilehistoryid);
                    delStmt->exec();
                }

                const auto u8path = u8path_from(backupfilefullpath);
                if (std::filesystem::exists(u8path))
                {
                    logger.info("delete broken file:" + backupfilefullpath);
                    std::filesystem::remove(u8path);
                }
                if (auto cntRet = m_sqliteHelper.prepareQuery(
                        "select count(*) from tb_backfilehistory where "
                        "backupfileid=:fileid");
                    cntRet)
                {
                    cntRet->bind(":fileid", backupfileid);
                    if (cntRet->executeStep())
                    {
                        const int historycnt = cntRet->getColumn("count(*)").getInt();
                        if (historycnt == 0)
                        {
                            if (auto delStmt2 = m_sqliteHelper.prepareQuery(
                                    "delete from tb_backfiles where id=:fileid");
                                delStmt2)
                            {
                                delStmt2->bind(":fileid", backupfileid);
                                delStmt2->exec();
                            }
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
    }
}

void ServiceRun::checkdata(bool withhash)
{
    try
    {
        logger.info("loading all file and check");
        int innercounter = 0;
        std::vector<timemachine::BackupHistory> historyList;
        auto timestamp = Utils::getMilliTimeStamp() / 1000;
        while (true)
        {
            int counter = 0;
            if (auto ret = m_sqliteHelper.prepareQuery(
                    "select * from tb_backfilehistory limit :offset,1000");
                ret)
            {
                ret->bind(":offset", innercounter);
                while (ret->executeStep())
                {
                    timemachine::BackupHistory backupHistory;
                    backupHistory.id = ret->getColumn("id").getInt();
                    backupHistory.md5 = ret->getColumn("md5").getString();
                    backupHistory.filesize = ret->getColumn("filesize").getInt64();
                    backupHistory.backupfileid =
                        ret->getColumn("backupfileid").getInt64();
                    backupHistory.backuptargetrootid =
                        ret->getColumn("backuptargetrootid").getInt();
                    backupHistory.backuptargetpath =
                        ret->getColumn("backuptargetpath").getString();

                    const std::string backuprootpath =
                        getTargetrootPath(backupHistory.backuptargetrootid) +
                        backupHistory.backuptargetpath;
                    backupHistory.backuptargetfullpath = backuprootpath;

                    const auto u8path = u8path_from(backuprootpath);
                    if (!std::filesystem::exists(u8path) ||
                        std::filesystem::file_size(u8path) !=
                            static_cast<std::uintmax_t>(backupHistory.filesize))
                    {
                        historyList.emplace_back(backupHistory);
                    }
                    else if (withhash)
                    {
                        const std::string md5str = Utils::getFileMD5(backuprootpath);
                        if (backupHistory.md5 != md5str)
                        {
                            logger.info("file hash not mismatch:" + backuprootpath);
                            historyList.emplace_back(backupHistory);
                        }
                    }
                    ++counter;
                    const auto nowSec = Utils::getMilliTimeStamp() / 1000;
                    if (nowSec != timestamp)
                    {
                        timestamp = nowSec;
                        logger.info(
                            "check num:" + std::to_string(innercounter + counter) +
                            " found:" + std::to_string(historyList.size()));
                    }
                }
            }
            if (counter == 0)
            {
                break;
            }
            innercounter += counter;
        }

        innercounter = 0;
        logger.info("begin removing wasted backup file records");
        for (const auto& backupHistory : historyList)
        {
            ++innercounter;
            removeWastedData(backupHistory.id, backupHistory.backuptargetfullpath);
            const auto nowSec = Utils::getMilliTimeStamp() / 1000;
            if (nowSec != timestamp)
            {
                timestamp = nowSec;
                logger.info("rm db count:" + std::to_string(innercounter) + " / " +
                            std::to_string(static_cast<size_t>(innercounter) * 100 /
                                           historyList.size()) +
                            "%");
            }
        }
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
    }
}

void ServiceRun::listBackupPaths()
{
    logger.info("Source Backup Paths:");
    for (const auto& backuproot : m_backupRootList)
    {
        logger.info(" - " + backuproot.rootpath);
    }
    logger.info("Target Backup Paths:");
    for (const auto& backuptargetroot : m_backupTargetRootList)
    {
        logger.info(" - " + backuptargetroot.targetrootpath);
    }
}

bool ServiceRun::addSourcePath(const std::string& source)
{
    const auto path = std::filesystem::path(source);
    if (std::filesystem::exists(path))
    {
        if (auto ret = m_sqliteHelper.prepareQuery(
                "insert into tb_backuproot(rootpath) values(:value)");
            ret)
        {
            ret->bind(":value", path.u8string());
            if (ret->exec())
            {
                logger.info("add source path success:" + source);
                return true;
            }
        }
    }
    return false;
}

bool ServiceRun::addTargetPath(const std::string& target)
{
    const auto path = std::filesystem::path(target);
    if (std::filesystem::exists(path))
    {
        if (auto ret = m_sqliteHelper.prepareQuery(
                "insert into tb_backuptargetroot(targetrootpath) "
                "values(:value)");
            ret)
        {
            ret->bind(":value", path.u8string());
            if (ret->exec())
            {
                logger.info("add target path success: " + target);
                return true;
            }
        }
    }
    return false;
}

bool ServiceRun::removeSourcePath(const std::string& source)
{
    if (auto ret = m_sqliteHelper.prepareQuery(
            "delete from tb_backuproot where rootpath = :value");
        ret)
    {
        ret->bind(":value", std::filesystem::path(source).u8string());
        if (ret->exec())
        {
            logger.info("delete source path success: " + source);
            return true;
        }
    }
    return false;
}

bool ServiceRun::removeTargetPath(const std::string& target)
{
    if (auto ret = m_sqliteHelper.prepareQuery(
            "delete from tb_backuptargetroot where targetrootpath = :value");
        ret)
    {
        ret->bind(":value", std::filesystem::path(target).u8string());
        if (ret->exec())
        {
            logger.info("delete target path success: " + target);
            return true;
        }
    }
    return false;
}

bool ServiceRun::restoreFile(const std::string& filePath)
{
    auto path = std::filesystem::path(filePath);
    const auto originFileName = path.filename();
    auto safeFilePath = Utils::replace(path.u8string(), "\\", "\\\\");
    const std::string sql =
        "select * from tb_backfilehistory, tb_backfiles, "
        "tb_backuptargetroot "
        "where tb_backfilehistory.backupfileid = tb_backfiles.id "
        "and tb_backfilehistory.backuptargetrootid = "
        "tb_backuptargetroot.id "
        "and tb_backfiles.filepath = :value";
    if (auto ret = m_sqliteHelper.prepareQuery(sql); ret)
    {
        ret->bind(":value", safeFilePath);
        std::vector<std::filesystem::path> allPath;
        allPath.reserve(8);
        int cnt = 0;
        while (ret->executeStep())
        {
            const auto targetPath = ret->getColumn("targetrootpath").getString();
            const auto fullBackupPath =
                u8path_from(targetPath) / u8path_from(ret->getColumn("backuptargetpath").getString());
            const auto modifyTime =
                static_cast<time_t>(ret->getColumn("motifytime").getInt64());

            allPath.emplace_back(fullBackupPath);
            logger.info("[" + std::to_string(++cnt) +
                        "]: " + Utils::Date::getDateFromMillis(modifyTime));
        }
        if (cnt)
        {
            int n = 0;
            logger.info("若要恢复指定时间的版本，请输入对应时间的编号");
            std::cin >> n;
            --n;
            if (n >= 0 && n < static_cast<int>(allPath.size()) &&
                std::filesystem::exists(allPath.at(n)))
            {
                std::filesystem::copy(allPath.at(n),
                                      path.replace_filename(originFileName),
                                      std::filesystem::copy_options::overwrite_existing);
                logger.info("restore file success: " + filePath);
                return true;
            }
        }
        else
        {
            logger.info("没有文件: " + filePath + " 的早期版本");
        }
    }
    return false;
}
