#include "BrokerImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace aegis
{
    namespace
    {
        std::string NormalizeHeader(std::string value)
        {
            value = Lower(Trim(value));
            std::string out;
            for (const unsigned char c : value)
            {
                if (std::isalnum(c))
                    out.push_back(static_cast<char>(c));
            }
            return out;
        }

        std::string NormalizeSymbol(std::string value)
        {
            value = Upper(Trim(value));
            const size_t space = value.find(' ');
            if (space != std::string::npos)
                value = value.substr(0, space);
            value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
                return std::isalnum(c) == 0 && c != '.' && c != '-';
            }), value.end());
            return value;
        }

        std::vector<std::string> SplitCsvLine(const std::string& line)
        {
            std::vector<std::string> cells;
            std::string cell;
            bool quoted = false;
            for (size_t i = 0; i < line.size(); ++i)
            {
                const char c = line[i];
                if (c == '"')
                {
                    if (quoted && i + 1 < line.size() && line[i + 1] == '"')
                    {
                        cell.push_back('"');
                        ++i;
                    }
                    else
                    {
                        quoted = !quoted;
                    }
                }
                else if (c == ',' && !quoted)
                {
                    cells.push_back(cell);
                    cell.clear();
                }
                else
                {
                    cell.push_back(c);
                }
            }
            cells.push_back(cell);
            return cells;
        }

        double ParseMoney(std::string value)
        {
            value = Trim(value);
            bool negative = false;
            if (value.size() > 2 && value.front() == '(' && value.back() == ')')
            {
                negative = true;
                value = value.substr(1, value.size() - 2);
            }
            value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
                return c == '$' || c == ',' || c == '%' || std::isspace(c);
            }), value.end());
            if (value.empty() || value == "--" || value == "N/A")
                return 0.0;
            try
            {
                const double parsed = std::stod(value);
                return negative ? -parsed : parsed;
            }
            catch (...)
            {
                return 0.0;
            }
        }

        int FindHeader(const std::map<std::string, int>& columns, const std::vector<std::string>& aliases)
        {
            for (const std::string& alias : aliases)
            {
                const auto found = columns.find(NormalizeHeader(alias));
                if (found != columns.end())
                    return found->second;
            }
            return -1;
        }

        std::string Cell(const std::vector<std::string>& cells, int index)
        {
            if (index < 0 || index >= static_cast<int>(cells.size()))
                return {};
            return Trim(cells[static_cast<size_t>(index)]);
        }

        bool LooksUnsupportedType(const std::string& value)
        {
            const std::string lowered = Lower(value);
            return lowered.find("option") != std::string::npos
                || lowered.find("crypto") != std::string::npos
                || lowered.find("cash") != std::string::npos
                || lowered.find("future") != std::string::npos;
        }

        std::vector<BrokerImportRow> PreviewProfile(const std::filesystem::path& directory, const BrokerImportProfile& profile)
        {
            std::vector<BrokerImportRow> rows;
            const std::filesystem::path path = directory / profile.file_name;
            std::ifstream file(path);
            if (!file)
                return rows;

            std::string line;
            if (!std::getline(file, line))
                return rows;

            const std::vector<std::string> headers = SplitCsvLine(line);
            std::map<std::string, int> columns;
            for (int i = 0; i < static_cast<int>(headers.size()); ++i)
                columns[NormalizeHeader(headers[static_cast<size_t>(i)])] = i;

            const int symbol_col = FindHeader(columns, profile.symbol_headers);
            const int shares_col = FindHeader(columns, profile.shares_headers);
            const int avg_cost_col = FindHeader(columns, profile.average_cost_headers);
            const int total_cost_col = FindHeader(columns, profile.total_cost_headers);
            const int account_col = FindHeader(columns, profile.account_headers);
            const int type_col = FindHeader(columns, profile.type_headers);

            int row_number = 1;
            while (std::getline(file, line))
            {
                ++row_number;
                if (Trim(line).empty())
                    continue;

                const std::vector<std::string> cells = SplitCsvLine(line);
                BrokerImportRow row;
                row.profile = profile.name;
                row.source_file = profile.file_name;
                row.row_number = row_number;
                row.symbol = NormalizeSymbol(Cell(cells, symbol_col));
                row.shares = ParseMoney(Cell(cells, shares_col));
                row.average_cost = ParseMoney(Cell(cells, avg_cost_col));
                const double total_cost = ParseMoney(Cell(cells, total_cost_col));
                row.account = Cell(cells, account_col);
                const std::string type = Cell(cells, type_col);
                if (row.average_cost <= 0.0 && total_cost > 0.0 && row.shares > 0.0)
                    row.average_cost = total_cost / row.shares;

                if (symbol_col < 0 || shares_col < 0 || (avg_cost_col < 0 && total_cost_col < 0))
                {
                    row.status = "Header error";
                    row.error = "Required Symbol, Quantity, and Cost headers were not found.";
                }
                else if (LooksUnsupportedType(type))
                {
                    row.status = "Skipped";
                    row.error = "Unsupported asset type: " + type;
                }
                else if (row.symbol.empty())
                {
                    row.status = "Error";
                    row.error = "Missing symbol.";
                }
                else if (row.shares <= 0.0)
                {
                    row.status = "Error";
                    row.error = "Shares must be positive.";
                }
                else if (row.average_cost < 0.0)
                {
                    row.status = "Error";
                    row.error = "Average cost cannot be negative.";
                }
                else
                {
                    row.valid = true;
                    row.status = "Ready";
                    row.note = profile.name;
                    if (!row.account.empty())
                        row.note += " account " + row.account;
                }

                rows.push_back(row);
            }
            return rows;
        }
    }

    std::vector<BrokerImportProfile> BrokerImportProfiles()
    {
        const std::vector<std::string> symbol = { "symbol", "ticker", "instrument", "underlying symbol", "security symbol" };
        const std::vector<std::string> shares = { "shares", "quantity", "qty", "position", "current quantity", "quantity held" };
        const std::vector<std::string> avg = { "average cost", "avg cost", "average price", "avg price", "cost per share", "cost/share", "average cost per share", "cost basis per share" };
        const std::vector<std::string> total = { "cost basis", "total cost", "total cost basis", "cost basis total", "cost", "book cost" };
        const std::vector<std::string> account = { "account", "account name", "account number", "portfolio" };
        const std::vector<std::string> type = { "type", "asset type", "security type", "instrument type" };

        return {
            { "Robinhood", "robinhood-import.csv", symbol, shares, avg, total, account, type },
            { "Fidelity", "fidelity-import.csv", symbol, shares, avg, total, account, type },
            { "Schwab", "schwab-import.csv", symbol, shares, avg, total, account, type },
            { "Webull", "webull-import.csv", symbol, shares, avg, total, account, type },
            { "IBKR", "ibkr-import.csv", symbol, shares, avg, total, account, type },
            { "Generic Broker", "broker-import.csv", symbol, shares, avg, total, account, type }
        };
    }

    std::vector<BrokerImportRow> PreviewBrokerImport(const std::filesystem::path& directory)
    {
        std::vector<BrokerImportRow> rows;
        for (const BrokerImportProfile& profile : BrokerImportProfiles())
        {
            std::vector<BrokerImportRow> profile_rows = PreviewProfile(directory, profile);
            rows.insert(rows.end(), profile_rows.begin(), profile_rows.end());
        }
        return rows;
    }

    int CountValidBrokerRows(const std::vector<BrokerImportRow>& rows)
    {
        return static_cast<int>(std::count_if(rows.begin(), rows.end(), [](const BrokerImportRow& row) {
            return row.valid;
        }));
    }

    std::string BrokerImportInstructions()
    {
        return "Place robinhood-import.csv, fidelity-import.csv, schwab-import.csv, webull-import.csv, ibkr-import.csv, or broker-import.csv in the app data folder. Aegis maps symbol/ticker, shares/quantity/position, average cost, or total cost basis before importing valid equity rows.";
    }
}
