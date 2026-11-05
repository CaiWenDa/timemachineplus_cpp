#pragma once

#include <cstddef>
#include <string>

namespace timemachineplus
{
namespace config
{

inline constexpr const char* kDbName = "timemachine.db";
inline constexpr const char* kLogFile = "log.log";
inline constexpr const char* kSchemaFile = "timemachine.sql";
inline constexpr const char* kBackupDirName = "BACKUPDATABASE";

inline constexpr std::size_t kCheckdataBatchSize = 1000;
inline constexpr std::size_t kMd5BufferSize = 8192;

}  // namespace config
}  // namespace timemachineplus
