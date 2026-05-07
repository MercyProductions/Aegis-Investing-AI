#include "PaperBroker.h"

#include <algorithm>

namespace aegis
{
    double EstimatePaperCommission(const PaperOrderRequest& request)
    {
        if (request.shares <= 0.0)
            return 0.0;
        return std::max(0.35, request.shares * 0.0035);
    }
}
