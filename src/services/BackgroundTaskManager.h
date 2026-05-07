#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct BackgroundTaskSnapshot
    {
        std::string name;
        std::string status;
        std::string progress;
        std::string last_run;
        std::string last_error;
        std::string detail;
        bool can_cancel = false;
        bool can_retry = false;
    };

    class BackgroundTaskManager
    {
    public:
        void Upsert(const BackgroundTaskSnapshot& task);
        std::vector<BackgroundTaskSnapshot> Tasks() const;
        std::vector<InfoItem> Rows() const;

    private:
        std::vector<BackgroundTaskSnapshot> tasks_;
    };
}
