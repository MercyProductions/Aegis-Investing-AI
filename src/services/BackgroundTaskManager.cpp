#include "BackgroundTaskManager.h"

#include <algorithm>

namespace aegis
{
    void BackgroundTaskManager::Upsert(const BackgroundTaskSnapshot& task)
    {
        const auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const BackgroundTaskSnapshot& existing) {
            return existing.name == task.name;
        });

        if (it == tasks_.end())
            tasks_.push_back(task);
        else
            *it = task;
    }

    std::vector<BackgroundTaskSnapshot> BackgroundTaskManager::Tasks() const
    {
        return tasks_;
    }

    std::vector<InfoItem> BackgroundTaskManager::Rows() const
    {
        std::vector<InfoItem> rows;
        rows.reserve(tasks_.size());
        for (const BackgroundTaskSnapshot& task : tasks_)
        {
            InfoItem item;
            item.name = task.name;
            item.label = task.status.empty() ? "Idle" : task.status;
            item.value = task.progress.empty() ? "--" : task.progress;
            item.state = task.status;
            item.tag = "background";
            item.time = task.last_run;

            std::string detail = task.detail;
            if (!task.last_run.empty())
                detail += (detail.empty() ? "" : " ") + std::string("Last run: ") + task.last_run + ".";
            if (!task.last_error.empty())
                detail += (detail.empty() ? "" : " ") + std::string("Last error: ") + task.last_error + ".";
            if (task.can_cancel || task.can_retry)
                detail += (detail.empty() ? "" : " ") + std::string("Controls: ") + (task.can_cancel ? "cancel" : "") + (task.can_cancel && task.can_retry ? " / " : "") + (task.can_retry ? "retry" : "") + ".";
            item.detail = detail.empty() ? "No task detail reported yet." : detail;
            rows.push_back(item);
        }
        return rows;
    }
}
