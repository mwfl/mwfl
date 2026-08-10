#include <mwfl/mwfl.h>

#include <array>
#include <memory>

using mwfl::operator""_dip;

class GalleryListModel final : public mwfl::VirtualListModel {
public:
    std::size_t GetRowCount() const noexcept override { return 3; }
    mwfl::ListItemId GetRowId(std::size_t row) const noexcept override {
        constexpr mwfl::ListItemId ids[]{{301}, {302}, {303}};
        return row < std::size(ids) ? ids[row] : mwfl::ListItemId{};
    }
    std::wstring GetCellText(std::size_t row, int column) const override {
        static constexpr std::wstring_view names[]{L"TreeView", L"ListView", L"TabControl"};
        if (row >= std::size(names)) return {};
        return column == 0 ? std::wstring{names[row]} : L"Stable model-owned ID";
    }
};

class CommonControlsWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Windows Common Controls gallery");
        mwfl::ControlHost ui{*this};
        ui.Add(rebar_, {200}, {16.0_dip, 12.0_dip, 1160.0_dip, 48.0_dip});
        mwfl::ControlHost rebar_ui{rebar_};
        rebar_ui.Add(toolbar_, {201}, {0.0_dip, 0.0_dip, 420.0_dip, 34.0_dip});
        ui.Add(tree_, {202}, {16.0_dip, 76.0_dip, 250.0_dip, 310.0_dip});
        ui.Add(list_, {203}, {282.0_dip, 76.0_dip, 400.0_dip, 180.0_dip},
               mwfl::ListViewOptions{.virtual_data = true});
        ui.Add(header_, {204}, {282.0_dip, 270.0_dip, 400.0_dip, 34.0_dip});
        ui.Add(tabs_, {205}, {282.0_dip, 318.0_dip, 400.0_dip, 68.0_dip});
        ui.Add(combo_ex_, {206}, {704.0_dip, 76.0_dip, 250.0_dip, 150.0_dip});
        ui.Add(date_, {207}, {704.0_dip, 126.0_dip, 250.0_dip, 34.0_dip});
        ui.Add(calendar_, {208}, {970.0_dip, 76.0_dip, 260.0_dip, 210.0_dip});
        ui.Add(hot_key_, {209}, {704.0_dip, 176.0_dip, 250.0_dip, 34.0_dip});
        ui.Add(ip_, {210}, {704.0_dip, 226.0_dip, 250.0_dip, 34.0_dip});
        ui.Add(number_, {211}, L"42", {704.0_dip, 276.0_dip, 210.0_dip, 34.0_dip});
        ui.Add(spin_, {212}, {914.0_dip, 276.0_dip, 40.0_dip, 34.0_dip});
        ui.Add(link_, {213}, L"Read the <a href=\"https://learn.microsoft.com/windows/win32/controls/\">Common Controls docs</a>", {704.0_dip, 326.0_dip, 510.0_dip, 38.0_dip});
        ui.Add(pager_, {214}, {16.0_dip, 410.0_dip, 666.0_dip, 54.0_dip});
        mwfl::ControlHost pager_ui{pager_};
        pager_ui.Add(pager_text_, {215}, L"Pager child: content can be wider than its viewport", {0.0_dip, 0.0_dip, 820.0_dip, 32.0_dip});
        ui.Add(animation_, {216}, {704.0_dip, 382.0_dip, 120.0_dip, 72.0_dip});
        ui.Add(animation_label_, {217}, L"Animation host\n(resource-driven AVI)", {836.0_dip, 390.0_dip, 260.0_dip, 56.0_dip});
        ui.Add(splitter_, {218}, mwfl::RectDip{16.0_dip, 486.0_dip, 666.0_dip, 80.0_dip},
               mwfl::SplitterOptions{
                   .constraints = {120.0_dip, 120.0_dip, 6.0_dip},
                   .initial_position = 250.0_dip,
               });
        mwfl::ControlHost splitter_ui{splitter_};
        splitter_ui.Add(first_pane_, {219}, L"Tree/details pane", mwfl::RectDip{});
        splitter_ui.Add(second_pane_, {220}, L"Preview pane", mwfl::RectDip{});
        mwfl::Must(splitter_.AttachPanes(first_pane_.GetHwnd(), second_pane_.GetHwnd()),
                   "attach split panes");
        ui.Add(scroll_, {221}, {16.0_dip, 580.0_dip, 666.0_dip, 28.0_dip});
        ui.Add(status_, {222}, {0.0_dip, 630.0_dip, 1240.0_dip, 28.0_dip});
        mwfl::Must(tooltip_.Create(rebar_.GetHwnd(),
                                   {.balloon = true, .max_width = 320}),
                   "create Tooltip");
        mwfl::Must(images_.Create(16, 16), "create ImageList");

        mwfl::Must(mwfl::AddButtons(toolbar_, {
            {kAbout, L"Task Dialog"}, {kRefresh, L"Refresh"}}),
            "populate Toolbar buttons");
        toolbar_.AutoSize();
        rebar_.AddBand(toolbar_, L"", 360);
        mwfl::Must(tooltip_.SetTitle(TTI_INFO, L"Command toolbar"),
                   "set Tooltip title");
        mwfl::Must(tooltip_.AddTool(toolbar_.GetHwnd(),
                                   L"Native Toolbar hosted by a Rebar"),
                   "add Tooltip tool");
        mwfl::Must(tooltip_.UpdateTool(
                       toolbar_.GetHwnd(),
                       L"Task Dialog and Refresh commands; keyboard shortcuts remain available"),
                   "update Tooltip tool");
        mwfl::SetAccessibleName(toolbar_.GetHwnd(), L"Common Controls command toolbar");

        mwfl::Must(tree_.AddItem({101}, L"Common Controls") != nullptr,
                   "add stable TreeView root");
        tree_.AddChild({102}, L"Navigation", {101});
        tree_.AddChild({103}, L"Input", {101});
        tree_.AddChild({104}, L"Commands", {101});
        tree_.Expand({101});

        mwfl::Must(mwfl::AddColumns(list_, {
            {L"Control", 170}, {L"Ownership", 150}}),
            "populate ListView columns");
        mwfl::Must(list_.SetVirtualModel(list_model_), "attach virtual ListView model");
        mwfl::Must(list_.SetSelected({302}), "select stable virtual row");
        list_.SetExtendedListStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        mwfl::Must(mwfl::AddColumns(header_, {
            {L"Standalone Header", 210}, {L"Resizable column", 180}}),
            "populate Header columns");
        mwfl::Must(tab_model_.Add({{1}, L"Tree", false, false}), "add Tree workspace tab");
        mwfl::Must(tab_model_.Add({{2}, L"List", false, true}), "add List workspace tab");
        mwfl::Must(tabs_.Synchronize(tab_model_), "populate stable Tabs");
        mwfl::Must(mwfl::AddItems(combo_ex_, {
            L"ComboBoxEx item", L"Image-capable item"}),
            "populate ComboBoxEx items");
        combo_ex_.SetSelection(0);
        hot_key_.SetValue('K', HOTKEYF_CONTROL | HOTKEYF_ALT);
        ip_.SetValue(127, 0, 0, 1);
        spin_.SetBuddy(number_); spin_.SetRange(0, 100); spin_.SetValue(42);
        pager_.SetChild(pager_text_); pager_.SetButtonSize(24);
        scroll_.SetRange(0, 100); scroll_.SetValue(35);
        mwfl::InitializeFlatScrollBars(scroll_.GetHwnd());
        const std::array parts{700, 980, -1};
        status_.SetParts(parts);
        mwfl::Must(mwfl::SetPartTexts(status_, {
            {0, L"All specialized control families are native"},
            {1, L"Unicode + DPI aware"}}), "populate Status text");
        const int information = images_.AddIcon(::LoadIconW(nullptr, IDI_INFORMATION));
        mwfl::Must(information >= 0 && images_.SetOverlayImage(information, 1),
                   "configure ImageList overlay");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.id == kAbout) {
            mwfl::ShowTaskDialog({
                .owner = GetHwnd(),
                .title = L"mwfl",
                .main_instruction = L"Native Task Dialog",
                .content = L"Buttons, radios, verification, progress, links, and callbacks use a structured result.",
                .verification_text = L"Remember this choice",
                .expanded_information = L"The callback receives a callback-scoped controller for progress and dynamic text.",
                .expanded_control_text = L"Show implementation note",
                .collapsed_control_text = L"Hide implementation note",
                .common_buttons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON,
                .flags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SHOW_PROGRESS_BAR,
                .radio_buttons = {{301, L"Normal priority"}, {302, L"High priority"}},
                .default_radio_button = 301,
                .callback = [](const mwfl::TaskDialogEvent& task_event,
                               const mwfl::TaskDialogController& dialog) {
                    if (task_event.kind == mwfl::TaskDialogEventKind::created) {
                        dialog.SetProgressRange(0, 100);
                        dialog.SetProgressPosition(70);
                    }
                    return mwfl::TaskDialogCallbackResult::continue_default;
                },
            });
            return mwfl::EventResult::Handled();
        }
        if (event.id == kRefresh) {
            status_.SetPartText(2, L"Toolbar command received");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.IsFrom(list_)) {
            LRESULT result = 0;
            if (list_.HandleNotification(event.header, result)) {
                if (const auto error = list_.TakeVirtualException()) std::rethrow_exception(error);
                return mwfl::EventResult::Handled();
            }
        }
        if (event.Is(splitter_, mwfl::kSplitterPositionChanged)) {
            status_.SetPartText(2, L"Splitter position changed");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    static constexpr mwfl::ControlId kAbout{500};
    static constexpr mwfl::ControlId kRefresh{501};
    mwfl::Rebar rebar_; mwfl::Toolbar toolbar_; mwfl::TreeView tree_; mwfl::ListView list_;
    std::shared_ptr<GalleryListModel> list_model_ = std::make_shared<GalleryListModel>();
    mwfl::Header header_; mwfl::TabControl tabs_; mwfl::TabWorkspaceModel tab_model_; mwfl::ComboBoxEx combo_ex_;
    mwfl::DateTimePicker date_; mwfl::MonthCalendar calendar_; mwfl::HotKey hot_key_;
    mwfl::IpAddress ip_; mwfl::TextBox number_; mwfl::UpDown spin_; mwfl::SysLink link_;
    mwfl::Pager pager_; mwfl::Label pager_text_; mwfl::Animation animation_;
    mwfl::Label animation_label_; mwfl::Splitter splitter_;
    mwfl::Label first_pane_; mwfl::Label second_pane_;
    mwfl::ScrollBar scroll_; mwfl::StatusBar status_;
    mwfl::Tooltip tooltip_; mwfl::ImageList images_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<CommonControlsWindow>(instance, show, {.title = L"Windows Common Controls gallery", .initial_bounds = {{0.0_dip, 0.0_dip}, {1260.0_dip, 720.0_dip}}, .use_default_bounds = false});
}
