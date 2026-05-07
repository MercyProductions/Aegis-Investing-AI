#include "CoreDatabase.h"

#include "../audit/AuditManager.h"
#include "../Platform.h"

#include <winsqlite/winsqlite3.h>

#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace aegis
{
    namespace
    {
        constexpr int kSchemaVersion = 1;

        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "core-db";
            return item;
        }

        std::string SqliteError(sqlite3* db)
        {
            return db != nullptr ? sqlite3_errmsg(db) : "sqlite database did not open";
        }

        bool Exec(sqlite3* db, const std::string& sql, std::string& error)
        {
            char* raw_error = nullptr;
            const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &raw_error);
            if (rc != SQLITE_OK)
            {
                error = raw_error != nullptr ? raw_error : SqliteError(db);
                sqlite3_free(raw_error);
                return false;
            }
            return true;
        }

        int QueryInt(sqlite3* db, const std::string& sql)
        {
            sqlite3_stmt* stmt = nullptr;
            int value = 0;
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
            {
                if (sqlite3_step(stmt) == SQLITE_ROW)
                    value = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
            return value;
        }

        int TableCount(sqlite3* db, const std::string& table)
        {
            return QueryInt(db, "select count(*) from " + table + ";");
        }

        std::string CurrentTimestamp()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
            localtime_s(&local, &tt);
            std::ostringstream stream;
            stream << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
            return stream.str();
        }

        void BindText(sqlite3_stmt* stmt, int index, const std::string& value)
        {
            sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
        }

        bool Prepare(sqlite3* db, const char* sql, sqlite3_stmt** stmt, std::string& error)
        {
            if (sqlite3_prepare_v2(db, sql, -1, stmt, nullptr) != SQLITE_OK)
            {
                error = SqliteError(db);
                return false;
            }
            return true;
        }

        bool StepAndReset(sqlite3* db, sqlite3_stmt* stmt, std::string& error)
        {
            const int rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
            {
                error = SqliteError(db);
                sqlite3_reset(stmt);
                sqlite3_clear_bindings(stmt);
                return false;
            }
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            return true;
        }

        std::string FreshnessForQuote(const StockQuote& quote)
        {
            if (!quote.data_quality.empty())
                return quote.data_quality;
            if (!quote.status.empty())
                return quote.status;
            return quote.live ? "fresh" : "fallback";
        }

        bool LooksLikeRiskEvent(const InfoItem& item)
        {
            const std::string text = Lower(item.tag + " " + item.name + " " + item.label + " " + item.state);
            return item.tag == "resilience" || text.find("risk") != std::string::npos || item.state == "Review";
        }
    }

    std::filesystem::path AuralithCoreDatabasePath()
    {
        return AppDataDirectory() / "data" / "auralith-core.sqlite";
    }

    std::string AuralithCoreSchemaSql()
    {
        return R"SQL(
pragma journal_mode = wal;
pragma foreign_keys = on;

create table if not exists schema_migrations (
    version integer primary key,
    name text not null,
    applied_at text not null default current_timestamp
);

create table if not exists symbols (
    symbol text primary key,
    asset_class text not null default 'Equity',
    name text not null default '',
    sector text not null default '',
    venue text not null default '',
    currency text not null default 'USD',
    active integer not null default 1,
    updated_at text not null default current_timestamp
);

create table if not exists quotes (
    id integer primary key autoincrement,
    symbol text not null,
    price real not null,
    change_percent real not null default 0,
    volume real not null default 0,
    provider text not null,
    fetched_at text not null,
    freshness text not null,
    fallback integer not null default 0,
    error_state text not null default '',
    unique(symbol, provider, fetched_at)
);

create table if not exists candles (
    symbol text not null,
    timeframe text not null,
    time text not null,
    open real not null,
    high real not null,
    low real not null,
    close real not null,
    volume real not null default 0,
    provider text not null,
    fetched_at text not null,
    freshness text not null,
    primary key(symbol, timeframe, time)
);

create table if not exists indicators (
    symbol text not null,
    timeframe text not null,
    calculated_at text not null,
    name text not null,
    value real not null,
    metadata_json text not null default '{}',
    primary key(symbol, timeframe, calculated_at, name)
);

create table if not exists alerts (
    id text primary key,
    symbol text not null,
    direction text not null,
    trigger_price real not null,
    enabled integer not null default 1,
    note text not null default '',
    created_at text not null default current_timestamp,
    updated_at text not null default current_timestamp
);

create table if not exists portfolios (
    id text primary key,
    name text not null,
    profile_id text not null default 'default',
    cash real not null default 0,
    created_at text not null default current_timestamp,
    updated_at text not null default current_timestamp
);

create table if not exists positions (
    portfolio_id text not null,
    symbol text not null,
    shares real not null,
    average_cost real not null,
    note text not null default '',
    updated_at text not null default current_timestamp,
    primary key(portfolio_id, symbol)
);

create table if not exists orders (
    id text primary key,
    portfolio_id text not null,
    symbol text not null,
    side text not null,
    quantity real not null,
    state text not null,
    mode text not null default 'paper',
    average_fill_price real not null default 0,
    notional real not null default 0,
    reason text not null default '',
    created_at text not null default current_timestamp,
    updated_at text not null default current_timestamp
);

create table if not exists journal (
    id integer primary key autoincrement,
    symbol text not null default '',
    action text not null,
    reason text not null default '',
    mistakes text not null default '',
    grade text not null default '',
    realized_pnl real not null default 0,
    created_at text not null default current_timestamp
);

create table if not exists reports (
    id text primary key,
    report_type text not null,
    title text not null,
    path text not null,
    generated_at text not null default current_timestamp,
    source_json text not null default '{}'
);

create table if not exists risk_events (
    id integer primary key autoincrement,
    symbol text not null default '',
    severity text not null,
    risk_state text not null,
    detail text not null,
    created_at text not null default current_timestamp
);

create table if not exists provider_health (
    provider text primary key,
    status text not null,
    health_score integer not null default 0,
    last_success_at text not null default '',
    last_error_at text not null default '',
    detail text not null default '',
    updated_at text not null default current_timestamp
);

create table if not exists workspace_layouts (
    id text primary key,
    profile_id text not null,
    name text not null,
    layout_json text not null,
    updated_at text not null default current_timestamp
);

create table if not exists ai_memory (
    id integer primary key autoincrement,
    scope text not null,
    symbol text not null default '',
    memory_type text not null,
    summary text not null,
    evidence_json text not null default '{}',
    confidence real not null default 0.5,
    created_at text not null default current_timestamp,
    updated_at text not null default current_timestamp
);

create table if not exists audit_events (
    id integer primary key autoincrement,
    actor text not null default 'local',
    action text not null,
    symbol text not null default '',
    detail text not null default '',
    hash text not null default '',
    created_at text not null default current_timestamp
);

create table if not exists scanner_history (
    id integer primary key autoincrement,
    symbol text not null,
    score integer not null,
    confidence integer not null,
    rating text not null,
    thesis text not null default '',
    source text not null default '',
    created_at text not null default current_timestamp
);

create table if not exists backtest_runs (
    id text primary key,
    strategy_name text not null,
    symbol text not null default '',
    started_at text not null default current_timestamp,
    completed_at text not null default '',
    trades integer not null default 0,
    win_rate real not null default 0,
    sharpe real not null default 0,
    sortino real not null default 0,
    max_drawdown real not null default 0,
    result_json text not null default '{}'
);

create index if not exists idx_quotes_symbol_time on quotes(symbol, fetched_at);
create index if not exists idx_candles_symbol_time on candles(symbol, timeframe, time);
create index if not exists idx_ai_memory_scope_symbol on ai_memory(scope, symbol, memory_type);
create index if not exists idx_audit_symbol_time on audit_events(symbol, created_at);
create index if not exists idx_scanner_symbol_time on scanner_history(symbol, created_at);
insert or ignore into schema_migrations(version, name) values (1, '001_intelligence_network_core');
)SQL";
    }

    CoreDatabaseResult InitializeAuralithCoreDatabase(const std::filesystem::path& path)
    {
        CoreDatabaseResult result;
        result.path = path.empty() ? AuralithCoreDatabasePath() : path;

        std::error_code ec;
        std::filesystem::create_directories(result.path.parent_path(), ec);
        if (ec)
        {
            result.error = ec.message();
            result.status = "Could not create database directory.";
            result.rows.push_back(Row("Core database", "Error", result.path.string(), result.error, "Review"));
            return result;
        }

        sqlite3* db = nullptr;
        if (sqlite3_open16(result.path.wstring().c_str(), &db) != SQLITE_OK)
        {
            result.error = SqliteError(db);
            result.status = "Could not open auralith-core.sqlite.";
            result.rows.push_back(Row("Core database", "Error", result.path.string(), result.error, "Review"));
            sqlite3_close(db);
            return result;
        }

        std::string error;
        result.ok = Exec(db, AuralithCoreSchemaSql(), error);
        if (!result.ok)
        {
            result.error = error;
            result.status = "Schema migration failed.";
        }
        else
        {
            result.schema_version = QueryInt(db, "select max(version) from schema_migrations;");
            result.status = "Core database ready.";
        }

        const std::array<std::string, 18> tables = {
            "schema_migrations",
            "symbols", "quotes", "candles", "indicators", "alerts",
            "portfolios", "positions", "orders", "journal", "reports",
            "risk_events", "provider_health", "workspace_layouts", "ai_memory", "audit_events",
            "scanner_history", "backtest_runs"
        };
        result.rows.push_back(Row("Core database", result.ok ? "Ready" : "Error", result.path.string(), result.ok ? "WAL mode, schema migrations, and indexed Intelligence Network tables are initialized." : result.error, result.ok ? "Ready" : "Review"));
        result.rows.push_back(Row("Schema version", "Applied", std::to_string(result.schema_version), "Schema migrations are tracked in schema_migrations."));
        for (const std::string& table : tables)
            result.rows.push_back(Row(table, "Table", std::to_string(TableCount(db, table)), "Indexed local-first persistence table."));
        sqlite3_close(db);
        return result;
    }

    bool PersistCoreServiceSnapshot(const StockState& state, const std::vector<InfoItem>& rows, const std::filesystem::path& path, std::string* status)
    {
        const CoreDatabaseResult initialized = InitializeAuralithCoreDatabase(path);
        if (!initialized.ok)
        {
            if (status != nullptr)
                *status = initialized.error.empty() ? initialized.status : initialized.error;
            return false;
        }

        sqlite3* db = nullptr;
        if (sqlite3_open16(initialized.path.wstring().c_str(), &db) != SQLITE_OK)
        {
            if (status != nullptr)
                *status = SqliteError(db);
            sqlite3_close(db);
            return false;
        }

        std::string error;
        const auto fail = [&](const std::string& message) {
            std::string rollback_error;
            Exec(db, "rollback;", rollback_error);
            if (status != nullptr)
                *status = message;
            sqlite3_close(db);
            return false;
        };

        if (!Exec(db, "begin immediate transaction;", error))
            return fail(error);

        const std::string timestamp = CurrentTimestamp();
        sqlite3_stmt* symbol_stmt = nullptr;
        sqlite3_stmt* quote_stmt = nullptr;
        sqlite3_stmt* provider_stmt = nullptr;
        sqlite3_stmt* scanner_stmt = nullptr;
        sqlite3_stmt* memory_stmt = nullptr;
        sqlite3_stmt* risk_stmt = nullptr;
        sqlite3_stmt* audit_stmt = nullptr;

        if (!Prepare(db,
            "insert into symbols(symbol, asset_class, name, sector, updated_at) values (?, ?, ?, ?, ?) "
            "on conflict(symbol) do update set asset_class=excluded.asset_class, name=excluded.name, sector=excluded.sector, updated_at=excluded.updated_at;",
            &symbol_stmt, error))
            return fail(error);

        if (!Prepare(db,
            "insert or ignore into quotes(symbol, price, change_percent, volume, provider, fetched_at, freshness, fallback, error_state) values (?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &quote_stmt, error))
            return fail(error);

        if (!Prepare(db,
            "insert into provider_health(provider, status, health_score, last_success_at, last_error_at, detail, updated_at) values (?, ?, ?, ?, ?, ?, ?) "
            "on conflict(provider) do update set status=excluded.status, health_score=excluded.health_score, last_success_at=excluded.last_success_at, last_error_at=excluded.last_error_at, detail=excluded.detail, updated_at=excluded.updated_at;",
            &provider_stmt, error))
            return fail(error);

        if (!Prepare(db,
            "insert into scanner_history(symbol, score, confidence, rating, thesis, source, created_at) values (?, ?, ?, ?, ?, ?, ?);",
            &scanner_stmt, error))
            return fail(error);

        if (!Prepare(db,
            "insert into ai_memory(scope, symbol, memory_type, summary, evidence_json, confidence, created_at, updated_at) values (?, ?, ?, ?, ?, ?, ?, ?);",
            &memory_stmt, error))
            return fail(error);

        if (!Prepare(db,
            "insert into risk_events(symbol, severity, risk_state, detail, created_at) values (?, ?, ?, ?, ?);",
            &risk_stmt, error))
            return fail(error);

        if (!Prepare(db,
            "insert into audit_events(actor, action, symbol, detail, hash, created_at) values (?, ?, ?, ?, ?, ?);",
            &audit_stmt, error))
            return fail(error);

        for (const StockQuote& quote : state.quotes)
        {
            const std::string provider = quote.source.empty() ? state.source_badge : quote.source;
            const std::string fetched_at = quote.timestamp.empty() ? timestamp : quote.timestamp;
            const std::string freshness = FreshnessForQuote(quote);
            BindText(symbol_stmt, 1, quote.symbol);
            BindText(symbol_stmt, 2, "Equity");
            BindText(symbol_stmt, 3, quote.name);
            BindText(symbol_stmt, 4, quote.sector);
            BindText(symbol_stmt, 5, timestamp);
            if (!StepAndReset(db, symbol_stmt, error))
                return fail(error);

            BindText(quote_stmt, 1, quote.symbol);
            sqlite3_bind_double(quote_stmt, 2, quote.price);
            sqlite3_bind_double(quote_stmt, 3, quote.change_percent);
            sqlite3_bind_double(quote_stmt, 4, static_cast<double>(quote.volume));
            BindText(quote_stmt, 5, provider);
            BindText(quote_stmt, 6, fetched_at);
            BindText(quote_stmt, 7, freshness);
            sqlite3_bind_int(quote_stmt, 8, quote.live ? 0 : 1);
            BindText(quote_stmt, 9, quote.note.empty() ? quote.status : quote.note);
            if (!StepAndReset(db, quote_stmt, error))
                return fail(error);
        }

        const std::string provider_status = state.loaded_from_api ? "healthy" : "fallback";
        BindText(provider_stmt, 1, state.source_badge.empty() ? "Auralith provider pipeline" : state.source_badge);
        BindText(provider_stmt, 2, provider_status);
        sqlite3_bind_int(provider_stmt, 3, state.loaded_from_api ? 90 : 45);
        BindText(provider_stmt, 4, state.loaded_from_api ? timestamp : "");
        BindText(provider_stmt, 5, state.loaded_from_api ? "" : timestamp);
        BindText(provider_stmt, 6, state.source_label + " " + state.market_detail);
        BindText(provider_stmt, 7, timestamp);
        if (!StepAndReset(db, provider_stmt, error))
            return fail(error);

        for (const StockSignal& signal : state.signals)
        {
            BindText(scanner_stmt, 1, signal.symbol);
            sqlite3_bind_int(scanner_stmt, 2, signal.score);
            sqlite3_bind_int(scanner_stmt, 3, signal.confidence);
            BindText(scanner_stmt, 4, signal.rating);
            BindText(scanner_stmt, 5, signal.thesis);
            BindText(scanner_stmt, 6, signal.source);
            BindText(scanner_stmt, 7, timestamp);
            if (!StepAndReset(db, scanner_stmt, error))
                return fail(error);
        }

        int memory_rows = 0;
        int risk_rows = 0;
        for (const InfoItem& item : rows)
        {
            if (item.tag == "ai-memory")
            {
                BindText(memory_stmt, 1, item.name.find("Portfolio") != std::string::npos ? "portfolio" : item.name.find("Session") != std::string::npos ? "session" : "symbol");
                BindText(memory_stmt, 2, item.name.find("Portfolio") != std::string::npos || item.name.find("Session") != std::string::npos ? "" : state.selected_symbol);
                BindText(memory_stmt, 3, Lower(item.name));
                BindText(memory_stmt, 4, item.label + ": " + item.value + ". " + item.detail);
                BindText(memory_stmt, 5, "{\"source\":\"core-cycle\"}");
                sqlite3_bind_double(memory_stmt, 6, item.state == "Ready" ? 0.75 : 0.55);
                BindText(memory_stmt, 7, timestamp);
                BindText(memory_stmt, 8, timestamp);
                if (!StepAndReset(db, memory_stmt, error))
                    return fail(error);
                ++memory_rows;
            }

            if (LooksLikeRiskEvent(item))
            {
                BindText(risk_stmt, 1, state.selected_symbol);
                BindText(risk_stmt, 2, item.state == "Review" ? "warning" : "info");
                BindText(risk_stmt, 3, item.label);
                BindText(risk_stmt, 4, item.name + ": " + item.value + ". " + item.detail);
                BindText(risk_stmt, 5, timestamp);
                if (!StepAndReset(db, risk_stmt, error))
                    return fail(error);
                ++risk_rows;
            }
        }

        const std::string audit_detail = "Core snapshot persisted: quotes=" + std::to_string(state.quotes.size()) +
            ", signals=" + std::to_string(state.signals.size()) +
            ", ai_memory=" + std::to_string(memory_rows) +
            ", risk_events=" + std::to_string(risk_rows) + ".";
        BindText(audit_stmt, 1, "AuralithCore");
        BindText(audit_stmt, 2, "core.snapshot_persisted");
        BindText(audit_stmt, 3, state.selected_symbol);
        BindText(audit_stmt, 4, audit_detail);
        BindText(audit_stmt, 5, AuditFingerprint("AuralithCore" + audit_detail + timestamp));
        BindText(audit_stmt, 6, timestamp);
        if (!StepAndReset(db, audit_stmt, error))
            return fail(error);

        sqlite3_finalize(symbol_stmt);
        sqlite3_finalize(quote_stmt);
        sqlite3_finalize(provider_stmt);
        sqlite3_finalize(scanner_stmt);
        sqlite3_finalize(memory_stmt);
        sqlite3_finalize(risk_stmt);
        sqlite3_finalize(audit_stmt);

        if (!Exec(db, "commit;", error))
        {
            sqlite3_close(db);
            if (status != nullptr)
                *status = error;
            return false;
        }

        sqlite3_close(db);
        if (status != nullptr)
            *status = audit_detail;
        return true;
    }

    std::vector<InfoItem> BuildCoreDatabaseRows(const std::filesystem::path& path)
    {
        return InitializeAuralithCoreDatabase(path).rows;
    }

    bool RunCoreDatabaseSelfTest(std::string& status)
    {
        const std::filesystem::path path = AppDataDirectory() / "self-test" / "auralith-core-self-test.sqlite";
        std::error_code ec;
        std::filesystem::remove(path, ec);
        const CoreDatabaseResult result = InitializeAuralithCoreDatabase(path);
        if (!result.ok || result.schema_version != kSchemaVersion)
        {
            status = result.error.empty() ? result.status : result.error;
            return false;
        }
        status = "Core database self-test passed at " + path.string();
        return true;
    }
}
