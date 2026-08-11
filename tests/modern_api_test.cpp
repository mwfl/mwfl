#include <mwfl/mwfl.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <stdexcept>

using namespace std::chrono_literals;
using mwfl::operator""_dip;

namespace {

static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Label>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Button>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::TextBox>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::CheckBox>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::RadioButton>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::GroupBox>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::ListBox>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::ComboBox>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::ProgressBar>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Slider>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::ScrollBar>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::TreeView>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::ListView>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Header>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::TabControl>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::ComboBoxEx>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::DateTimePicker>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::MonthCalendar>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::HotKey>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::IpAddress>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::UpDown>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::SysLink>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Toolbar>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::StatusBar>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Rebar>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Pager>);
static_assert(mwfl::IntrinsicallyMeasurable<mwfl::Animation>);

bool command_seen = false;
bool key_seen = false;
bool custom_seen = false;
bool timer_seen = false;
bool notify_seen = false;
bool scroll_seen = false;
int scroll_failure = 0;
int notify_failure = 0;

class ModernApiWindow final : public mwfl::WindowBase {
   public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(label_, L"Modern API test", mwfl::LabelOptions{});
        ui.Add(text_, L"native edit");
        if (label_.GetId().value != 0x4000 || text_.GetId().value != 0x4001) {
            throw std::runtime_error("automatic control IDs are not sequential");
        }
        text_.SelectAll();
        if (text_.GetSelection() != mwfl::TextSelection{0, 11} ||
            !text_.ReplaceSelection(L"modern edit") || !text_.CanUndo() ||
            !text_.SetSelection({7, 11}) || text_.GetSelection() != mwfl::TextSelection{7, 11}) {
            throw std::runtime_error("TextBox editing API failed");
        }
        text_.Undo().ScrollCaretIntoView();
        if (!text_.SetCueBanner(L"Type text"))
            throw std::runtime_error("TextBox cue banner failed");
        ui.Add(button_, kButton, L"Verify");
        ui.Add(check_, kCheck, L"Checked");
        ui.Add(combo_, kCombo);
        ui.Add(progress_, kProgress);
        ui.Add(group_, kGroup, L"Choices");
        ui.Add(radio_, kRadio, L"Selected");
        ui.Add(list_, kList);
        ui.Add(slider_, kSlider);

        ui.Add(tree_, {220}, mwfl::TreeViewOptions{});
        ui.Add(list_view_, {221});
        ui.Add(header_, {222});
        ui.Add(tabs_, {223});
        ui.Add(combo_ex_, {224});
        ui.Add(date_, {225});
        ui.Add(calendar_, {226});
        ui.Add(hot_key_, {227});
        ui.Add(ip_, {228});
        ui.Add(spin_text_, {238}, L"0");
        ui.Add(spin_, {229});
        ui.Add(link_, {230}, L"<a href=\"https://example.test\">link</a>");
        ui.Add(rebar_, {231});
        mwfl::ControlHost rebar_ui{rebar_};
        rebar_ui.Add(toolbar_, {232});
        ui.Add(pager_, {233});
        mwfl::ControlHost pager_ui{pager_};
        pager_ui.Add(pager_label_, {234}, L"pager");
        ui.Add(animation_, {235});
        ui.Add(scroll_, {236});
        ui.Add(status_bar_, {237});

        const auto has_intrinsic_size = [&](const auto& control) {
            const auto size = control.GetPreferredSize(GetDpiContext());
            return size.width.value > 0.0f && size.height.value > 0.0f;
        };
        if (!(has_intrinsic_size(label_) && has_intrinsic_size(text_) &&
              has_intrinsic_size(button_) && has_intrinsic_size(check_) &&
              has_intrinsic_size(radio_) && has_intrinsic_size(group_) &&
              has_intrinsic_size(list_) && has_intrinsic_size(combo_) &&
              has_intrinsic_size(progress_) && has_intrinsic_size(slider_) &&
              has_intrinsic_size(scroll_) && has_intrinsic_size(tree_) &&
              has_intrinsic_size(list_view_) && has_intrinsic_size(header_) &&
              has_intrinsic_size(tabs_) && has_intrinsic_size(combo_ex_) &&
              has_intrinsic_size(date_) && has_intrinsic_size(calendar_) &&
              has_intrinsic_size(hot_key_) && has_intrinsic_size(ip_) &&
              has_intrinsic_size(spin_) && has_intrinsic_size(link_) &&
              has_intrinsic_size(toolbar_) && has_intrinsic_size(status_bar_) &&
              has_intrinsic_size(rebar_) && has_intrinsic_size(pager_) &&
              has_intrinsic_size(animation_))) {
            throw std::runtime_error("missing intrinsic control size");
        }
        mwfl::Must(tooltip_.Create(rebar_.GetHwnd()), "create tooltip");
        mwfl::Must(images_.Create(16, 16), "create image list");
        mwfl::Must(timer_.Start(*this, kTimer, 1ms), "start timer");

