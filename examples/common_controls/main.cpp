#include <mwtl/mwtl.h>

#include <array>

using mwtl::operator""_dip;

class CommonControlsWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Windows Common Controls gallery");
        mwtl::ControlHost ui{*this};
        ui.Add(rebar_, {200}, {16.0_dip, 12.0_dip, 1160.0_dip, 48.0_dip});
        mwtl::ControlHost rebar_ui{rebar_};
        rebar_ui.Add(toolbar_, {201}, {0.0_dip, 0.0_dip, 420.0_dip, 34.0_dip});
        ui.Add(tree_, {202}, {16.0_dip, 76.0_dip, 250.0_dip, 310.0_dip});
        ui.Add(list_, {203}, {282.0_dip, 76.0_dip, 400.0_dip, 180.0_dip});
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
        mwtl::ControlHost pager_ui{pager_};
        pager_ui.Add(pager_text_, {215}, L"Pager child: content can be wider than its viewport", {0.0_dip, 0.0_dip, 820.0_dip, 32.0_dip});
        ui.Add(animation_, {216}, {704.0_dip, 382.0_dip, 120.0_dip, 72.0_dip});
        ui.Add(animation_label_, {217}, L"Animation host\n(resource-driven AVI)", {836.0_dip, 390.0_dip, 260.0_dip, 56.0_dip});
        ui.Add(scroll_, {218}, {16.0_dip, 486.0_dip, 666.0_dip, 28.0_dip});
        ui.Add(status_, {219}, {0.0_dip, 540.0_dip, 1240.0_dip, 28.0_dip});
        mwtl::Must(tooltip_.Create(rebar_.GetHwnd()), "create Tooltip");
        mwtl::Must(images_.Create(16, 16), "create ImageList");

        mwtl::Must(mwtl::AddButtons(toolbar_, {
            {kAbout, L"Task Dialog"}, {kRefresh, L"Refresh"}}),
            "populate Toolbar buttons");
        toolbar_.AutoSize();
        rebar_.AddBand(toolbar_, L"", 360);
        tooltip_.AddTool(toolbar_.GetHwnd(), L"Native Toolbar hosted by a Rebar");

        const HTREEITEM root = tree_.AddItem(L"Common Controls");
        tree_.AddItem(L"Navigation", root);
        tree_.AddItem(L"Input", root);
        tree_.AddItem(L"Commands", root);
        tree_.Expand(root);

        mwtl::Must(mwtl::AddColumns(list_, {
            {L"Control", 170}, {L"Ownership", 150}}),
            "populate ListView columns");
        const int row = list_.AddItem(L"ListView");
        list_.SetSubItem(row, 1, L"Native HWND");
        list_.SetExtendedListStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        mwtl::Must(mwtl::AddColumns(header_, {
            {L"Standalone Header", 210}, {L"Resizable column", 180}}),
            "populate Header columns");
        mwtl::Must(tab_model_.Add({{1}, L"Tree", false, false}), "add Tree workspace tab");
        mwtl::Must(tab_model_.Add({{2}, L"List", false, true}), "add List workspace tab");
        mwtl::Must(tabs_.Synchronize(tab_model_), "populate stable Tabs");
        mwtl::Must(mwtl::AddItems(combo_ex_, {
            L"ComboBoxEx item", L"Image-capable item"}),
            "populate ComboBoxEx items");
        combo_ex_.SetSelection(0);
        hot_key_.SetValue('K', HOTKEYF_CONTROL | HOTKEYF_ALT);
        ip_.SetValue(127, 0, 0, 1);
        spin_.SetBuddy(number_); spin_.SetRange(0, 100); spin_.SetValue(42);
        pager_.SetChild(pager_text_); pager_.SetButtonSize(24);
        scroll_.SetRange(0, 100); scroll_.SetValue(35);
        mwtl::InitializeFlatScrollBars(scroll_.GetHwnd());
        const std::array parts{700, 980, -1};
        status_.SetParts(parts);
        mwtl::Must(mwtl::SetPartTexts(status_, {
            {0, L"All specialized control families are native"},
            {1, L"Unicode + DPI aware"}}), "populate Status text");
        images_.AddIcon(::LoadIconW(nullptr, IDI_INFORMATION));
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.id == kAbout) {
            mwtl::ShowTaskDialog(GetHwnd(), L"mwtl", L"Native Task Dialog", L"This modal control is wrapped as a function, not a child HWND.");
            return mwtl::EventResult::Handled();
        }
        if (event.id == kRefresh) {
            status_.SetPartText(2, L"Toolbar command received");
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

private:
    static constexpr mwtl::ControlId kAbout{500};
    static constexpr mwtl::ControlId kRefresh{501};
    mwtl::Rebar rebar_; mwtl::Toolbar toolbar_; mwtl::TreeView tree_; mwtl::ListView list_;
    mwtl::Header header_; mwtl::TabControl tabs_; mwtl::TabWorkspaceModel tab_model_; mwtl::ComboBoxEx combo_ex_;
    mwtl::DateTimePicker date_; mwtl::MonthCalendar calendar_; mwtl::HotKey hot_key_;
    mwtl::IpAddress ip_; mwtl::TextBox number_; mwtl::UpDown spin_; mwtl::SysLink link_;
    mwtl::Pager pager_; mwtl::Label pager_text_; mwtl::Animation animation_;
    mwtl::Label animation_label_; mwtl::ScrollBar scroll_; mwtl::StatusBar status_;
    mwtl::Tooltip tooltip_; mwtl::ImageList images_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwtl::RunApplication<CommonControlsWindow>(instance, show, {.title = L"Windows Common Controls gallery", .initial_bounds = {{0.0_dip, 0.0_dip}, {1260.0_dip, 620.0_dip}}, .use_default_bounds = false});
}
