#pragma once

#include <cstdlib>
#include <cstdint>
#include <vector>

#include "config.h"
#include "models.h"
#include "sqlite_helper.h"
#include "util.h"

class ServiceRun
{
   public:
    ServiceRun()
        : m_sqliteHelper(
              // 支持 TIMEMACHINE_DB 环境变量覆盖 DB 路径（便于测试和多实例场景）
              []() {
                  if (const char* env = std::getenv("TIMEMACHINE_DB")) {
                      return std::string(env[0] ? env : timemachineplus::config::kDbName);
                  }
                  return std::string(timemachineplus::config::kDbName);
              }())
    {}
    void init();
    void loadBackupRoot();
    void deleteByBackuprootid(int64_t rootid);
    void XCopy();
    // 检查备份是否损坏并移除损坏备份
    void checkdata(bool withhash);
    void listBackupPaths();
    bool addSourcePath(const std::string& source);
    bool addTargetPath(const std::string& target);
    bool removeSourcePath(const std::string& source);
    bool removeTargetPath(const std::string& target);
    bool restoreFile(const std::string& filePath);

    // --- API 层需要的公开访问器 ---
    SQLiteHelper& sqliteHelper() { return m_sqliteHelper; }
    std::string getTargetrootPath(int targetbkid);  // 供 restore / api_server 复用
    const std::vector<timemachine::Backuproot>& backupRoots() const { return m_backupRootList; }
    const std::vector<timemachine::Backuptargetroot>& backupTargets() const { return m_backupTargetRootList; }

   private:
    static void loadAllFiles(const std::string& pathName,
                             std::vector<std::string>& fileList);
    std::optional<timemachine::Backuptargetroot> getAvailableTarget(uintmax_t needspace);
    static void copyFile(const std::string& source, const std::string& dest);
    bool exeCopy(const std::string& fileName, int64_t backupfileid);
    void XCopy(const timemachine::Backuproot& backuproot);
    int beginbackup();
    void finishbackup();
    void removeWastedData(int64_t backupfilehistoryid,
                          const std::string& backupfilefullpath);

   private:
    SQLiteHelper m_sqliteHelper;
    inline static Utils::Log logger{timemachineplus::config::kLogFile};
    std::vector<timemachine::Backuproot> m_backupRootList;
    std::vector<timemachine::Backuptargetroot> m_backupTargetRootList;
    int64_t m_fileCopyCount = 0;
    int64_t m_dataCopyCount = 0;
    int m_backupId = 0;
    inline static constexpr std::string_view targetBkDirName =
        timemachineplus::config::kBackupDirName;
};