#include <iostream>
#include <chrono>
#include "service_run.h"
#include "util.h"

int main(int argc, char *argv[])
{
    using namespace timemachine;
    std::cout << "timemachineplus C++ skeleton (C++17)" << std::endl;

    Utils::Log logger;
    try
    {
        ServiceRun serviceRun;
        serviceRun.init();
        serviceRun.loadBackupRoot();

        if (argc == 2)
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
    std::cin.get();
#endif
    return 0;
}
