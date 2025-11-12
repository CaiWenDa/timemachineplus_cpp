#pragma once
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <openssl/md5.h>
#include <mutex>

namespace Utils
{
    class Date
    {
    public:
        static std::string getDate()
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

        void info(const std::string& str)
        {
            log("INFO", str);
        }

        void error(const std::string& str)
        {
            log("ERROR", str);
        }

        void debug(const std::string& str)
        {
            log("DEBUG", str);
        }

        void warn(const std::string& str)
        {
            log("WARN", str);
        }

    private:
        void log(const std::string& level, const std::string& message)
        {
            std::lock_guard<std::mutex> lock(logMutex);
            std::string timestamp = Date::getDate();
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

    inline long long getTimeStamp()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string replace(std::string str, const std::string& from, const std::string& to);
    std::string trim(const std::string& str);
    std::string getFileMD5(const std::string& filePath);

}