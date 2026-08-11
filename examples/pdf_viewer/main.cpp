#include <mwfl/mwfl.h>
#include <mwfl/webview2.h>

#include <shlwapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using mwfl::operator""_dip;

namespace {
constexpr mwfl::ControlId kOpen{1600};
constexpr mwfl::ControlId kCloseTab{1601};
constexpr mwfl::ControlId kBack{1602};
constexpr mwfl::ControlId kForward{1603};
constexpr mwfl::ControlId kReload{1604};
constexpr mwfl::ControlId kTabs{1605};
constexpr mwfl::ControlId kViewer{1606};
constexpr mwfl::ControlId kStatus{1607};
constexpr mwfl::ControlId kExit{1608};
constexpr mwfl::ControlId kRecentBase{1620};
constexpr mwfl::TabId kWelcomeTab{1};
constexpr wchar_t kSettingsKey[] = L"Software\\mwfl\\PdfViewer";
constexpr UINT kFinishSelfTest = WM_APP + 0x270;
constexpr UINT kRestartViewer = WM_APP + 0x271;

struct PdfDocument {
    mwfl::TabId id{};
    std::filesystem::path path;
};

std::optional<std::wstring> FileUri(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path).wstring();
    DWORD length = 32768;
    std::wstring uri(length, L'\0');
    if (FAILED(::UrlCreateFromPathW(absolute.c_str(), uri.data(), &length, 0))) return std::nullopt;
    uri.resize(length);
    return uri;
}

