#pragma once

#include <vector>

#include "models.h"
#include "sqlite_helper.h"
#include "util.h"

class ServiceRun
{
   public:
    ServiceRun() : m_sqliteHelper("timemachine.db") {}
    void init();
    void loadBackupRoot();
    void deleteByBackuprootid(long rootid);
    void XCopy();
    void checkdata(bool withhash);
    bool addSourcePath(const std::string& source);
    bool addTargetPath(const std::string& target);
    bool removeSourcePath(const std::string& source);
    bool removeTargetPath(const std::string& target);
    bool restoreFile(const std::string& filePath);

   private:
    static void loadAllFiles(const std::string& pathName,
                             std::vector<std::string>& fileList);
    timemachine::Backuptargetroot getAvailableTarget(long long needspace);
    static void copyFile(const std::string& source, const std::string& dest);
    bool exeCopy(const std::string& fileName, long backupfileid);
    void XCopy(const timemachine::Backuproot& backuproot);
    int beginbackup();
    void finishbackup();
    std::string getTargetrootPath(int targetbkid);
    void removeWastedData(long backupfilehistoryid,
                          const std::string& backupfilefullpath);

   private:
    SQLiteHelper m_sqliteHelper;
    static Utils::Log logger;
    std::vector<timemachine::Backuproot> m_backupRootList;
    std::vector<timemachine::Backuptargetroot> m_backupTargetRootList;
    long m_fileCopyCount = 0;
    long long m_dataCopyCount = 0;
    int m_backupId = 0;
    static const std::string targetBkDirName;
};