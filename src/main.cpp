#include <chrono>
#include <iostream>

#include "service_run.h"
#include "util.h"

int main(int argc, char* argv[])
{
    using namespace timemachine;
    std::cout << "timemachineplus C++ skeleton (C++17)" << std::endl;

    Utils::Log logger;
    try
    {
        ServiceRun serviceRun;
        serviceRun.init();
        serviceRun.loadBackupRoot();

        if (argc >= 2)
        {
            if (std::string(argv[1]) == "checkdata")
            {
                logger.info("begin to checkdata");
                serviceRun.checkdata(false);
            }
            else if (std::string(argv[1]) == "checkdatawithhash")
            {
                logger.info("begin to checkdata with hash");
                serviceRun.checkdata(true);
            }
            else if (std::string(argv[1]) == "add")
            {
                if (argc == 4)
                {
                    if (std::string(argv[2]) == "-s")
                    {
                        return !serviceRun.addSourcePath(argv[3]);
                    }
                    else if (std::string(argv[2]) == "-t")
                    {
                        return !serviceRun.addTargetPath(argv[3]);
                    }
                }
                logger.error("invalid args");
                return 1;
            }
            else if (std::string(argv[1]) == "rm")
            {
                if (argc == 4)
                {
                    if (std::string(argv[2]) == "-s")
                    {
                        return !serviceRun.removeSourcePath(argv[3]);
                    }
                    else if (std::string(argv[2]) == "-t")
                    {
                        return !serviceRun.removeTargetPath(argv[3]);
                    }
                }
                logger.error("invalid args");
                return 1;
            }
            else if (std::string(argv[1]) == "restore")
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
                logger.info("cleaning data from backuprootid: " +
                            std::string(argv[1]));
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
    while (std::cin.get() != EOF)
        ;
#endif
    return 0;
}
