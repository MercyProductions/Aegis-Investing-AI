#include "AnalyticsEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "analytics-engine";
            return item;
        }

        double Mean(const std::vector<double>& values)
        {
            return values.empty() ? 0.0 : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        }

        double StdDev(const std::vector<double>& values, double mean)
        {
            if (values.size() < 2)
                return 0.0;
            double sum = 0.0;
            for (double value : values)
                sum += (value - mean) * (value - mean);
            return std::sqrt(sum / static_cast<double>(values.size() - 1));
        }
    }

    AnalyticsEngineSnapshot BuildAnalyticsEngineSnapshot(const std::vector<Candle>& candles, const std::string& label)
    {
        AnalyticsEngineSnapshot snapshot;
        std::vector<double> returns;
        returns.reserve(candles.size());
        for (size_t i = 1; i < candles.size(); ++i)
        {
            if (candles[i - 1].close > 0.0)
                returns.push_back((candles[i].close - candles[i - 1].close) / candles[i - 1].close);
        }

        const double mean = Mean(returns);
        const double stdev = StdDev(returns, mean);
        std::vector<double> downside;
        for (double value : returns)
        {
            if (value < 0.0)
                downside.push_back(value);
        }
        const double downside_dev = StdDev(downside, Mean(downside));
        snapshot.volatility = stdev * std::sqrt(252.0) * 100.0;
        snapshot.sharpe = stdev > 0.0 ? (mean / stdev) * std::sqrt(252.0) : 0.0;
        snapshot.sortino = downside_dev > 0.0 ? (mean / downside_dev) * std::sqrt(252.0) : 0.0;
        if (candles.size() > 1 && candles.front().close > 0.0)
            snapshot.rolling_return = ((candles.back().close - candles.front().close) / candles.front().close) * 100.0;

        double peak = candles.empty() ? 0.0 : candles.front().close;
        for (const Candle& candle : candles)
        {
            peak = std::max(peak, candle.close);
            if (peak > 0.0)
                snapshot.max_drawdown = std::min(snapshot.max_drawdown, (candle.close - peak) / peak * 100.0);
        }

        snapshot.rows = {
            Row("Analytics sample", label.empty() ? "Selected" : label, std::to_string(static_cast<int>(candles.size())) + " candles", "AnalyticsEngine owns rolling return, volatility, drawdown, and benchmark-ready calculations.", candles.size() > 5 ? "Ready" : "Review"),
            Row("Rolling return", snapshot.rolling_return >= 0.0 ? "Positive" : "Negative", FormatPercent(snapshot.rolling_return), "Return over the loaded candle window."),
            Row("Annualized volatility", snapshot.volatility > 45.0 ? "Elevated" : "Normal", FormatPercent(snapshot.volatility), "Volatility is annualized from close-to-close returns.", snapshot.volatility > 45.0 ? "Review" : "Ready"),
            Row("Sharpe estimate", snapshot.sharpe >= 1.0 ? "Constructive" : "Low/unknown", std::to_string(static_cast<int>(snapshot.sharpe * 100.0) / 100.0), "Risk-adjusted estimate using zero risk-free rate for now."),
            Row("Max drawdown", snapshot.max_drawdown < -15.0 ? "Review" : "Contained", FormatPercent(snapshot.max_drawdown), "Worst peak-to-trough drawdown inside the loaded window.", snapshot.max_drawdown < -15.0 ? "Review" : "Ready")
        };
        return snapshot;
    }

    std::vector<InfoItem> BuildAnalyticsEngineRows(const std::vector<Candle>& candles, const std::string& label)
    {
        return BuildAnalyticsEngineSnapshot(candles, label).rows;
    }
}
