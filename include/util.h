#pragma once
#include <openssl/md5.h>

#include <chrono>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace Utils
{
class Date
{
   public:
    static std::string getCurrentDateTime()
    {
        // 获取当前时间点
        auto now = std::chrono::system_clock::now();
        // 转换为time_t以便使用std::put_time
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        // 使用stringstream和put_time格式化时间
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");

        return ss.str();
    }

    static std::string getDateFromMillis(time_t millisec)
    {
        std::stringstream ss;
        std::chrono::milliseconds ms(millisec);
        std::chrono::time_point<std::chrono::system_clock> tp(ms);
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        if (auto tm = std::localtime(&t))
        {
            ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        }
        return ss.str();
    }
};

class Log
{
   private:
    std::ofstream logFile;
    std::mutex logMutex;

   public:
    Log(const std::string& fileName = "log.log")
    {
        logFile.open(fileName, std::ios::app);
        if (!logFile.is_open())
        {
            std::cerr << "Failed to open log file: " << fileName << std::endl;
        }
    }

    ~Log()
    {
        if (logFile.is_open())
        {
            logFile.close();
        }
    }

    void info(const std::string& str) { log("INFO", str); }

    void error(const std::string& str) { log("ERROR", str); }

    void debug(const std::string& str) { log("DEBUG", str); }

    void warn(const std::string& str) { log("WARN", str); }

   private:
    void log(const std::string& level, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(logMutex);
        std::string timestamp = Date::getCurrentDateTime();
        std::string logMessage = "[" + timestamp + "] [" + level + "] " + message;

        // Write to console
        std::cerr << logMessage << std::endl;

        // Write to file
        if (logFile.is_open())
        {
            logFile << logMessage << std::endl;
        }
    }
};

std::string replace(std::string str, const std::string& from, const std::string& to);
std::string trim(const std::string& str);
std::string getFileMD5(const std::string& filePath);

inline int64_t getMilliTimeStamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline time_t getSysFileMilliTimeStamp(const std::filesystem::path& filePath)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               (std::filesystem::last_write_time(filePath) -
                std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now())
                   .time_since_epoch())
        .count();
}

}  // namespace Utils