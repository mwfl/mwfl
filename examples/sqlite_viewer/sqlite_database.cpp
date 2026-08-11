#include "sqlite_database.h"

#include <windows.h>
#include <winsqlite/winsqlite3.h>

#include <chrono>
#include <fstream>
#include <utility>

namespace sqlite_viewer {
namespace {

std::string ToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0, nullptr,
                                           nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring FromUtf8(const char* text, int length = -1) {
    if (text == nullptr) return L"NULL";
    if (length < 0) length = static_cast<int>(std::char_traits<char>::length(text));
    if (length == 0) return {};
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, nullptr, 0);
    if (size <= 0) return L"�";
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, result.data(), size);
    return result;
}

std::wstring ErrorText(sqlite3* database, int code) {
    const char* message = database != nullptr ? sqlite3_errmsg(database) : sqlite3_errstr(code);
    return FromUtf8(message);
}

std::wstring CellText(sqlite3_stmt* statement, int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) return L"NULL";
    if (sqlite3_column_type(statement, column) == SQLITE_BLOB)
        return L"BLOB (" + std::to_wstring(sqlite3_column_bytes(statement, column)) +
               L" bytes)";
    const auto* value = sqlite3_column_text(statement, column);
    return FromUtf8(reinterpret_cast<const char*>(value), sqlite3_column_bytes(statement, column));
}

std::string CsvCell(std::wstring_view value) {
    std::string utf8 = ToUtf8(value);
    std::string escaped;
    escaped.reserve(utf8.size() + 2);
    escaped.push_back('"');
    for (char character : utf8) {
        if (character == '"') escaped.push_back('"');
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

}  // namespace

Database::~Database() noexcept { Close(); }

Database::Database(Database&& other) noexcept
    : database_(std::exchange(other.database_, nullptr)), path_(std::move(other.path_)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        database_ = std::exchange(other.database_, nullptr);
        path_ = std::move(other.path_);
    }
    return *this;
}

bool Database::OpenReadOnly(const std::filesystem::path& path, std::wstring& error) noexcept {
    Close();
    const std::string utf8_path = ToUtf8(path.wstring());
    sqlite3* opened = nullptr;
    const int result = sqlite3_open_v2(utf8_path.c_str(), &opened,
                                       SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (result != SQLITE_OK) {
        error = ErrorText(opened, result);
        if (opened != nullptr) sqlite3_close_v2(opened);
        return false;
    }
    sqlite3_extended_result_codes(opened, 1);
    sqlite3_busy_timeout(opened, 1500);
    database_ = opened;
    path_ = path;
    error.clear();
    return true;
}

void Database::Close() noexcept {
    if (database_ != nullptr) sqlite3_close_v2(database_);
    database_ = nullptr;
    path_.clear();
}

std::vector<SchemaObject> Database::ReadSchema(std::wstring& error) const {
    std::vector<SchemaObject> objects;
    if (!database_) {
        error = L"No database is open.";
        return objects;
    }
    constexpr char query[] =
        "SELECT type,name,tbl_name,COALESCE(sql,'') FROM sqlite_schema "
        "WHERE name NOT LIKE 'sqlite_%' ORDER BY type,name";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, query, -1, &statement, nullptr) != SQLITE_OK) {
        error = ErrorText(database_, sqlite3_errcode(database_));
        return objects;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
        objects.push_back({CellText(statement, 0), CellText(statement, 1),
                           CellText(statement, 2), CellText(statement, 3)});
    }
    const int result = sqlite3_finalize(statement);
    if (result != SQLITE_OK) {
        error = ErrorText(database_, result);
        objects.clear();
    } else {
        error.clear();
    }
    return objects;
}

std::optional<QueryResult> Database::ExecuteReadOnly(std::wstring_view sql,
                                                     std::size_t row_limit,
                                                     std::wstring& error) const {
    if (!database_) {
        error = L"No database is open.";
        return std::nullopt;
    }
    const std::string utf8_sql = ToUtf8(sql);
    sqlite3_stmt* statement = nullptr;
    const char* tail = nullptr;
    const int prepared = sqlite3_prepare_v2(database_, utf8_sql.c_str(),
                                            static_cast<int>(utf8_sql.size()), &statement, &tail);
    if (prepared != SQLITE_OK || statement == nullptr) {
        error = prepared == SQLITE_OK ? L"Enter a SQL query." : ErrorText(database_, prepared);
        return std::nullopt;
    }
    const auto finalize = [&] { sqlite3_finalize(statement); };
    if (sqlite3_stmt_readonly(statement) == 0) {
        finalize();
        error = L"This viewer only permits read-only SQL statements.";
        return std::nullopt;
    }
    sqlite3_stmt* extra = nullptr;
    while (tail != nullptr && *tail != '\0') {
        const char* next_tail = nullptr;
        const int extra_result = sqlite3_prepare_v2(database_, tail, -1, &extra, &next_tail);
        if (extra_result != SQLITE_OK) {
            finalize();
            error = ErrorText(database_, extra_result);
            return std::nullopt;
        }
        if (extra != nullptr) {
            sqlite3_finalize(extra);
            finalize();
            error = L"Run one SQL statement at a time.";
            return std::nullopt;
        }
        if (next_tail == tail) break;
        tail = next_tail;
    }

    QueryResult output;
    const int columns = sqlite3_column_count(statement);
    for (int column = 0; column < columns; ++column)
        output.columns.push_back(FromUtf8(sqlite3_column_name(statement, column)));
    const auto started = std::chrono::steady_clock::now();
    int step = SQLITE_ROW;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (output.rows.size() >= row_limit) {
            output.truncated = true;
            break;
        }
        auto& row = output.rows.emplace_back();
        row.reserve(static_cast<std::size_t>(columns));
        for (int column = 0; column < columns; ++column)
            row.push_back(CellText(statement, column));
    }
    output.elapsed_ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - started)
                            .count();
    const int finalized = sqlite3_finalize(statement);
    if (step != SQLITE_DONE && !output.truncated) {
        error = ErrorText(database_, step);
        return std::nullopt;
    }
    if (finalized != SQLITE_OK && !output.truncated) {
        error = ErrorText(database_, finalized);
        return std::nullopt;
    }
    error.clear();
    return output;
}

bool ExportCsv(const QueryResult& result, const std::filesystem::path& path,
               std::wstring& error) noexcept {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = L"Could not create the CSV file.";
        return false;
    }
    stream.write("\xEF\xBB\xBF", 3);
    const auto write_row = [&](const auto& values) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index != 0) stream.put(',');
            stream << CsvCell(values[index]);
        }
        stream << "\r\n";
    };
    write_row(result.columns);
    for (const auto& row : result.rows) write_row(row);
    if (!stream) {
        error = L"Writing the CSV file failed.";
        return false;
    }
    error.clear();
    return true;
}

std::wstring QuoteIdentifier(std::wstring_view identifier) {
    std::wstring quoted{L'"'};
    for (wchar_t character : identifier) {
        if (character == L'"') quoted.push_back(L'"');
        quoted.push_back(character);
    }
    quoted.push_back(L'"');
    return quoted;
}

}  // namespace sqlite_viewer
