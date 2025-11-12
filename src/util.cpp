#include "util.h"
#include <filesystem>

std::string Utils::replace(std::string str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

std::string Utils::getFileMD5(const std::string& filePath)
{
    auto u8path = std::filesystem::u8path(filePath);
    std::ifstream file(u8path, std::ifstream::binary);
    if (!file)
    {
        return "";  // 文件打开失败
    }

    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    constexpr size_t bufferSize = 4096;
    char buffer[bufferSize];
    while (file.good())
    {
        file.read(buffer, bufferSize);
        MD5_Update(&md5Context, buffer, file.gcount());
    }

    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5Context);

    std::stringstream ss;
    for (const auto& byte : hash)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }

    return ss.str();
}

std::string Utils::trim(const std::string& str)
{
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}
