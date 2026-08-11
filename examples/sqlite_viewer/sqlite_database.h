#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace sqlite_viewer {

struct SchemaObject {
    std::wstring type;
    std::wstring name;
    std::wstring table;
    std::wstring sql;
};

struct QueryResult {
    std::vector<std::wstring> columns;
    std::vector<std::vector<std::wstring>> rows;
    bool truncated = false;
    double elapsed_ms = 0.0;
};

class Database final {
public:
    Database() noexcept = default;
    ~Database() noexcept;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    bool OpenReadOnly(const std::filesystem::path& path, std::wstring& error) noexcept;
    void Close() noexcept;
    bool IsOpen() const noexcept { return database_ != nullptr; }
    const std::filesystem::path& GetPath() const noexcept { return path_; }

    std::vector<SchemaObject> ReadSchema(std::wstring& error) const;
    std::optional<QueryResult> ExecuteReadOnly(std::wstring_view sql,
                                               std::size_t row_limit,
                                               std::wstring& error) const;

private:
    sqlite3* database_ = nullptr;
    std::filesystem::path path_;
};

bool ExportCsv(const QueryResult& result, const std::filesystem::path& path,
               std::wstring& error) noexcept;
std::wstring QuoteIdentifier(std::wstring_view identifier);

}  // namespace sqlite_viewer