bool WriteSelfTestPdf(const std::filesystem::path& path) {
    std::string pdf = "%PDF-1.4\n";
    std::vector<std::size_t> offsets;
    const auto object = [&](int id, std::string_view body) {
        offsets.push_back(pdf.size());
        pdf += std::to_string(id) + " 0 obj\n" + std::string(body) + "\nendobj\n";
    };
    object(1, "<< /Type /Catalog /Pages 2 0 R >>");
    object(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    object(3,
           "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
           "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>");
    constexpr std::string_view content = "BT /F1 24 Tf 72 720 Td (MWFL PDF Viewer) Tj ET";
    object(4, std::string{"<< /Length "} + std::to_string(content.size()) + " >>\nstream\n" +
                  std::string(content) + "\nendstream");
    object(5, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    const std::size_t xref = pdf.size();
    pdf += "xref\n0 6\n0000000000 65535 f \n";
    for (const auto offset : offsets) {
        std::ostringstream row;
        row << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
        pdf += row.str();
    }
    pdf += "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(pdf.data(), static_cast<std::streamsize>(pdf.size()));
    output.flush();
    return output.good();
}

class PdfViewerWindow final : public mwfl::WindowBase {
   public:
    void BuildUI() override {
        self_test_ =
            std::wstring_view{::GetCommandLineW()}.find(L"--self-test") != std::wstring_view::npos;
        if (!self_test_) {
            const auto loaded = mwfl::LoadRecentFilesFromRegistry(HKEY_CURRENT_USER, kSettingsKey,
                                                                  recent_.GetMaximumEntries());
            if (loaded.Succeeded()) recent_ = std::move(*loaded.value);
        }
        SetTitle(L"MWFL PDF Viewer");
        BuildCommands();
        RefreshRecentCommands();
        mwfl::ControlHost ui{*this};
        ui.Add(open_, kOpen, L"Open PDF...");
        ui.Add(close_tab_, kCloseTab, L"Close tab");
        ui.Add(back_, kBack, L"Back");
        ui.Add(forward_, kForward, L"Forward");
        ui.Add(reload_, kReload, L"Reload");
        ui.Add(tabs_, kTabs);
        ui.AddNative(viewer_, kViewer, mwfl::RectDip{});
        ui.Add(status_, kStatus, L"Starting PDF viewer...");

        mwfl::Must(mwfl::SetAccessibleName(tabs_.GetHwnd(), L"Open PDF documents"),
                   "name PDF tabs");
        mwfl::Must(mwfl::SetAccessibleName(viewer_.GetHwnd(), L"PDF document"), "name PDF content");
        mwfl::Must(mwfl::SetAccessibleName(status_.GetHwnd(), L"PDF viewer status"),
                   "name PDF status");
        BuildMenu();
        mwfl::EnableFileDrop(GetHwnd());
        SetLayout(mwfl::Column()
                      .Gap(6.0_dip)
                      .Margin(8.0_dip)
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(open_, mwfl::Fixed(104.0_dip))
                               .Add(close_tab_, mwfl::Fixed(92.0_dip))
                               .Add(back_, mwfl::Fixed(62.0_dip))
                               .Add(forward_, mwfl::Fixed(72.0_dip))
                               .Add(reload_, mwfl::Fixed(68.0_dip)),
                           mwfl::Fixed(32.0_dip))
                      .Add(tabs_, mwfl::Fixed(32.0_dip))
                      .Add(viewer_, mwfl::Stretch())
                      .Add(status_, mwfl::Auto()));

        mwfl::Must(tabs_model_.Add({kWelcomeTab, L"Welcome", false, false}), "add PDF welcome tab");
        mwfl::Must(tabs_.Synchronize(tabs_model_), "show PDF welcome tab");
        mwfl::ApplyWindowAppearance(GetHwnd(), {mwfl::ColorMode::system, mwfl::Backdrop::mica});
        StartViewer();

        mwfl::SavedWindowPlacement placement;
        if (!self_test_ && mwfl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kSettingsKey,
                                                                 L"WindowPlacement", placement))
            mwfl::RestoreWindowPlacement(GetHwnd(), placement);
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.Is(tabs_, TCN_SELCHANGE)) {
            ActivateSelectedTab();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == WM_DROPFILES) {
            const auto files = mwfl::ReadDroppedFiles(reinterpret_cast<HDROP>(event.wparam));
            for (const auto& value : files) {
                const std::filesystem::path path{value};
                if (_wcsicmp(path.extension().c_str(), L".pdf") == 0) OpenPath(path);
            }
            return mwfl::EventResult::Handled();
        }
        if (event.id == kFinishSelfTest) {
            const bool valid = viewer_.IsReady() && viewer_.GetWebView() != nullptr &&
                               tabs_model_.GetCount() == 2 &&
                               tabs_.GetSelectedTabId() != kWelcomeTab && navigation_succeeded_;
            ::PostQuitMessage(valid ? 0 : 8);
            return mwfl::EventResult::Handled();
        }
        if (event.id == kRestartViewer) {
            if (!viewer_.Restart()) status_.SetText(L"PDF rendering recovery failed");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnClose() override {
        if (!self_test_) {
            mwfl::SavedWindowPlacement placement;
            if (mwfl::CaptureWindowPlacement(GetHwnd(), placement))
                static_cast<void>(mwfl::SaveWindowPlacementToRegistry(
                    HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement));
        }
        viewer_.Close();
        if (self_test_) {
            std::error_code ignored;
            std::filesystem::remove_all(user_data_folder_, ignored);
            std::filesystem::remove(self_test_pdf_, ignored);
        }
        return mwfl::EventResult::Propagate();
    }

   private:
    void BuildCommands() {
        commands_
            .Add(mwfl::Command(kOpen, L"&Open PDF...", [this] { OpenInteractive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'O'}))
            .Add(mwfl::Command(kCloseTab, L"&Close tab", [this] { CloseSelectedTab(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'W'}))
            .Add(mwfl::Command(kBack, L"&Back", [this] { viewer_.GoBack(); })
                     .SetShortcut({FVIRTKEY | FALT, VK_LEFT}))
            .Add(mwfl::Command(kForward, L"&Forward", [this] { viewer_.GoForward(); })
                     .SetShortcut({FVIRTKEY | FALT, VK_RIGHT}))
            .Add(mwfl::Command(kReload, L"&Reload", [this] { viewer_.Reload(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'R'}))
            .Add(mwfl::Command(kExit, L"E&xit", [this] { static_cast<void>(Close()); }));
        for (std::size_t index = 0; index < recent_.GetMaximumEntries(); ++index) {
            commands_.Add(mwfl::Command({static_cast<WORD>(kRecentBase.value + index)},
                                        L"Recent PDF",
                                        [this, index] {
                                            const auto paths = recent_.GetPaths();
                                            if (index < paths.size()) OpenPath(paths[index]);
                                        })
                              .SetEnabled(false));
        }
        mwfl::Must(accelerators_.Create(commands_), "create PDF shortcuts");
        SetAccelerators(accelerators_.GetHandle());
    }

    void RefreshRecentCommands() {
        const auto paths = recent_.GetPaths();
        for (std::size_t index = 0; index < recent_.GetMaximumEntries(); ++index) {
            auto* command = commands_.Find({static_cast<WORD>(kRecentBase.value + index)});
            if (index < paths.size())
                command->SetText(paths[index].filename().wstring()).SetEnabled(true);
            else
                command->SetText(L"Recent PDF").SetEnabled(false);
        }
    }

    void BuildMenu() {
        mwfl::Menu bar, file, view;
        mwfl::Must(bar.Create(), "create PDF menu bar");
        mwfl::Must(file.CreatePopup(), "create PDF File menu");
        mwfl::Must(file.AppendCommand(*commands_.Find(kOpen)), "append PDF Open command");
        mwfl::Must(file.AppendCommand(*commands_.Find(kCloseTab)), "append PDF Close command");
        if (!recent_.GetPaths().empty()) {
            mwfl::Must(file.AppendSeparator(), "append recent PDF separator");
            for (std::size_t index = 0; index < recent_.GetPaths().size(); ++index)
                mwfl::Must(file.AppendCommand(
                               *commands_.Find({static_cast<WORD>(kRecentBase.value + index)})),
                           "append recent PDF");
        }
        mwfl::Must(file.AppendSeparator(), "append PDF Exit separator");
        mwfl::Must(file.AppendCommand(*commands_.Find(kExit)), "append PDF Exit command");
        mwfl::Must(view.CreatePopup(), "create PDF View menu");
        mwfl::Must(view.AppendCommand(*commands_.Find(kReload)), "append PDF Reload command");
        mwfl::Must(bar.AppendSubmenu(std::move(file), L"&File"), "append PDF File menu");
        mwfl::Must(bar.AppendSubmenu(std::move(view), L"&View"), "append PDF View menu");
        mwfl::Must(bar.AttachToWindow(GetHwnd()), "attach PDF menu");
        menu_ = std::move(bar);
    }

    void StartViewer() {
        if (self_test_)
            user_data_folder_ = std::filesystem::temp_directory_path() /
                                (L"mwfl-pdf-viewer-" + std::to_wstring(::GetCurrentProcessId()));
        const bool started = viewer_.Initialize(
            {.user_data_folder = user_data_folder_},
            {.initialized =
                 [this](mwfl::WebView2InitializationResult result) {
                     if (result.state != mwfl::WebView2HostState::ready) {
                         status_.SetText(result.runtime == mwfl::WebView2RuntimeStatus::missing
                                             ? L"WebView2 Runtime is required to display PDF files"
                                             : L"PDF rendering could not start");
                         if (self_test_)
                             ::PostQuitMessage(
                                 result.runtime == mwfl::WebView2RuntimeStatus::missing ? 0 : 2);
                         return;
                     }
                     ShowWelcome();
                 },
             .navigation_completed =
                 [this](bool succeeded, HRESULT) {
                     navigation_succeeded_ = succeeded;
                     status_.SetText(succeeded ? L"Ready" : L"The PDF could not be displayed");
                     if (!self_test_) return;
                     if (!self_test_pdf_started_) {
                         self_test_pdf_started_ = true;
                         self_test_pdf_ = std::filesystem::temp_directory_path() /
                                          (L"mwfl-pdf-viewer-" +
                                           std::to_wstring(::GetCurrentProcessId()) + L".pdf");
                         if (!succeeded || !WriteSelfTestPdf(self_test_pdf_)) {
                             ::PostQuitMessage(3);
                             return;
                         }
                         OpenPath(self_test_pdf_);
                     } else if (::PostMessageW(GetHwnd(), kFinishSelfTest, 0, 0) == FALSE) {
                         ::PostQuitMessage(4);
                     }
                 },
             .process_failed =
                 [this](mwfl::WebView2ProcessFailureKind) {
                     status_.SetText(L"PDF rendering process stopped; reloading...");
                     if (::PostMessageW(GetHwnd(), kRestartViewer, 0, 0) == FALSE)
                         status_.SetText(L"PDF rendering recovery could not be scheduled");
                 }});
        if (!started) status_.SetText(L"PDF rendering initialization failed");
    }

    void ShowWelcome() {
        static_cast<void>(viewer_.NavigateToString(
            L"<!doctype html><meta charset='utf-8'><style>"
            L"body{font:16px Segoe UI;margin:10vh auto;max-width:650px;padding:32px;color:#243447}"
            L"h1{font-size:32px}kbd{background:#eef1f4;padding:3px 7px;border-radius:4px}"
            L"</style><h1>MWFL PDF Viewer</h1>"
            L"<p>Open a local PDF in a clean native Windows workspace.</p>"
            L"<p>Press <kbd>Ctrl+O</kbd> or drop one or more PDF files here.</p>"));
    }

    void OpenInteractive() {
        const auto selected = mwfl::ShowOpenFileDialog(
            {.owner = GetHwnd(),
             .title = L"Open PDF",
             .filters = {{L"PDF documents", L"*.pdf"}, {L"All files", L"*.*"}}});
        if (selected.accepted) OpenPath(selected.path);
    }

    void OpenPath(const std::filesystem::path& path) {
        if (_wcsicmp(path.extension().c_str(), L".pdf") != 0) {
            status_.SetText(L"Only PDF documents can be opened");
            return;
        }
        for (const auto& document : documents_) {
            if (std::filesystem::equivalent(document.path, path, equivalence_error_)) {
                tabs_model_.Select(document.id);
                tabs_.Synchronize(tabs_model_);
                ActivateSelectedTab();
                return;
            }
            equivalence_error_.clear();
        }
        const mwfl::TabId id{next_tab_id_++};
        documents_.push_back({id, path});
        tabs_model_.Add({id, path.filename().wstring(), false, true});
        tabs_model_.Select(id);
        tabs_.Synchronize(tabs_model_);
        if (!self_test_) {
            recent_.Add(path);
            RefreshRecentCommands();
            BuildMenu();
            static_cast<void>(
                mwfl::SaveRecentFilesToRegistry(HKEY_CURRENT_USER, kSettingsKey, recent_));
        }
        ActivateSelectedTab();
    }

    void ActivateSelectedTab() {
        const auto selected = tabs_.GetSelectedTabId();
        if (!selected) return;
        tabs_model_.Select(*selected);
        if (*selected == kWelcomeTab) {
            ShowWelcome();
            SetTitle(L"MWFL PDF Viewer");
            return;
        }
        for (const auto& document : documents_) {
            if (document.id != *selected) continue;
            const auto uri = FileUri(document.path);
            if (!uri || !viewer_.Navigate(*uri)) {
                status_.SetText(L"The PDF path could not be opened");
                return;
            }
            SetTitle(document.path.filename().wstring() + L" - MWFL PDF Viewer");
            status_.SetText(document.path.wstring());
            break;
        }
    }

    void CloseSelectedTab() {
        const auto selected = tabs_.GetSelectedTabId();
        if (!selected || *selected == kWelcomeTab) return;
        std::erase_if(documents_, [&](const PdfDocument& item) { return item.id == *selected; });
        tabs_model_.Remove(*selected);
        tabs_.Synchronize(tabs_model_);
        ActivateSelectedTab();
    }

    mwfl::CommandSet commands_;
    mwfl::AcceleratorTable accelerators_;
    mwfl::Menu menu_;
    mwfl::RecentFileList recent_{8};
    mwfl::Button open_, close_tab_, back_, forward_, reload_;
    mwfl::TabControl tabs_;
    mwfl::TabWorkspaceModel tabs_model_;
    mwfl::WebView2Host viewer_;
    mwfl::Label status_;
    std::vector<PdfDocument> documents_;
    std::filesystem::path user_data_folder_;
    std::filesystem::path self_test_pdf_;
    std::error_code equivalence_error_;
    std::uint64_t next_tab_id_ = 2;
    bool self_test_ = false;
    bool self_test_pdf_started_ = false;
    bool navigation_succeeded_ = false;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<PdfViewerWindow>(instance, show_command,
                                                 {.title = L"MWFL PDF Viewer",
                                                  .initial_bounds = {{}, {1120.0_dip, 780.0_dip}},
                                                  .use_default_bounds = false},
                                                 {.com_apartment = mwfl::ComApartment::sta});
}
