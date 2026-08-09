#include <mwtl/mwtl.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <stdexcept>

using namespace std::chrono_literals;
using mwtl::operator""_dip;

namespace {

static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Label>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Button>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::TextBox>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::CheckBox>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::RadioButton>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::GroupBox>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::ListBox>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::ComboBox>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::ProgressBar>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Slider>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::ScrollBar>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::TreeView>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::ListView>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Header>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::TabControl>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::ComboBoxEx>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::DateTimePicker>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::MonthCalendar>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::HotKey>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::IpAddress>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::UpDown>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::SysLink>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Toolbar>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::StatusBar>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Rebar>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Pager>);
static_assert(mwtl::IntrinsicallyMeasurable<mwtl::Animation>);

bool command_seen = false;
bool key_seen = false;
bool custom_seen = false;
bool timer_seen = false;
bool notify_seen = false;
bool scroll_seen = false;
int scroll_failure = 0;
int notify_failure = 0;

class ModernApiWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this};
        ui.Add(label_, L"Modern API test", mwtl::LabelOptions{});
        ui.Add(text_, L"native edit");
        if (label_.GetId().value != 0x4000 ||
            text_.GetId().value != 0x4001) {
            throw std::runtime_error("automatic control IDs are not sequential");
        }
        ui.Add(button_, kButton, L"Verify");
        ui.Add(check_, kCheck, L"Checked");
        ui.Add(combo_, kCombo);
        ui.Add(progress_, kProgress);
        ui.Add(group_, kGroup, L"Choices");
        ui.Add(radio_, kRadio, L"Selected");
        ui.Add(list_, kList);
        ui.Add(slider_, kSlider);

        ui.Add(tree_, {220}, mwtl::TreeViewOptions{});
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
        mwtl::ControlHost rebar_ui{rebar_};
        rebar_ui.Add(toolbar_, {232});
        ui.Add(pager_, {233});
        mwtl::ControlHost pager_ui{pager_};
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
        mwtl::Must(tooltip_.Create(rebar_.GetHwnd()), "create tooltip");
        mwtl::Must(images_.Create(16, 16), "create image list");
        mwtl::Must(timer_.Start(*this, kTimer, 1ms), "start timer");

        check_.SetChecked(true).SetEnabled(true);
        const std::array combo_items{L"first", L"second"};
        mwtl::Must(mwtl::AddItems(combo_, combo_items),
                   "populate ComboBox");
        progress_.SetRange(0, 100).SetValue(64);
        radio_.SetChecked(true);
        mwtl::Must(mwtl::AddItems(list_, {L"first", L"second"}),
                   "populate ListBox");
        slider_.SetRange(0, 100).SetValue(73);
        const HTREEITEM root = tree_.AddItem(L"root");
        static_cast<void>(tree_.AddItem(L"child", root));
        static_cast<void>(tree_.Expand(root));
        const std::array list_columns{mwtl::ColumnSpec{L"name", 80}};
        mwtl::Must(mwtl::AddColumns(list_view_, list_columns),
                   "populate ListView columns");
        const int list_row = list_view_.AddItem(L"item");
        static_cast<void>(list_view_.SetSubItem(list_row, 0, L"updated"));
        mwtl::Must(mwtl::AddColumns(header_, {{L"header", 80}}),
                   "populate Header columns");
        mwtl::Must(tab_model_.Add({{901}, L"first", false, true}), "add first stable tab");
        mwtl::Must(tab_model_.Add({{902}, L"second", true, true}), "add second stable tab");
        mwtl::Must(tab_model_.Select({902}), "select stable tab");
        mwtl::Must(tabs_.Synchronize(tab_model_), "synchronize native tabs");
        TCITEMW native_tab{};
        native_tab.mask = TCIF_PARAM;
        if (tabs_.GetSelectedTabId() != mwtl::TabId{902} ||
            TabCtrl_GetItem(tabs_.GetHwnd(), 1, &native_tab) == FALSE || native_tab.lParam != 902 ||
            tabs_.SetSelection(mwtl::TabId{999})) {
            throw std::runtime_error("stable native tab state failed");
        }
        if (!tabs_.RemoveTab(mwtl::TabId{901}) ||
            tabs_.GetSelectedTabId() != mwtl::TabId{902} ||
            !tabs_.Synchronize(tab_model_)) {
            throw std::runtime_error("unselected native tab removal failed");
        }
        static_cast<void>(TabCtrl_SetCurSel(tabs_.GetHwnd(), -1));
        if (!tabs_.SetSelection(0) || tabs_.GetSelectedTabId() != mwtl::TabId{901} ||
            !tabs_.SetSelection(mwtl::TabId{902}) || !tabs_.RemoveTab(mwtl::TabId{902}) ||
            tabs_.GetSelectedTabId() != mwtl::TabId{901}) {
            throw std::runtime_error("native tab selection or removal failed");
        }
        mwtl::Must(mwtl::AddItems(combo_ex_, {L"combo"}),
                   "populate ComboBoxEx");
        static_cast<void>(combo_ex_.SetSelection(0));
        hot_key_.SetValue(mwtl::HotKeyValue{'K', HOTKEYF_CONTROL});
        ip_.SetValue(mwtl::IpAddressValue{{127, 0, 0, 1}});
        spin_.SetBuddy(spin_text_).SetRange(0, 100).SetValue(42);
        mwtl::Command toolbar_command({600}, L"Tool");
        const int command_image = images_.AddIcon(
            ::LoadIconW(nullptr, IDI_APPLICATION));
        if (command_image < 0 || !toolbar_.SetImageList(images_))
            throw std::runtime_error("configure toolbar images failed");
        toolbar_command.SetChecked(true).SetImageIndex(command_image);
        if (!toolbar_.AddCommand(toolbar_command))
            throw std::runtime_error("populate toolbar command failed");
        toolbar_command.SetImageIndex(-1);
        if (toolbar_.UpdateCommand(toolbar_command))
            throw std::runtime_error("negative toolbar image index accepted");
        toolbar_command.SetImageIndex(command_image);
        toolbar_command.SetChecked(false).SetEnabled(false).SetVisible(false)
            .SetText(L"Disabled");
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
            toolbar_info.iImage != command_image ||
            (toolbar_info.fsState & TBSTATE_ENABLED) != 0 ||
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
        static_cast<void>(mwtl::InitializeFlatScrollBars(scroll_.GetHwnd()));
        const std::array status_parts{120, -1};
        static_cast<void>(status_bar_.SetParts(status_parts));
        const std::array status_texts{mwtl::StatusPartText{0, L"ready"}};
        mwtl::Must(mwtl::SetPartTexts(status_bar_, status_texts),
                   "populate status text");
        static_cast<void>(tooltip_.AddTool(toolbar_.GetHwnd(), L"tooltip"));
        if (!combo_.SetSelection(1) || combo_.GetSelection() != 1 ||
            !check_.IsChecked() || progress_.GetValue() != 64 ||
            !radio_.IsChecked() || !list_.SetSelection(1) ||
            list_.GetSelectedIndex() != 1 || slider_.GetValue() != 73 ||
            !hot_key_.GetHotKey() || !ip_.GetAddress()) {
            throw std::runtime_error("modern controls state verification failed");
        }

        SetLayout(
            mwtl::Row()
                .Margin(8.0_dip)
                .Gap(8.0_dip)
                .Add(label_, mwtl::Stretch())
                .Add(button_, mwtl::Fixed(100.0_dip)));

        button_.Click();
        ::SendMessageW(GetHwnd(), WM_KEYDOWN, VK_SPACE, 1);
        ::SendMessageW(GetHwnd(), kCustomMessage, 42, 0);
        NMHDR notification{button_.GetHwnd(), static_cast<UINT_PTR>(kButton.value), NM_CLICK};
        ::SendMessageW(GetHwnd(), WM_NOTIFY, notification.idFrom,
                       reinterpret_cast<LPARAM>(&notification));
        ::SendMessageW(GetHwnd(), WM_HSCROLL, SB_THUMBPOSITION,
                       reinterpret_cast<LPARAM>(slider_.GetHwnd()));
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(button_)) {
            command_seen = text_.GetText() == L"native edit";
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnKeyDown(const mwtl::KeyEvent& event) override {
        key_seen = event.virtual_key == VK_SPACE && event.repeat_count == 1;
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
        if (!event.Is(button_, NM_CLICK)) return mwtl::EventResult::Propagate();
        notify_seen = true;
        if (event.GetId() != kButton) notify_failure |= 1;
        if (event.GetControl() != button_.GetHwnd()) notify_failure |= 4;
        return mwtl::EventResult::Handled(1);
    }

    mwtl::EventResult OnScroll(const mwtl::ScrollEvent& event) override {
        scroll_seen = true;
        if (!event.IsFrom(slider_)) scroll_failure |= 1;
        if (!event.horizontal) scroll_failure |= 2;
        if (event.request != SB_THUMBPOSITION) scroll_failure |= 4;
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& message) override {
        if (message.id == kCustomMessage) {
            custom_seen = message.wparam == 42;
            return mwtl::EventResult::Handled(77);
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnTimer(mwtl::TimerId id) override {
        if (id == kTimer) {
            timer_seen = true;
            timer_.Stop();
            ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

private:
    static constexpr UINT kCustomMessage = WM_APP + 76;
    static constexpr mwtl::ControlId kButton{202};
    static constexpr mwtl::ControlId kCheck{203};
    static constexpr mwtl::ControlId kCombo{204};
    static constexpr mwtl::ControlId kProgress{205};
    static constexpr mwtl::ControlId kGroup{206};
    static constexpr mwtl::ControlId kRadio{207};
    static constexpr mwtl::ControlId kList{208};
    static constexpr mwtl::ControlId kSlider{209};
    static constexpr mwtl::TimerId kTimer{9};

    mwtl::Label label_;
    mwtl::TextBox text_;
    mwtl::Button button_;
    mwtl::CheckBox check_;
    mwtl::ComboBox combo_;
    mwtl::ProgressBar progress_;
    mwtl::GroupBox group_;
    mwtl::RadioButton radio_;
    mwtl::ListBox list_;
    mwtl::Slider slider_;
    mwtl::TreeView tree_; mwtl::ListView list_view_; mwtl::Header header_;
    mwtl::TabControl tabs_; mwtl::TabWorkspaceModel tab_model_;
    mwtl::ComboBoxEx combo_ex_; mwtl::DateTimePicker date_;
    mwtl::MonthCalendar calendar_; mwtl::HotKey hot_key_; mwtl::IpAddress ip_;
    mwtl::ImageList images_;
    mwtl::TextBox spin_text_; mwtl::UpDown spin_; mwtl::SysLink link_; mwtl::Rebar rebar_; mwtl::Toolbar toolbar_;
    mwtl::Pager pager_; mwtl::Label pager_label_; mwtl::Animation animation_;
    mwtl::ScrollBar scroll_; mwtl::StatusBar status_bar_; mwtl::Tooltip tooltip_;
    mwtl::UiTimer timer_;
};

}  // namespace

int main() {
    const int result =
        mwtl::Application(::GetModuleHandleW(nullptr)).Run<ModernApiWindow>(SW_HIDE);
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