        check_.SetChecked(true).SetEnabled(true);
        mwfl::SelectionAdapter<mwfl::ComboBox, int> combo_items{combo_};
        mwfl::Must(combo_items.Add(L"first", 10), "add typed ComboBox item");
        mwfl::Must(combo_items.Add(L"second", 20), "add typed ComboBox item");
        mwfl::Must(combo_items.SelectValue(20), "select typed ComboBox value");
        if (!combo_items.GetSelectedValue() || combo_items.GetSelectedValue()->get() != 20 ||
            combo_.GetItemText(1) != L"second" || combo_.GetItemText(2)) {
            throw std::runtime_error("typed ComboBox state failed");
        }
        progress_.SetRange(0, 100).SetValue(64);
        radio_.SetChecked(true);
        mwfl::SelectionAdapter<mwfl::ListBox, std::wstring> list_items{list_};
        mwfl::Must(list_items.Add(L"first", L"alpha"), "add typed ListBox item");
        mwfl::Must(list_items.Add(L"second", L"beta"), "add typed ListBox item");
        mwfl::Must(list_items.SelectValue(L"beta"), "select typed ListBox value");
        if (!list_items.GetSelectedValue() || list_items.GetSelectedValue()->get() != L"beta" ||
            list_.GetItemText(0) != L"first") {
            throw std::runtime_error("typed ListBox state failed");
        }
        slider_.SetRange(0, 100).SetValue(73);
        const HTREEITEM root = tree_.AddItem({1001}, L"root");
        static_cast<void>(tree_.AddItem({1002}, L"child", root));
        static_cast<void>(tree_.Expand(root));
        const std::array list_columns{mwfl::ColumnSpec{L"name", 80}};
        mwfl::Must(mwfl::AddColumns(list_view_, list_columns), "populate ListView columns");
        const int list_row = list_view_.AddItem({1101}, L"item");
        static_cast<void>(list_view_.SetSubItem(list_row, 0, L"updated"));
        mwfl::Must(mwfl::AddColumns(header_, {{L"header", 80}}), "populate Header columns");
        mwfl::Must(tab_model_.Add({{901}, L"first", false, true}), "add first stable tab");
        mwfl::Must(tab_model_.Add({{902}, L"second", true, true}), "add second stable tab");
        mwfl::Must(tab_model_.Select({902}), "select stable tab");
        mwfl::Must(tabs_.Synchronize(tab_model_), "synchronize native tabs");
        mwfl::Must(tabs_.SetSelection(mwfl::TabId{901}), "select first tab for keyboard test");
        ::SendMessageW(tabs_.GetHwnd(), WM_KEYDOWN, VK_RIGHT, 0);
        if (tabs_.GetSelectedTabId() != mwfl::TabId{902}) {
            throw std::runtime_error("native tab keyboard navigation failed");
        }
        TCITEMW native_tab{};
        native_tab.mask = TCIF_PARAM;
        if (tabs_.GetSelectedTabId() != mwfl::TabId{902} ||
            TabCtrl_GetItem(tabs_.GetHwnd(), 1, &native_tab) == FALSE || native_tab.lParam != 902 ||
            tabs_.SetSelection(mwfl::TabId{999})) {
            throw std::runtime_error("stable native tab state failed");
        }
        if (!tabs_.RemoveTab(mwfl::TabId{901}) || tabs_.GetSelectedTabId() != mwfl::TabId{902} ||
            !tabs_.Synchronize(tab_model_)) {
            throw std::runtime_error("unselected native tab removal failed");
        }
        static_cast<void>(TabCtrl_SetCurSel(tabs_.GetHwnd(), -1));
        if (!tabs_.SetSelection(0) || tabs_.GetSelectedTabId() != mwfl::TabId{901} ||
            !tabs_.SetSelection(mwfl::TabId{902}) || !tabs_.RemoveTab(mwfl::TabId{902}) ||
            tabs_.GetSelectedTabId() != mwfl::TabId{901}) {
            throw std::runtime_error("native tab selection or removal failed");
        }
        mwfl::SelectionAdapter<mwfl::ComboBoxEx, int> combo_ex_items{combo_ex_};
        mwfl::Must(combo_ex_items.Add(L"combo", 42), "add typed ComboBoxEx item");
        mwfl::Must(combo_ex_items.Select(0), "select typed ComboBoxEx item");
        if (!combo_ex_items.GetSelectedValue() || combo_ex_items.GetSelectedValue()->get() != 42 ||
            combo_ex_.GetItemText(0) != L"combo") {
            throw std::runtime_error("typed ComboBoxEx state failed");
        }
        hot_key_.SetValue(mwfl::HotKeyValue{'K', HOTKEYF_CONTROL});
        ip_.SetValue(mwfl::IpAddressValue{{127, 0, 0, 1}});
        spin_.SetBuddy(spin_text_).SetRange(0, 100).SetValue(42);
        mwfl::Command toolbar_command({600}, L"Tool");
        const int command_image = images_.AddIcon(::LoadIconW(nullptr, IDI_APPLICATION));
        if (command_image < 0 || !toolbar_.SetImageList(images_))
            throw std::runtime_error("configure toolbar images failed");
        toolbar_command.SetChecked(true).SetImageIndex(command_image);
        if (!toolbar_.AddCommand(toolbar_command))
            throw std::runtime_error("populate toolbar command failed");
        toolbar_command.SetImageIndex(-1);
        if (toolbar_.UpdateCommand(toolbar_command))
            throw std::runtime_error("negative toolbar image index accepted");
        toolbar_command.SetImageIndex(command_image);
        toolbar_command.SetChecked(false).SetEnabled(false).SetVisible(false).SetText(L"Disabled");
        if (!toolbar_.UpdateCommand(toolbar_command))
            throw std::runtime_error("update toolbar command failed");
        TBBUTTONINFOW toolbar_info{};
        wchar_t toolbar_text[32]{};
        toolbar_info.cbSize = sizeof(toolbar_info);
        toolbar_info.dwMask = TBIF_IMAGE | TBIF_STATE | TBIF_TEXT;
        toolbar_info.pszText = toolbar_text;
        toolbar_info.cchText = 32;
        if (::SendMessageW(toolbar_.GetHwnd(), TB_GETBUTTONINFOW, 600,
                           reinterpret_cast<LPARAM>(&toolbar_info)) < 0 ||
            toolbar_info.iImage != command_image || (toolbar_info.fsState & TBSTATE_ENABLED) != 0 ||
            (toolbar_info.fsState & TBSTATE_HIDDEN) == 0 ||
            std::wstring_view{toolbar_text} != L"Disabled") {
            throw std::runtime_error("toolbar command state did not propagate");
        }
        toolbar_command.SetVisible(true);
        if (!toolbar_.UpdateCommand(toolbar_command))
            throw std::runtime_error("show toolbar command failed");
        toolbar_.AutoSize();
        static_cast<void>(rebar_.AddBand(toolbar_, L"Band", 120));
        pager_.SetChild(pager_label_);
        scroll_.SetRange(0, 100).SetValue(31);
        static_cast<void>(mwfl::InitializeFlatScrollBars(scroll_.GetHwnd()));
        const std::array status_parts{120, -1};
        static_cast<void>(status_bar_.SetParts(status_parts));
        const std::array status_texts{mwfl::StatusPartText{0, L"ready"}};
        mwfl::Must(mwfl::SetPartTexts(status_bar_, status_texts), "populate status text");
        static_cast<void>(tooltip_.AddTool(toolbar_.GetHwnd(), L"tooltip"));
        if (!combo_.SetSelection(1) || combo_.GetSelection() != 1 || !check_.IsChecked() ||
            progress_.GetValue() != 64 || !radio_.IsChecked() || !list_.SetSelection(1) ||
            list_.GetSelectedIndex() != 1 || slider_.GetValue() != 73 || !hot_key_.GetHotKey() ||
            !ip_.GetAddress()) {
            throw std::runtime_error("modern controls state verification failed");
        }

