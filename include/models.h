// models.hpp - C++ equivalents for Java model classes
#pragma once

#include <string>
#include <cstdint>

namespace timemachine
{

struct BackupHistory
{
    int id = 0;
    int64_t backupfileid = 0;
    int64_t filesize = 0;
    int64_t motifytime = 0;
    std::string backuptargetpath;
    std::string backuptargetfullpath;
    int backuptargetrootid = 0;
    std::string md5;
};

struct Backuproot
{
    int id = 0;
    std::string rootpath;
};

struct Backuptargetroot
{
    int id = 0;
    std::string targetrootpath;
    std::string targetrootdir;
    uintmax_t spaceRemain = 0;
};

}  // namespace timemachine
