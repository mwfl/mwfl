#include "../examples/sqlite_viewer/sqlite_database.h"

#include <winsqlite/winsqlite3.h>
#include <wil/resource.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      (L"mwfl-sqlite-test-" + std::to_wstring(::GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto cleanup = wil::scope_exit([&] {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    });
    const auto path = root / L"viewer.db";
    sqlite3* raw = nullptr;
    const std::string narrow = path.string();
    if (sqlite3_open_v2(narrow.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        nullptr) != SQLITE_OK)
        return 1;
    const int setup = sqlite3_exec(
        raw,
        "CREATE TABLE people(id INTEGER PRIMARY KEY,name TEXT);"
        "INSERT INTO people(name) VALUES('Ada'),('Grace');"
        "CREATE INDEX people_name ON people(name);",
        nullptr, nullptr, nullptr);
    sqlite3_close_v2(raw);
    if (setup != SQLITE_OK) return 2;

    sqlite_viewer::Database database;
    std::wstring error;
    if (!database.OpenReadOnly(path, error)) return 3;
    const auto schema = database.ReadSchema(error);
    if (!error.empty() || schema.size() != 2 || schema[0].name.empty()) return 4;
    auto result = database.ExecuteReadOnly(L"SELECT id,name FROM people ORDER BY id", 10, error);
    if (!result || !error.empty() || result->columns.size() != 2 ||
        result->rows.size() != 2 || result->rows[1][1] != L"Grace")
        return 5;
    if (database.ExecuteReadOnly(L"DELETE FROM people", 10, error) || error.empty()) return 6;
    if (database.ExecuteReadOnly(L"SELECT 1; SELECT 2", 10, error) || error.empty()) return 7;
    auto limited = database.ExecuteReadOnly(L"SELECT id FROM people", 1, error);
    if (!limited || !limited->truncated || limited->rows.size() != 1) return 8;
    const auto csv = root / L"result.csv";
    if (!sqlite_viewer::ExportCsv(*result, csv, error) ||
        std::filesystem::file_size(csv) < 10)
        return 9;
    if (sqlite_viewer::QuoteIdentifier(L"a\"b") != L"\"a\"\"b\"") return 10;
    return EXIT_SUCCESS;
}
