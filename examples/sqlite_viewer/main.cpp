#include <mwfl/mwfl.h>

#include "sqlite_database.h"

#include <winsqlite/winsqlite3.h>

#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

using mwfl::operator""_dip;

namespace {

constexpr mwfl::ControlId kOpen{1700};
constexpr mwfl::ControlId kRun{1701};
constexpr mwfl::ControlId kExport{1702};
constexpr mwfl::ControlId kSchema{1703};
constexpr mwfl::ControlId kQuery{1704};
constexpr mwfl::ControlId kResults{1705};
constexpr mwfl::ControlId kStatus{1706};
constexpr UINT kSelfTest = WM_APP + 0x210;
constexpr wchar_t kSettingsKey[] = L"Software\\mwfl\\Examples\\SQLiteViewer";
bool g_self_test = false;
bool g_showcase = false;

std::filesystem::path MakeSelfTestDatabase() {
    const auto path = std::filesystem::temp_directory_path() /
                      (L"mwfl-sqlite-viewer-" + std::to_wstring(::GetCurrentProcessId()) +
                       L".db");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    sqlite3* database = nullptr;
    const std::string utf8 = path.string();
    if (sqlite3_open_v2(utf8.c_str(), &database,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        throw std::runtime_error("create SQLite self-test database");
    const char* sql =
        "CREATE TABLE projects(id INTEGER PRIMARY KEY,name TEXT NOT NULL,stars INTEGER);"
        "INSERT INTO projects(name,stars) VALUES('mwfl',45),('GrapeCompare',3);"
        "CREATE VIEW popular AS SELECT name,stars FROM projects WHERE stars>=10;";
    const int result = sqlite3_exec(database, sql, nullptr, nullptr, nullptr);
    sqlite3_close_v2(database);
    if (result != SQLITE_OK) throw std::runtime_error("populate SQLite self-test database");
    return path;
}

class SQLiteViewerWindow final : public mwfl::WindowBase {
public:
    ~SQLiteViewerWindow() noexcept override {
        if (!self_test_path_.empty()) {
            std::error_code ignored;
            std::filesystem::remove(self_test_path_, ignored);
        }
    }

    void BuildUI() override {
        SetTitle(L"MWFL SQLite Viewer — read-only database workspace");
        BuildCommands();
        BuildMenu();

        mwfl::ControlHost ui{*this};
        ui.Add(open_, kOpen, L"Open database…");
        ui.Add(run_, kRun, L"Run query  F5");
        ui.Add(export_, kExport, L"Export CSV…");
        ui.Add(schema_, kSchema);
        mwfl::TextBoxOptions query_options;
        query_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                               WS_VSCROLL | WS_HSCROLL | ES_NOHIDESEL;
        ui.Add(query_, kQuery, L"SELECT name, type FROM sqlite_schema ORDER BY type, name;",
               query_options);
        ui.Add(results_, kResults);
        ui.Add(status_, kStatus, L"Open a SQLite database or drop one onto this window.");

        results_.SetExtendedListStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                      LVS_EX_HEADERDRAGDROP | LVS_EX_GRIDLINES);
        mwfl::EnableFileDrop(GetHwnd());
        mwfl::Must(mwfl::SetAccessibleName(schema_.GetHwnd(), L"Database schema"),
                   "name SQLite schema tree");
        mwfl::Must(mwfl::SetAccessibleName(query_.GetHwnd(), L"Read-only SQL query"),
                   "name SQLite query editor");
        mwfl::Must(mwfl::SetAccessibleName(results_.GetHwnd(), L"Query results"),
                   "name SQLite results");

        SetLayout(mwfl::Column()
                      .Margin(8.0_dip)
                      .Gap(6.0_dip)
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(open_, mwfl::Auto())
                               .Add(run_, mwfl::Auto())
                               .Add(export_, mwfl::Auto()),
                           mwfl::Auto())
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(schema_, mwfl::Fixed(260.0_dip))
                               .Add(mwfl::Column()
                                        .Gap(6.0_dip)
                                        .Add(query_, mwfl::Fixed(118.0_dip),
                                             {.native_size = mwfl::SizeDip{0.0_dip, 118.0_dip}})
                                        .Add(results_, mwfl::Stretch()),
                                    mwfl::Stretch()),
                           mwfl::Stretch())
                      .Add(status_, mwfl::Auto()));
        mwfl::ApplyWindowAppearance(GetHwnd());

        if (!g_self_test && !g_showcase) {
            mwfl::SavedWindowPlacement saved;
            if (mwfl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kSettingsKey,
                                                      L"WindowPlacement", saved))
                mwfl::RestoreWindowPlacement(GetHwnd(), saved);
        }
        if (g_self_test || g_showcase) {
            self_test_path_ = MakeSelfTestDatabase();
            OpenPath(self_test_path_);
            if (g_self_test && !::PostMessageW(GetHwnd(), kSelfTest, 0, 0))
                throw std::runtime_error("post SQLite self-test");
        }
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(open_)) {
            OpenInteractive();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(run_)) {
            RunQuery();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(export_)) {
            ExportInteractive();
            return mwfl::EventResult::Handled();
        }
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.IsFrom(schema_)) {
            if (const auto notification = schema_.DecodeNotification(event.header);
                notification &&
                notification->kind == mwfl::TreeViewNotificationKind::selection_changed) {
                SelectSchemaObject(notification->item);
                return mwfl::EventResult::Handled();
            }
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == WM_DROPFILES) {
            const auto files = mwfl::ReadDroppedFiles(reinterpret_cast<HDROP>(event.wparam));
            if (!files.empty()) OpenPath(files.front());
            return mwfl::EventResult::Handled();
        }
        if (event.id == WM_THEMECHANGED || event.id == WM_SETTINGCHANGE) {
            mwfl::ApplyWindowAppearance(GetHwnd());
            return mwfl::EventResult::Handled();
        }
        if (event.id == kSelfTest) {
            RunSelfTest();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnClose() override {
        if (!g_self_test && !g_showcase) {
            mwfl::SavedWindowPlacement saved;
            if (mwfl::CaptureWindowPlacement(GetHwnd(), saved))
                static_cast<void>(mwfl::SaveWindowPlacementToRegistry(
                    HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", saved));
        }
        return mwfl::EventResult::Propagate();
    }

private:
    void BuildCommands() {
        commands_.Add(mwfl::Command{kOpen, L"Open database…", [this] { OpenInteractive(); }}
                          .SetShortcut({FVIRTKEY | FCONTROL, 'O'}));
        commands_.Add(mwfl::Command{kRun, L"Run query", [this] { RunQuery(); }}
                          .SetShortcut({FVIRTKEY, VK_F5}));
        commands_.Add(mwfl::Command{kExport, L"Export CSV…", [this] { ExportInteractive(); }}
                          .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'S'}));
        mwfl::Must(accelerators_.Create(commands_), "create SQLite accelerators");
        SetAccelerators(accelerators_.GetHandle());
    }

    void BuildMenu() {
        mwfl::Menu file;
        mwfl::Menu query;
        mwfl::Menu bar;
        mwfl::Must(file.CreatePopup() && query.CreatePopup() && bar.Create(),
                   "create SQLite menus");
        mwfl::Must(file.AppendCommand(*commands_.Find(kOpen)) &&
                       file.AppendCommand(*commands_.Find(kExport)) &&
                       query.AppendCommand(*commands_.Find(kRun)) &&
                       bar.AppendSubmenu(std::move(file), L"&File") &&
                       bar.AppendSubmenu(std::move(query), L"&Query") &&
                       bar.AttachToWindow(GetHwnd()),
                   "attach SQLite menu");
        menu_ = std::move(bar);
    }

    void OpenInteractive() {
        const auto chosen = mwfl::ShowOpenFileDialog(
            {.owner = GetHwnd(),
             .title = L"Open SQLite database read-only",
             .filters = {{L"SQLite databases", L"*.db;*.db3;*.sqlite;*.sqlite3"},
                         {L"All files", L"*.*"}}});
        if (chosen.accepted) OpenPath(chosen.path);
    }

    void OpenPath(const std::filesystem::path& path) {
        std::wstring error;
        if (!database_.OpenReadOnly(path, error)) {
            ShowError(L"The database could not be opened read-only.\n\n" + error);
            return;
        }
        schema_objects_ = database_.ReadSchema(error);
        if (!error.empty()) {
            ShowError(error);
            database_.Close();
            return;
        }
        PopulateSchema();
        SetTitle(path.filename().wstring() + L" — MWFL SQLite Viewer (read-only)");
        status_.SetText(std::to_wstring(schema_objects_.size()) + L" schema objects · " +
                        path.wstring());
        RunQuery();
    }

    void PopulateSchema() {
        TreeView_DeleteAllItems(schema_.GetHwnd());
        const std::array groups{std::pair{mwfl::TreeItemId{1}, L"Tables"},
                                std::pair{mwfl::TreeItemId{2}, L"Views"},
                                std::pair{mwfl::TreeItemId{3}, L"Indexes"},
                                std::pair{mwfl::TreeItemId{4}, L"Triggers"}};
        for (const auto& [id, name] : groups)
            mwfl::Must(schema_.AddItem(id, name) != nullptr, "add SQLite schema group");
        for (std::size_t index = 0; index < schema_objects_.size(); ++index) {
            const auto& object = schema_objects_[index];
            const mwfl::TreeItemId parent = object.type == L"table" ? mwfl::TreeItemId{1}
                                             : object.type == L"view" ? mwfl::TreeItemId{2}
                                             : object.type == L"index" ? mwfl::TreeItemId{3}
                                                                        : mwfl::TreeItemId{4};
            mwfl::Must(schema_.AddChild({100 + index}, object.name, parent) != nullptr,
                       "add SQLite schema object");
        }
        for (const auto& [id, name] : groups) {
            static_cast<void>(name);
            schema_.Expand(id);
        }
    }

    void SelectSchemaObject(mwfl::TreeItemId id) {
        if (id.value < 100 || id.value - 100 >= schema_objects_.size()) return;
        const auto& object = schema_objects_[static_cast<std::size_t>(id.value - 100)];
        if (object.type == L"table" || object.type == L"view") {
            query_.SetText(L"SELECT * FROM " + sqlite_viewer::QuoteIdentifier(object.name) +
                           L" LIMIT 500;");
            RunQuery();
        } else {
            query_.SetText(object.sql);
            status_.SetText(object.type + L" · " + object.name);
        }
    }

    void RunQuery() {
        if (!database_.IsOpen()) return;
        std::wstring error;
        auto result = database_.ExecuteReadOnly(query_.GetText(), 5000, error);
        if (!result) {
            status_.SetText(L"Query failed · " + error);
            if (!g_self_test) ShowError(error);
            return;
        }
        result_ = std::move(*result);
        PopulateResults();
        status_.SetText(std::format(L"{} row(s) · {:.2f} ms{}", result_.rows.size(),
                                    result_.elapsed_ms,
                                    result_.truncated ? L" · limited to 5,000" : L""));
    }

    void PopulateResults() {
        ListView_DeleteAllItems(results_.GetHwnd());
        while (ListView_DeleteColumn(results_.GetHwnd(), 0) != FALSE) {}
        for (const auto& column : result_.columns)
            results_.AddColumn(column, 180);
        for (std::size_t row = 0; row < result_.rows.size(); ++row) {
            const int native_row = results_.AddItem({row + 1},
                                                     result_.rows[row].empty()
                                                         ? L""
                                                         : result_.rows[row][0]);
            for (std::size_t column = 1; column < result_.rows[row].size(); ++column)
                results_.SetSubItem(native_row, static_cast<int>(column),
                                    result_.rows[row][column]);
        }
    }

    void ExportInteractive() {
        if (result_.columns.empty()) return;
        const auto chosen = mwfl::ShowSaveFileDialog(
            {.owner = GetHwnd(),
             .title = L"Export current query results",
             .filters = {{L"CSV file", L"*.csv"}},
             .default_extension = L"csv"});
        if (!chosen.accepted) return;
        std::wstring error;
        if (!sqlite_viewer::ExportCsv(result_, chosen.path, error)) ShowError(error);
        else status_.SetText(L"Exported " + std::to_wstring(result_.rows.size()) + L" rows");
    }

    void RunSelfTest() {
        int result = 0;
        if (schema_objects_.size() != 2) result = 31;
        else if (result_.rows.empty()) result = 32;
        else if (result_.columns.empty()) result = 33;
        else if (!schema_.IsWindow() || !results_.IsWindow()) result = 34;
        ::PostQuitMessage(result);
    }

    void ShowError(const std::wstring& message) const {
        if (!g_self_test)
            ::MessageBoxW(GetHwnd(), message.c_str(), L"MWFL SQLite Viewer",
                          MB_OK | MB_ICONERROR);
    }

    sqlite_viewer::Database database_;
    std::vector<sqlite_viewer::SchemaObject> schema_objects_;
    sqlite_viewer::QueryResult result_;
    std::filesystem::path self_test_path_;
    mwfl::CommandSet commands_;
    mwfl::AcceleratorTable accelerators_;
    mwfl::Menu menu_;
    mwfl::Button open_, run_, export_;
    mwfl::TreeView schema_;
    mwfl::TextBox query_;
    mwfl::ListView results_;
    mwfl::Label status_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    g_showcase = wcsstr(::GetCommandLineW(), L"--showcase") != nullptr;
    return mwfl::RunApplication<SQLiteViewerWindow>(
        instance, show,
        {.title = L"MWFL SQLite Viewer",
         .initial_bounds = {{40.0_dip, 40.0_dip}, {1180.0_dip, 760.0_dip}},
         .use_default_bounds = false});
}