        SetLayout(mwfl::Row()
                      .Margin(8.0_dip)
                      .Gap(8.0_dip)
                      .Add(label_, mwfl::Stretch())
                      .Add(button_, mwfl::Fixed(100.0_dip)));

        button_.Click();
        ::SendMessageW(GetHwnd(), WM_KEYDOWN, VK_SPACE, 1);
        ::SendMessageW(GetHwnd(), kCustomMessage, 42, 0);
        NMHDR notification{button_.GetHwnd(), static_cast<UINT_PTR>(kButton.value), NM_CLICK};
        ::SendMessageW(GetHwnd(), WM_NOTIFY, notification.idFrom,
                       reinterpret_cast<LPARAM>(&notification));
        ::SendMessageW(GetHwnd(), WM_HSCROLL, SB_THUMBPOSITION,
                       reinterpret_cast<LPARAM>(slider_.GetHwnd()));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(button_)) {
            command_seen = text_.GetText() == L"native edit";
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnKeyDown(const mwfl::KeyEvent& event) override {
        key_seen = event.virtual_key == VK_SPACE && event.repeat_count == 1;
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (!event.Is(button_, NM_CLICK)) return mwfl::EventResult::Propagate();
        notify_seen = true;
        if (event.GetId() != kButton) notify_failure |= 1;
        if (event.GetControl() != button_.GetHwnd()) notify_failure |= 4;
        return mwfl::EventResult::Handled(1);
    }

    mwfl::EventResult OnScroll(const mwfl::ScrollEvent& event) override {
        scroll_seen = true;
        if (!event.IsFrom(slider_)) scroll_failure |= 1;
        if (!event.horizontal) scroll_failure |= 2;
        if (event.request != SB_THUMBPOSITION) scroll_failure |= 4;
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
        if (message.id == kCustomMessage) {
            custom_seen = message.wparam == 42;
            return mwfl::EventResult::Handled(77);
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnTimer(mwfl::TimerId id) override {
        if (id == kTimer) {
            timer_seen = true;
            timer_.Stop();
            ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

   private:
    static constexpr UINT kCustomMessage = WM_APP + 76;
    static constexpr mwfl::ControlId kButton{202};
    static constexpr mwfl::ControlId kCheck{203};
    static constexpr mwfl::ControlId kCombo{204};
    static constexpr mwfl::ControlId kProgress{205};
    static constexpr mwfl::ControlId kGroup{206};
    static constexpr mwfl::ControlId kRadio{207};
    static constexpr mwfl::ControlId kList{208};
    static constexpr mwfl::ControlId kSlider{209};
    static constexpr mwfl::TimerId kTimer{9};

    mwfl::Label label_;
    mwfl::TextBox text_;
    mwfl::Button button_;
    mwfl::CheckBox check_;
    mwfl::ComboBox combo_;
    mwfl::ProgressBar progress_;
    mwfl::GroupBox group_;
    mwfl::RadioButton radio_;
    mwfl::ListBox list_;
    mwfl::Slider slider_;
    mwfl::TreeView tree_;
    mwfl::ListView list_view_;
    mwfl::Header header_;
    mwfl::TabControl tabs_;
    mwfl::TabWorkspaceModel tab_model_;
    mwfl::ComboBoxEx combo_ex_;
    mwfl::DateTimePicker date_;
    mwfl::MonthCalendar calendar_;
    mwfl::HotKey hot_key_;
    mwfl::IpAddress ip_;
    mwfl::ImageList images_;
    mwfl::TextBox spin_text_;
    mwfl::UpDown spin_;
    mwfl::SysLink link_;
    mwfl::Rebar rebar_;
    mwfl::Toolbar toolbar_;
    mwfl::Pager pager_;
    mwfl::Label pager_label_;
    mwfl::Animation animation_;
    mwfl::ScrollBar scroll_;
    mwfl::StatusBar status_bar_;
    mwfl::Tooltip tooltip_;
    mwfl::UiTimer timer_;
};

}  // namespace

int main() {
    const int result = mwfl::Application(::GetModuleHandleW(nullptr)).Run<ModernApiWindow>(SW_HIDE);
    if (result != 0) return 10;
    if (!command_seen) return 11;
    if (!key_seen) return 12;
    if (!custom_seen) return 13;
    if (!timer_seen) return 14;
    if (!notify_seen) return 15;
    if (!scroll_seen) return 16;
    if (scroll_failure != 0) return 30 + scroll_failure;
    if (notify_failure != 0) return 20 + notify_failure;
    return EXIT_SUCCESS;
}
