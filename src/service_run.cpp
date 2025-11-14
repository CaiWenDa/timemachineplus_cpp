#include "service_run.h"

#include <algorithm>
#include <chrono>
#include <codecvt>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "util.h"

const std::string ServiceRun::targetBkDirName = "BACKUPDATABASE";
Utils::Log ServiceRun::logger;

void ServiceRun::init()
{
    logger.info("init db...");
    if (!m_sqliteHelper.valid())
    {
        logger.error("SQLite init failed");
    }
}

void ServiceRun::loadBackupRoot()
{
    logger.info("loadBackupRoot");
    auto res = m_sqliteHelper.prepareQuery("select * from tb_backuproot");
    if (res)
    {
        while (res->executeStep())
        {
            timemachine::Backuproot backuproot;
            backuproot.id = res->getColumn("id").getInt();
            backuproot.rootpath = res->getColumn("rootpath").getString();
            if (!std::filesystem::exists(
                    std::filesystem::u8path(backuproot.rootpath)))
            {
                logger.error("No found backup source: " + backuproot.rootpath);
            }
            else
            {
                m_backupRootList.push_back(backuproot);
            }
        }
    }

    res = m_sqliteHelper.prepareQuery("select * from tb_backuptargetroot");
    if (res)
    {
        while (res->executeStep())
        {
            timemachine::Backuptargetroot backuptargetroot;
            backuptargetroot.id = res->getColumn("id").getInt();
            backuptargetroot.targetrootpath =
                res->getColumn("targetrootpath").getString();
            backuptargetroot.targetrootdir = targetBkDirName;
            auto u8path =
                std::filesystem::u8path(backuptargetroot.targetrootpath);
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
                backuptargetroot.spaceRemain = static_cast<long long>(
                    std::filesystem::space(u8path).available);
                m_backupTargetRootList.push_back(backuptargetroot);
            }
        }
    }
}

void ServiceRun::loadAllFiles(const std::string& pathName,
                              std::vector<std::string>& fileList)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             std::filesystem::u8path(pathName)))
    {
        if (!std::filesystem::is_directory(entry))
        {
            const auto path = entry.path().u8string();
            if (path.find("/.") == std::string::npos &&
                path.find("\\.") == std::string::npos)
            {
                fileList.push_back(path);
            }
        }
    }
}

timemachine::Backuptargetroot ServiceRun::getAvailableTarget(
    long long needspace)
{
    std::vector<timemachine::Backuptargetroot> avaliedTarget;
    for (auto& backupTargetRoot : m_backupTargetRootList)
    {
        auto u8path = std::filesystem::u8path(backupTargetRoot.targetrootpath);
        backupTargetRoot.spaceRemain = std::filesystem::space(u8path).available;
        if (backupTargetRoot.spaceRemain > needspace)
        {
            avaliedTarget.push_back(backupTargetRoot);
        }
    }
    if (!avaliedTarget.empty())
    {
        return *std::max_element(avaliedTarget.begin(), avaliedTarget.end(),
                                 [](const timemachine::Backuptargetroot& a,
                                    const timemachine::Backuptargetroot& b)
                                 { return a.spaceRemain < b.spaceRemain; });
    }
    return timemachine::Backuptargetroot();
}

void ServiceRun::copyFile(const std::string& source, const std::string& dest)
{
    try
    {
        auto destDir = std::filesystem::u8path(dest).parent_path();
        if (!std::filesystem::exists(destDir))
        {
            std::filesystem::create_directories(destDir);
        }
        std::filesystem::copy_file(std::filesystem::u8path(source), dest);
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
        throw e;
    }
}

bool ServiceRun::exeCopy(const std::string& fileName, long backupfileid)
{
    auto u8fileName = std::filesystem::u8path(fileName);
    auto fileSize = std::filesystem::file_size(u8fileName);
    auto lastWriteTime = Utils::getSysFileMilliTimeStamp(u8fileName);
    timemachine::Backuptargetroot backuptargetroot =
        getAvailableTarget(fileSize);
    if (!backuptargetroot.spaceRemain)
    {
        logger.error("no space in all targetbackups! need:" +
                     std::to_string(fileSize));
        return false;
    }
    std::string md5str = "";
    try
    {
        md5str = Utils::getFileMD5(fileName);
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
    }

    auto timeStamp = Utils::getMilliTimeStamp();
    std::string targetName = backuptargetroot.targetrootpath + "/" +
                             backuptargetroot.targetrootdir + "/" + md5str +
                             "_" + std::to_string(timeStamp);
    std::string targetNameSave = "/" + backuptargetroot.targetrootdir + "/" +
                                 md5str + "_" + std::to_string(timeStamp);
    auto begincopysingle = Utils::Date::getCurrentDateTime();
    try
    {
        copyFile(fileName, targetName);
    }
    catch (const std::exception&)
    {
        logger.error("failed to copy file from " + fileName + " to " +
                     targetName);
        return false;
    }

    m_sqliteHelper.execSql(
        "insert into tb_backfilehistory "
        "(backupfileid,backupid,motifytime,filesize,copystarttime,copyendtime,"
        "backuptargetpath,backuptargetrootid,md5)"
        " values (" +
        std::to_string(backupfileid) + "," + std::to_string(m_backupId) + "," +
        std::to_string(lastWriteTime) + "," + std::to_string(fileSize) + ",'" +
        begincopysingle + "','" + Utils::Date::getCurrentDateTime() + "','" +
        targetNameSave + "'," + std::to_string(backuptargetroot.id) + ",'" +
        md5str + "')");
    logger.info("copy file from " + fileName + " to " + targetName);
    return true;
}

