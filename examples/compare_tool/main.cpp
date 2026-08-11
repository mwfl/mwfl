#include <mwfl/mwfl.h>

#include "compare_model.h"

#include <windowsx.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

using mwfl::operator""_dip;

namespace {

constexpr mwfl::ControlId kBrowseLeft{1800};
constexpr mwfl::ControlId kBrowseRight{1801};
constexpr mwfl::ControlId kCompare{1802};
constexpr mwfl::ControlId kCancel{1803};
constexpr mwfl::ControlId kBack{1804};
constexpr mwfl::ControlId kCopyRight{1805};
constexpr mwfl::ControlId kCopyLeft{1806};
constexpr mwfl::ControlId kLeftPath{1810};
constexpr mwfl::ControlId kRightPath{1811};
constexpr mwfl::ControlId kShowSame{1812};
constexpr mwfl::ControlId kExclusions{1813};
constexpr mwfl::ControlId kResults{1814};
constexpr UINT kSelfTestFailed = WM_APP + 0x221;
bool g_self_test = false;
bool g_showcase = false;
constexpr wchar_t kSettingsKey[] = L"Software\\mwfl\\Examples\\CompareTool";

std::wstring SizeText(std::uintmax_t bytes, bool directory) {
    if (directory) return L"Folder";
    if (bytes >= 1024 * 1024)
        return std::format(L"{:.1f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    if (bytes >= 1024)
        return std::format(L"{:.1f} KB", static_cast<double>(bytes) / 1024.0);
    return std::to_wstring(bytes) + L" B";
}

std::wstring LineKindText(compare_tool::LineKind kind) {
    switch (kind) {
        case compare_tool::LineKind::same: return L"Same";
        case compare_tool::LineKind::modified: return L"Modified";
        case compare_tool::LineKind::left_only: return L"Left only";
        case compare_tool::LineKind::right_only: return L"Right only";
    }
    return {};
}

class FolderListModel final : public mwfl::VirtualListModel {
public:
    void Reset(std::shared_ptr<const compare_tool::FolderResult> result) {
        result_ = std::move(result);
    }
    std::size_t GetRowCount() const noexcept override {
        return result_ ? result_->entries.size() : 0;
    }
    mwfl::ListItemId GetRowId(std::size_t row) const noexcept override {
        return row < GetRowCount() ? mwfl::ListItemId{row + 1} : mwfl::ListItemId{};
    }
    std::wstring GetCellText(std::size_t row, int column) const override {
        if (!result_ || row >= result_->entries.size()) return {};
        const auto& entry = result_->entries[row];
        switch (column) {
            case 0: return compare_tool::StatusText(entry.status);
            case 1: return entry.relative_path.generic_wstring();
            case 2: return SizeText(entry.left_size, entry.left_directory);
            case 3: return SizeText(entry.right_size, entry.right_directory);
            case 4: return entry.detail;
            default: return {};
        }
    }
    const compare_tool::Entry* Get(std::size_t row) const noexcept {
        return result_ && row < result_->entries.size() ? &result_->entries[row] : nullptr;
    }

private:
    std::shared_ptr<const compare_tool::FolderResult> result_;
};

class DiffListModel final : public mwfl::VirtualListModel {
public:
    void Reset(std::shared_ptr<const compare_tool::TextDiff> diff) { diff_ = std::move(diff); }
    std::size_t GetRowCount() const noexcept override {
        return diff_ ? diff_->lines.size() : 0;
    }
    mwfl::ListItemId GetRowId(std::size_t row) const noexcept override {
        return row < GetRowCount() ? mwfl::ListItemId{row + 1} : mwfl::ListItemId{};
    }
    std::wstring GetCellText(std::size_t row, int column) const override {
        if (!diff_ || row >= diff_->lines.size()) return {};
        const auto& line = diff_->lines[row];
        switch (column) {
            case 0: return LineKindText(line.kind);
            case 1: return line.left_number ? std::to_wstring(line.left_number) : L"";
            case 2: return line.left;
            case 3: return line.right_number ? std::to_wstring(line.right_number) : L"";
            case 4: return line.right;
            default: return {};
        }
    }

private:
    std::shared_ptr<const compare_tool::TextDiff> diff_;
};

class CompareWindow final : public mwfl::WindowBase {
public:
    ~CompareWindow() noexcept override {
        cancel_.store(true, std::memory_order_relaxed);
        if (worker_.joinable()) worker_.join();
        if (!self_test_root_.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(self_test_root_, ignored);
        }
    }

    void BuildUI() override {
        SetTitle(L"MWFL Compare — local file and folder comparison");
        mwfl::ControlHost ui{*this};
        ui.Add(left_path_, kLeftPath, L"");
        ui.Add(browse_left_, kBrowseLeft, L"Left…");
        ui.Add(right_path_, kRightPath, L"");
        ui.Add(browse_right_, kBrowseRight, L"Right…");
        ui.Add(compare_, kCompare, L"Compare  F5");
        ui.Add(cancel_button_, kCancel, L"Cancel");
        ui.Add(back_, kBack, L"Back to folders");
        ui.Add(copy_right_, kCopyRight, L"Copy →");
        ui.Add(copy_left_, kCopyLeft, L"← Copy");
        ui.Add(show_same_, kShowSame, L"Show identical");
        show_same_.SetChecked(false);
        ui.Add(exclusions_, kExclusions, L".git;.vs;build;*.obj;*.pdb");
        ui.Add(results_, kResults, mwfl::ListViewOptions{.virtual_data = true});
        ui.Add(status_, L"Choose two files or folders. All comparisons stay on this PC.");

        left_path_.SetCueBanner(L"Left file or folder");
        right_path_.SetCueBanner(L"Right file or folder");
        exclusions_.SetCueBanner(L"Excluded names, separated by semicolons");
        results_.SetExtendedListStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                      LVS_EX_HEADERDRAGDROP | LVS_EX_GRIDLINES);
        folder_model_ = std::make_shared<FolderListModel>();
        diff_model_ = std::make_shared<DiffListModel>();
        ConfigureFolderColumns();
        mwfl::Must(results_.SetVirtualModel(folder_model_), "attach compare result model");
        mwfl::EnableFileDrop(GetHwnd());
        mwfl::Must(mwfl::SetAccessibleName(left_path_.GetHwnd(), L"Left comparison path"),
                   "name left path");
        mwfl::Must(mwfl::SetAccessibleName(right_path_.GetHwnd(), L"Right comparison path"),
                   "name right path");
        mwfl::Must(mwfl::SetAccessibleName(results_.GetHwnd(), L"Comparison results"),
                   "name comparison results");

        SetLayout(mwfl::Column()
                      .Margin(8.0_dip)
                      .Gap(6.0_dip)
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(left_path_, mwfl::Stretch())
                               .Add(browse_left_, mwfl::Auto()),
                           mwfl::Auto())
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(right_path_, mwfl::Stretch())
                               .Add(browse_right_, mwfl::Auto()),
                           mwfl::Auto())
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(compare_, mwfl::Auto())
                               .Add(cancel_button_, mwfl::Auto())
                               .Add(back_, mwfl::Auto())
                               .Add(copy_left_, mwfl::Auto())
                               .Add(copy_right_, mwfl::Auto())
                               .Add(show_same_, mwfl::Auto())
                               .Add(exclusions_, mwfl::Stretch()),
                           mwfl::Auto())
                      .Add(results_, mwfl::Stretch())
                      .Add(status_, mwfl::Auto()));
        UpdateControls(false, false);
        mwfl::ApplyWindowAppearance(GetHwnd());

        if (!g_self_test && !g_showcase) {
            mwfl::SavedWindowPlacement saved;
            if (mwfl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kSettingsKey,
                                                      L"WindowPlacement", saved))
                mwfl::RestoreWindowPlacement(GetHwnd(), saved);
        }
        if (g_self_test || g_showcase) SetupSelfTest();
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(browse_left_)) return Browse(true);
        if (event.IsClicked(browse_right_)) return Browse(false);
        if (event.IsClicked(compare_)) {
            StartComparison();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(cancel_button_)) {
            cancel_.store(true, std::memory_order_relaxed);
            status_.SetText(L"Cancelling…");
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(back_)) {
            ShowFolderResults();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(copy_right_)) {
            CopySelected(true);
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(copy_left_)) {
            CopySelected(false);
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(show_same_)) {
            if (!busy_ && !diff_mode_) StartComparison();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.IsFrom(results_)) {
            LRESULT native_result = 0;
            if (results_.HandleNotification(event.header, native_result)) {
                if (auto error = results_.TakeVirtualException()) std::rethrow_exception(error);
                return mwfl::EventResult::Handled(native_result);
            }
            if (const auto notification = results_.DecodeNotification(event.header);
                notification &&
                notification->kind == mwfl::ListViewNotificationKind::activated) {
                if (!diff_mode_ && notification->row >= 0)
                    OpenDiff(static_cast<std::size_t>(notification->row));
                return mwfl::EventResult::Handled();
            }
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnWakeup() noexcept override {
        try {
            std::optional<compare_tool::FolderResult> completed;
            std::wstring progress;
            std::wstring worker_error;
            {
                std::scoped_lock lock(worker_mutex_);
                completed = std::move(pending_result_);
                pending_result_.reset();
                progress = progress_text_;
                worker_error = std::move(worker_error_);
                worker_error_.clear();
            }
            if (!worker_error.empty()) {
                if (worker_.joinable()) worker_.join();
                busy_ = false;
                UpdateControls(false, false);
                status_.SetText(L"Comparison failed · " + worker_error);
                if (g_self_test) ::PostQuitMessage(44);
                else ShowError(worker_error);
            } else if (completed) {
                if (worker_.joinable()) worker_.join();
                busy_ = false;
                if (completed->cancelled) {
                    status_.SetText(L"Comparison cancelled.");
                    UpdateControls(false, false);
                } else {
                    folder_result_ = std::make_shared<compare_tool::FolderResult>(
                        std::move(*completed));
                    ShowFolderResults();
                    if (g_self_test) FinishSelfTest();
                }
            } else if (!progress.empty()) {
                status_.SetText(progress);
            }
        } catch (...) {
            ::PostMessageW(GetHwnd(), kSelfTestFailed, 0, 0);
        }
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == WM_DROPFILES) {
            const auto files = mwfl::ReadDroppedFiles(reinterpret_cast<HDROP>(event.wparam));
            if (!files.empty()) left_path_.SetText(files[0]);
            if (files.size() > 1) {
                right_path_.SetText(files[1]);
                StartComparison();
            }
            return mwfl::EventResult::Handled();
        }
        if (event.id == WM_KEYDOWN && event.wparam == VK_F5) {
            StartComparison();
            return mwfl::EventResult::Handled();
        }
        if (event.id == WM_THEMECHANGED || event.id == WM_SETTINGCHANGE) {
            mwfl::ApplyWindowAppearance(GetHwnd());
            return mwfl::EventResult::Handled();
        }
        if (event.id == kSelfTestFailed) {
            ::PostQuitMessage(41);
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
    mwfl::EventResult Browse(bool left) {
        const auto chosen = mwfl::ShowFolderDialog(
            {.owner = GetHwnd(), .title = left ? L"Choose left folder" : L"Choose right folder"});
        if (chosen.accepted) (left ? left_path_ : right_path_).SetText(chosen.path.wstring());
        return mwfl::EventResult::Handled();
    }

    void StartComparison() {
        if (busy_) return;
        const std::filesystem::path left{left_path_.GetText()};
        const std::filesystem::path right{right_path_.GetText()};
        std::error_code error;
        const bool left_directory = std::filesystem::is_directory(left, error);
        error.clear();
        const bool right_directory = std::filesystem::is_directory(right, error);
        if (!left_directory || !right_directory) {
            if (std::filesystem::is_regular_file(left) &&
                std::filesystem::is_regular_file(right)) {
                left_root_ = left.parent_path();
                right_root_ = right.parent_path();
                ShowDiff(left, right, left.filename().wstring());
            } else {
                ShowError(L"Choose two existing files or two existing folders.");
            }
            return;
        }
        if (worker_.joinable()) worker_.join();
        left_root_ = left;
        right_root_ = right;
        cancel_.store(false, std::memory_order_relaxed);
        busy_ = true;
        diff_mode_ = false;
        UpdateControls(true, false);
        status_.SetText(L"Scanning folders…");
        const compare_tool::FolderOptions options{.include_same = show_same_.IsChecked(),
                                                   .verify_contents = true,
                                                   .exclusions = exclusions_.GetText()};
        const mwfl::WindowWakeup wake = GetWakeup();
        worker_ = std::jthread([this, left, right, options, wake] {
            try {
                auto result = compare_tool::CompareFolders(
                    left, right, options, &cancel_, [this, wake](std::size_t scanned,
                                                                 std::wstring_view current) {
                        {
                            std::scoped_lock lock(worker_mutex_);
                            progress_text_ = L"Scanned " + std::to_wstring(scanned) +
                                             L" items · " + std::wstring(current);
                        }
                        wake.TryWake();
                    });
                {
                    std::scoped_lock lock(worker_mutex_);
                    pending_result_ = std::move(result);
                    progress_text_.clear();
                }
            } catch (const std::exception&) {
                std::scoped_lock lock(worker_mutex_);
                worker_error_ = L"The filesystem comparison raised an exception.";
            } catch (...) {
                std::scoped_lock lock(worker_mutex_);
                worker_error_ = L"The filesystem comparison failed unexpectedly.";
            }
            wake.TryWake();
        });
    }

    void ShowFolderResults() {
        diff_mode_ = false;
        ConfigureFolderColumns();
        folder_model_->Reset(folder_result_);
        mwfl::Must(results_.SetVirtualModel(folder_model_), "show folder comparison model");
        results_.RefreshVirtualModel();
        UpdateControls(false, false);
        if (folder_result_) {
            status_.SetText(std::format(
                L"{} differences · {} left only · {} right only · {} identical · {} errors",
                folder_result_->different, folder_result_->left_only,
                folder_result_->right_only, folder_result_->same, folder_result_->errors));
        }
    }

    void OpenDiff(std::size_t row) {
        const auto* entry = folder_model_->Get(row);
        if (!entry || entry->left_directory || entry->right_directory ||
            (entry->status != compare_tool::Status::different &&
             entry->status != compare_tool::Status::same))
            return;
        ShowDiff(left_root_ / entry->relative_path, right_root_ / entry->relative_path,
                 entry->relative_path.generic_wstring());
    }

    void ShowDiff(const std::filesystem::path& left, const std::filesystem::path& right,
                  std::wstring_view label) {
        auto diff = std::make_shared<compare_tool::TextDiff>(
            compare_tool::CompareTextFiles(left, right));
        if (!diff->error.empty()) {
            ShowError(diff->error);
            return;
        }
        diff_mode_ = true;
        ConfigureDiffColumns();
        diff_model_->Reset(diff);
        mwfl::Must(results_.SetVirtualModel(diff_model_), "show text diff model");
        results_.RefreshVirtualModel();
        UpdateControls(false, true);
        status_.SetText(std::format(L"{} · {} changed row(s){}", label, diff->changed,
                                    diff->left_final_newline == diff->right_final_newline
                                        ? L""
                                        : L" · final newline differs"));
    }

    void ConfigureFolderColumns() {
        ResetColumns();
        mwfl::Must(mwfl::AddColumns(results_, {{L"Status", 110},
                                                {L"Relative path", 430},
                                                {L"Left", 110},
                                                {L"Right", 110},
                                                {L"Details", 260}}),
                   "add folder comparison columns");
    }

    void ConfigureDiffColumns() {
        ResetColumns();
        mwfl::Must(mwfl::AddColumns(results_, {{L"Change", 90},
                                                {L"L#", 58},
                                                {L"Left text", 430},
                                                {L"R#", 58},
                                                {L"Right text", 430}}),
                   "add text comparison columns");
    }

    void ResetColumns() {
        while (ListView_DeleteColumn(results_.GetHwnd(), 0) != FALSE) {}
    }

    void CopySelected(bool left_to_right) {
        if (diff_mode_ || busy_ || !folder_result_) return;
        const auto selected = results_.GetSelectedItemIds();
        if (selected.empty()) return;
        const std::size_t row = static_cast<std::size_t>(selected.front().value - 1);
        const auto* entry = folder_model_->Get(row);
        if (!entry) return;
        const auto source = left_to_right ? left_root_ : right_root_;
        const auto destination = left_to_right ? right_root_ : left_root_;
        std::error_code exists_error;
        if (!std::filesystem::exists(source / entry->relative_path, exists_error) ||
            exists_error) {
            ShowError(L"The selected item does not exist on the source side.");
            return;
        }
        if (!g_self_test) {
            const std::wstring prompt =
                L"Copy this item " + std::wstring(left_to_right ? L"left → right" : L"right → left") +
                L"?\n\n" + entry->relative_path.generic_wstring() +
                L"\n\nExisting destination files will be replaced only after the copied bytes verify.";
            if (::MessageBoxW(GetHwnd(), prompt.c_str(), L"Confirm verified copy",
                              MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK)
                return;
        }
        std::wstring error;
        if (!compare_tool::CopyEntryVerified(source, destination, *entry, error))
            ShowError(error);
        else
            StartComparison();
    }

    void UpdateControls(bool busy, bool diff) {
        browse_left_.SetEnabled(!busy);
        browse_right_.SetEnabled(!busy);
        compare_.SetEnabled(!busy && !diff);
        cancel_button_.SetEnabled(busy);
        back_.SetEnabled(diff);
        copy_left_.SetEnabled(!busy && !diff);
        copy_right_.SetEnabled(!busy && !diff);
        show_same_.SetEnabled(!busy && !diff);
        exclusions_.SetEnabled(!busy && !diff);
    }

    void SetupSelfTest() {
        self_test_root_ = std::filesystem::temp_directory_path() /
                          (L"mwfl-compare-" + std::to_wstring(::GetCurrentProcessId()));
        const auto left = self_test_root_ / L"left";
        const auto right = self_test_root_ / L"right";
        std::filesystem::create_directories(left / L"nested");
        std::filesystem::create_directories(right / L"nested");
        const auto write = [](const std::filesystem::path& path, std::string_view text) {
            std::ofstream stream(path, std::ios::binary);
            stream << text;
        };
        write(left / L"same.txt", "same\n");
        write(right / L"same.txt", "same\n");
        write(left / L"changed.txt", "before\nline\n");
        write(right / L"changed.txt", "after\nline\n");
        write(left / L"left.txt", "left\n");
        write(right / L"right.txt", "right\n");
        left_path_.SetText(left.wstring());
        right_path_.SetText(right.wstring());
        StartComparison();
    }

    void FinishSelfTest() {
        if (!folder_result_ || folder_result_->different != 1 ||
            folder_result_->left_only != 1 || folder_result_->right_only != 1 ||
            !results_.IsWindow() || !status_.IsWindow()) {
            ::PostQuitMessage(42);
            return;
        }
        const auto diff = compare_tool::CompareTextFiles(self_test_root_ / L"left/changed.txt",
                                                         self_test_root_ / L"right/changed.txt");
        ::PostQuitMessage(diff.changed == 1 ? 0 : 43);
    }

    void ShowError(const std::wstring& error) const {
        if (!g_self_test)
            ::MessageBoxW(GetHwnd(), error.c_str(), L"MWFL Compare", MB_OK | MB_ICONERROR);
    }

    std::filesystem::path left_root_, right_root_, self_test_root_;
    std::shared_ptr<compare_tool::FolderResult> folder_result_;
    std::shared_ptr<FolderListModel> folder_model_;
    std::shared_ptr<DiffListModel> diff_model_;
    std::atomic_bool cancel_{false};
    std::mutex worker_mutex_;
    std::optional<compare_tool::FolderResult> pending_result_;
    std::wstring progress_text_;
    std::wstring worker_error_;
    bool busy_ = false;
    bool diff_mode_ = false;
    mwfl::Button browse_left_, browse_right_, compare_, cancel_button_, back_, copy_right_,
        copy_left_;
    mwfl::TextBox left_path_, right_path_, exclusions_;
    mwfl::CheckBox show_same_;
    mwfl::ListView results_;
    mwfl::Label status_;
    std::jthread worker_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    g_showcase = wcsstr(::GetCommandLineW(), L"--showcase") != nullptr;
    return mwfl::RunApplication<CompareWindow>(
        instance, show,
        {.title = L"MWFL Compare",
         .initial_bounds = {{30.0_dip, 30.0_dip}, {1280.0_dip, 780.0_dip}},
         .use_default_bounds = false});
}
