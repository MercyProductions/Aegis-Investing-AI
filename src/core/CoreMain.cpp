#include "CoreService.h"

#include "../persistence/CoreDatabase.h"
#include "../Platform.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv)
{
    bool health = false;
    bool once = false;
    bool daemon = false;
    bool self_test = false;
    bool refresh = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--health")
            health = true;
        else if (arg == "--once")
            once = true;
        else if (arg == "--daemon")
            daemon = true;
        else if (arg == "--self-test")
            self_test = true;
        else if (arg == "--refresh")
            refresh = true;
    }

    if (self_test)
    {
        std::string status;
        const bool ok = aegis::RunCoreDatabaseSelfTest(status);
        std::cout << status << "\n";
        return ok ? 0 : 2;
    }

    if (health)
    {
        const aegis::CoreServiceStatus status = aegis::BuildCoreServiceHealth();
        std::cout << aegis::CoreServiceStatusJson(status) << "\n";
        return status.ok ? 0 : 2;
    }

    if (daemon)
    {
        std::cout << "AuralithCore daemon started. Live execution remains locked.\n";
        for (;;)
        {
            const aegis::CoreServiceStatus status = aegis::RunCoreServiceCycle(refresh);
            std::cout << aegis::CoreServiceStatusJson(status) << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }

    const aegis::CoreServiceStatus status = aegis::RunCoreServiceCycle(refresh || once);
    std::cout << aegis::CoreServiceStatusJson(status) << "\n";
    return status.ok ? 0 : 2;
}