void ServiceRun::XCopy(const timemachine::Backuproot& backuproot)
{
    logger.info("Loading File list:" + backuproot.rootpath);
    std::vector<std::string> fileList;
    std::map<std::string, int> mapFile;
    loadAllFiles(backuproot.rootpath, fileList);
    logger.info("Loading CurrentFile In Db:" + std::to_string(backuproot.id));

    std::string sqlquery =
        "select id,filepath from tb_backfiles where backuprootid=" +
        std::to_string(backuproot.id);
    auto ret = m_sqliteHelper.prepareQuery(sqlquery);
    if (ret)
    {
        while (ret->executeStep())
        {
            auto id = ret->getColumn("id").getInt();
            auto x = ret->getColumn("filepath").getText();
            mapFile[ret->getColumn("filepath").getText()] =
                ret->getColumn("id").getInt();
        }
    }
    logger.info("begin xcopy! total:" + std::to_string(fileList.size()));
    long long counter = 0;
    long long timestamp = Utils::getMilliTimeStamp() / 1000;
    for (const auto& file : fileList)
    {
        if (file.find("/.") != std::string::npos ||
            file.find("\\.") != std::string::npos)
        {
            continue;
        }
        counter++;
        if (Utils::getMilliTimeStamp() / 1000 != timestamp)
        {
            timestamp = Utils::getMilliTimeStamp() / 1000;
            logger.info("copy progress:" +
                        std::to_string(counter * 100 / fileList.size()) +
                        "%  " + std::to_string(counter) + "/" +
                        std::to_string(fileList.size()));
        }

        long id = 0;
        if (mapFile.find(file) == mapFile.end())
        {
            std::string sqlquery =
                "select * from tb_backfiles where filepath='" +
                Utils::replace(Utils::replace(file, "\\", "\\\\"), "'", "\\'") +
                "' and backuprootid=" + std::to_string(backuproot.id);

            m_sqliteHelper.execSql(
                "insert into tb_backfiles "
                "(backuprootid,filepath,versionhistorycnt,lastbackuptime) "
                "values (" +
                std::to_string(backuproot.id) + ",'" +
                Utils::replace(Utils::replace(file, "\\", "\\\\"), "'", "\\'") +
                "',0,datetime('now'))");
            auto ret = m_sqliteHelper.prepareQuery(sqlquery);
            if (!ret || !ret->executeStep())
            {
                logger.error("内部错误！数据库异常，退出...");
                break;
            }
            id = ret->getColumn("id").getInt();
        }
        else
        {
            id = mapFile.at(file);
        }

        auto ret = m_sqliteHelper.prepareQuery(
            "select * from tb_backfilehistory where backupfileid=" +
            std::to_string(id) + " order by id desc limit 1");
        auto u8path = std::filesystem::u8path(file);
        if (ret && ret->executeStep())
        {
            long long lastmotify = ret->getColumn("motifytime").getInt64();
            long long filesize = ret->getColumn("filesize").getInt64();
            std::string hash = ret->getColumn("md5").getString();
            long fidid = ret->getColumn("id").getInt();
            auto lastWriteTime = Utils::getSysFileMilliTimeStamp(u8path);
            if (lastmotify == lastWriteTime &&
                filesize == std::filesystem::file_size(u8path))
            {
                continue;
            }
            logger.info(
                "motify time indb:" + std::to_string(lastmotify) +
                " real:" + std::to_string(lastWriteTime) +
                " filesize indb:" + std::to_string(filesize) +
                " real:" + std::to_string(std::filesystem::file_size(u8path)));

            if (lastmotify != lastWriteTime &&
                filesize == std::filesystem::file_size(u8path))
            {
                std::string md5str = Utils::getFileMD5(file);
                if (md5str == hash)
                {
                    logger.info(
                        "historyfile id=[" + std::to_string(fidid) +
                        "] backupid:[" + std::to_string(id) +
                        "] not changed but motifytime diff, correcting...");
                    m_sqliteHelper.execSql(
                        "update tb_backfilehistory set motifytime=" +
                        std::to_string(lastWriteTime) +
                        " where backupfileid=" + std::to_string(id) +
                        " and id=" + std::to_string(fidid));
                    continue;
                }
                logger.info("hash indb:" + hash + " real:" + md5str);
            }
        }
        if (!exeCopy(file, id))
        {
            logger.error("拷贝错误！退出...");
            break;
        }
        m_fileCopyCount++;
        m_dataCopyCount += std::filesystem::file_size(u8path);
    }
}

