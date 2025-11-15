#pragma once

#include <vector>
#include <cstdint>

#include "models.h"
#include "sqlite_helper.h"
#include "util.h"

class ServiceRun
{
   public:
    ServiceRun() : m_sqliteHelper("timemachine.db") {}
    void init();
    void loadBackupRoot();
    void deleteByBackuprootid(int64_t rootid);
    void XCopy();
    void checkdata(bool withhash);
    void listBackupPaths();
    bool addSourcePath(const std::string& source);
    bool addTargetPath(const std::string& target);
    bool removeSourcePath(const std::string& source);
    bool removeTargetPath(const std::string& target);
    bool restoreFile(const std::string& filePath);

   private:
    static void loadAllFiles(const std::string& pathName,
                             std::vector<std::string>& fileList);
    std::optional<timemachine::Backuptargetroot> getAvailableTarget(uintmax_t needspace);
    static void copyFile(const std::string& source, const std::string& dest);
    bool exeCopy(const std::string& fileName, int64_t backupfileid);
    void XCopy(const timemachine::Backuproot& backuproot);
    int beginbackup();
    void finishbackup();
    std::string getTargetrootPath(int targetbkid);
    void removeWastedData(int64_t backupfilehistoryid,
                          const std::string& backupfilefullpath);

   private:
    SQLiteHelper m_sqliteHelper;
    inline static Utils::Log logger;
    std::vector<timemachine::Backuproot> m_backupRootList;
    std::vector<timemachine::Backuptargetroot> m_backupTargetRootList;
    int64_t m_fileCopyCount = 0;
    int64_t m_dataCopyCount = 0;
    int m_backupId = 0;
    inline static constexpr std::string_view targetBkDirName = "BACKUPDATABASE";
};