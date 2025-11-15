#include <charconv>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string_view>

#include "service_run.h"
#include "util.h"

int main(int argc, char* argv[])
{
    std::cout << "timemachineplus C++ version (C++17)" << std::endl;

    Utils::Log logger;
    try
    {
        ServiceRun serviceRun;
        serviceRun.init();
        serviceRun.loadBackupRoot();

        if (argc >= 2)
        {
            std::string_view cmd{argv[1]};

            if (cmd == "checkdata")
            {
                logger.info("begin to checkdata");
                serviceRun.checkdata(false);
            }
            else if (cmd == "checkdatawithhash")
            {
                logger.info("begin to checkdata with hash");
                serviceRun.checkdata(true);
            }
            else if (cmd == "add")
            {
                if (argc == 4)
                {
                    std::string_view flag{argv[2]};
                    if (flag == "-s")
                    {
                        return !serviceRun.addSourcePath(argv[3]);
                    }
                    else if (flag == "-t")
                    {
                        return !serviceRun.addTargetPath(argv[3]);
                    }
                }
                logger.error("invalid args");
                return 1;
            }
            else if (cmd == "rm")
            {
                if (argc == 4)
                {
                    std::string_view flag{argv[2]};
                    if (flag == "-s")
                    {
                        return !serviceRun.removeSourcePath(argv[3]);
                    }
                    else if (flag == "-t")
                    {
                        return !serviceRun.removeTargetPath(argv[3]);
                    }
                }
                logger.error("invalid args");
                return 1;
            }
            else if (cmd == "restore")
            {
                if (argc == 3)
                {
                    return !serviceRun.restoreFile(argv[2]);
                }
                logger.error("invalid args");
                return 1;
            }
            else
            {
                logger.info("cleaning data from backuprootid: " + std::string(argv[1]));
                serviceRun.deleteByBackuprootid(std::atol(argv[1]));
                return 0;
            }
        }
        serviceRun.XCopy();
    }
    catch (const std::exception& e)
    {
        logger.error(e.what());
    }

#if _DEBUG
    while (std::cin.get() != EOF);
#endif

    return 0;
}