int ServiceRun::beginbackup()
{
    m_sqliteHelper.execSql(
        "insert into tb_backup (begintime) values(datetime('now'))");
    auto ret = m_sqliteHelper.prepareQuery("select last_insert_rowid() as id");
    if (ret && ret->executeStep())
    {
        return ret->getColumn("id").getInt();
    }
    return -1;
}

void ServiceRun::finishbackup()
{
    m_sqliteHelper.execSql(
        "update tb_backup set endtime=datetime('now'),filecopycount=" +
        std::to_string(m_fileCopyCount) +
        ",datacopycount=" + std::to_string(m_dataCopyCount) +
        " where id=" + std::to_string(m_backupId));
}

void ServiceRun::deleteByBackuprootid(long rootid)
{
    long counter = 0;
    long long timestamp = Utils::getMilliTimeStamp() / 1000;
    logger.info("loading files backuprootid=" + std::to_string(rootid));
    auto ret = m_sqliteHelper.prepareQuery(
        "select id from tb_backfiles where backuprootid=" +
        std::to_string(rootid));
    std::vector<long> tbbackfilesList;

    if (ret)
    {
        while (ret->executeStep())
        {
            tbbackfilesList.push_back(ret->getColumn("id").getInt());
        }
    }

    for (const long id : tbbackfilesList)
    {
        auto subret = m_sqliteHelper.prepareQuery(
            "select id,backuptargetpath from tb_backfilehistory where "
            "backupfileid=" +
            std::to_string(id));
        if (subret && subret->executeStep())
        {
            auto file = subret->getColumn("backuptargetpath").getString();
            auto u8path = std::filesystem::u8path(file);
            if (std::filesystem::exists(u8path))
            {
                std::filesystem::remove(u8path);
            }
            else
            {
                logger.error("not found target path:" + file);
            }
            m_sqliteHelper.execSql(
                "delete from tb_backfilehistory where id=" +
                std::to_string(subret->getColumn("id").getInt()));
        }
        counter++;
        if (Utils::getMilliTimeStamp() / 1000 != timestamp)
        {
            timestamp = Utils::getMilliTimeStamp() / 1000;
            logger.info("delete progress:" +
                        std::to_string(static_cast<size_t>(counter) * 100 /
                                       tbbackfilesList.size()) +
                        "%  " + std::to_string(counter) + "/" +
                        std::to_string(tbbackfilesList.size()));
        }
    }
    m_sqliteHelper.execSql("delete from tb_backfiles where backuprootid=" +
                           std::to_string(rootid));
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

void ServiceRun::removeWastedData(long backupfilehistoryid,
                                  const std::string& backupfilefullpath)
{
    try
    {
        auto ret = m_sqliteHelper.prepareQuery(
            "select backupfileid from tb_backfilehistory where id=" +
            std::to_string(backupfilehistoryid));
        if (ret && ret->executeStep())
        {
            long backupfileid = ret->getColumn("backupfileid").getInt();
            m_sqliteHelper.execSql("delete from tb_backfilehistory where id=" +
                                   std::to_string(backupfilehistoryid));

            auto u8path = std::filesystem::u8path(backupfilefullpath);
            if (std::filesystem::exists(u8path))
            {
                logger.info("delete broken file:" + backupfilefullpath);
                std::filesystem::remove(u8path);
            }
            ret = m_sqliteHelper.prepareQuery(
                "select count(*) from tb_backfilehistory where backupfileid=" +
                std::to_string(backupfileid));
            if (ret && ret->executeStep())
            {
                int historycnt = ret->getColumn("count(*)").getInt();
                if (historycnt == 0)
                {
                    m_sqliteHelper.execSql(
                        "delete from tb_backfiles where id=" +
                        std::to_string(backupfileid));
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
            auto ret = m_sqliteHelper.prepareQuery(
                "select * from tb_backfilehistory limit " +
                std::to_string(innercounter) + ",1000");
            if (ret)
            {
                while (ret->executeStep())
                {
                    timemachine::BackupHistory backupHistory;
                    backupHistory.id = (ret->getColumn("id").getInt());
                    backupHistory.md5 = (ret->getColumn("md5").getString());
                    backupHistory.filesize =
                        (ret->getColumn("filesize").getInt());
                    backupHistory.backupfileid =
                        (ret->getColumn("backupfileid").getInt());
                    backupHistory.backuptargetrootid =
                        (ret->getColumn("backuptargetrootid").getInt());
                    backupHistory.backuptargetpath =
                        (ret->getColumn("backuptargetpath").getString());

                    std::string backuprootpath =
                        getTargetrootPath(backupHistory.backuptargetrootid) +
                        backupHistory.backuptargetpath;
                    backupHistory.backuptargetfullpath = backuprootpath;

                    auto u8path = std::filesystem::u8path(backuprootpath);
                    if (!std::filesystem::exists(u8path) ||
                        std::filesystem::file_size(u8path) !=
                            backupHistory.filesize)
                    {
                        historyList.push_back(backupHistory);
                    }
                    else if (withhash)
                    {
                        std::string md5str = Utils::getFileMD5(backuprootpath);
                        if (backupHistory.md5 != md5str)
                        {
                            logger.info("file hash not mismatch:" +
                                        backuprootpath);
                            historyList.push_back(backupHistory);
                        }
                    }
                    counter++;
                    if (Utils::getMilliTimeStamp() / 1000 != timestamp)
                    {
                        timestamp = Utils::getMilliTimeStamp() / 1000;
                        logger.info(
                            "check num:" +
                            std::to_string(innercounter + counter) +
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
        logger.info("begin removing wasted backup file recoreds");
        for (const auto& backupHistory : historyList)
        {
            innercounter++;
            removeWastedData(backupHistory.id,
                             backupHistory.backuptargetfullpath);
            if (Utils::getMilliTimeStamp() / 1000 != timestamp)
            {
                timestamp = Utils::getMilliTimeStamp() / 1000;
                logger.info("rm db count:" + std::to_string(innercounter) +
                            " / " +
                            std::to_string(static_cast<size_t>(innercounter) *
                                           100 / historyList.size()) +
                            "%");
            }
        }
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
    }
}

bool ServiceRun::addSourcePath(const std::string& source)
{
    auto path = std::filesystem::path(source);
    if (std::filesystem::exists(path))
    {
        auto ret = m_sqliteHelper.prepareQuery(
            "insert into tb_backuproot(rootpath)"
            " values(:value)");
        if (ret)
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
    auto path = std::filesystem::path(target);
    if (std::filesystem::exists(path))
    {
        auto ret = m_sqliteHelper.prepareQuery(
            "insert into tb_backuptargetroot(targetrootpath)"
            " values(:value)");
        if (ret)
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
    auto ret = m_sqliteHelper.prepareQuery(
        "delete from tb_backuproot"
        " where rootpath = :value");
    if (ret)
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
    auto ret = m_sqliteHelper.prepareQuery(
        "delete from tb_backuptargetroot"
        " where targetrootpath = :value");
    if (ret)
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

    if (std::filesystem::exists(path))
    {
        auto originFileName = path.filename();
        auto uniFilePath = path.u8string();
        std::replace(uniFilePath.begin(), uniFilePath.end(), '\\', '/');
        std::string sql =
            "select *"
            "from tb_backfilehistory, tb_backfiles, tb_backuptargetroot "
            "where tb_backfilehistory.backupfileid = tb_backfiles.id "
            "and tb_backfilehistory.backuptargetrootid = "
            "tb_backuptargetroot.id "
            "and tb_backfiles.filepath = :value";
        auto ret = m_sqliteHelper.prepareQuery(sql);
        if (ret)
        {
            ret->bind(":value", uniFilePath);
            std::vector<std::filesystem::path> allPath;
            int cnt = 0;
            while (ret->executeStep())
            {
                auto targetPath = ret->getColumn("targetrootpath").getString();
                auto fullBackupPath = std::filesystem::u8path(
                    targetPath +
                    ret->getColumn("backuptargetpath").getString());
                auto modifyTime = static_cast<time_t>(
                    ret->getColumn("motifytime").getInt64());

                allPath.push_back(fullBackupPath);
                logger.info("[" + std::to_string(++cnt) +
                            "]: " + Utils::Date::getDateFromMillis(modifyTime));
            }
            logger.info("若要恢复指定时间的版本，请输入对应时间的编号");
            if (cnt)
            {
                int n = 0;
                std::cin >> n;
                n--;
                if (n >= 0 && n < allPath.size() &&
                    std::filesystem::exists(allPath.at(n)))
                {
                    std::filesystem::copy(
                        allPath.at(n), path.replace_filename(originFileName),
                        std::filesystem::copy_options::overwrite_existing);
                    logger.info(
                        "restore file success: " +
                        path.replace_filename(originFileName).u8string());
                    return true;
                }
            }
        }
    }
    return false;
}
