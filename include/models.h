// models.hpp - C++ equivalents for Java model classes
#pragma once

#include <string>

namespace timemachine
{

struct BackupHistory
{
    int id = 0;
    long backupfileid = 0;
    long filesize = 0;
    long motifytime = 0;
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
    long long spaceRemain = 0;
};

}  // namespace timemachine
