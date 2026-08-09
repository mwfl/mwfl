#include <mwtl/mwtl.h>

static_assert(mwtl::Dip{1.0f} < mwtl::Dip{2.0f});
static_assert(mwtl::WindowLike<mwtl::Label>);
static_assert(mwtl::ControlLike<mwtl::Label>);

int main() {
    mwtl::ComboBox combo;
    mwtl::SelectionAdapter<mwtl::ComboBox, int> selections{combo};
    static_cast<void>(selections.Size());
    mwtl::TabWorkspaceModel tabs;
    const bool tab_added = tabs.Add({{7}, L"Installed API", false, true});
    const auto dpi = mwtl::DpiContext::FromDpi(144);
    const mwtl::RectDip concise{mwtl::Dip{1.0f}, mwtl::Dip{2.0f}, mwtl::Dip{3.0f}, mwtl::Dip{4.0f}};
    mwtl::LayoutHost layout(mwtl::Column().Margin(mwtl::Dip{8.0f}));
    mwtl::SplitterModel splitter;
    splitter.SetRatio(0.5f, {mwtl::Dip{400.0f}, mwtl::Dip{200.0f}});
    const auto split = splitter.Arrange({mwtl::Dip{400.0f}, mwtl::Dip{200.0f}});
    mwtl::PropertySheetModel property_pages;
    const bool property_page_added = property_pages.Add({{1}, L"Installed property page"});
    mwtl::TaskDialogOptions task_dialog{.title = L"Installed Task Dialog API"};
    mwtl::DialogOptions custom_dialog{.title = L"Installed custom Dialog API"};
    mwtl::TrayIconOptions tray_icon;
    mwtl::TooltipOptions tooltip_options{.balloon = true};
    mwtl::PopupMenuResult popup_result{mwtl::PopupMenuStatus::cancelled};
    const mwtl::TreeItemId tree_item{8};
    const mwtl::ListItemId list_item{9};
    const mwtl::ListViewOptions virtual_list{.virtual_data = true};
    mwtl::Must(layout.HasRoot(), "installed layout");
    return tab_added && tabs.GetSelectedId() == mwtl::TabId{7} &&
                   dpi.ToPixels(mwtl::Dip{2.0f}) == 3 && concise.size.width == mwtl::Dip{3.0f} &&
                   layout.HasRoot() && split.constraints_satisfied &&
                   split.first.size.width.value > 0.0f && property_page_added &&
                   property_pages.GetSelectedId() == mwtl::PropertyPageId{1} &&
                   task_dialog.title == L"Installed Task Dialog API" &&
                   custom_dialog.title == L"Installed custom Dialog API" && tray_icon.id == 1 &&
                   tooltip_options.balloon && popup_result.Cancelled() && tree_item && list_item &&
                   virtual_list.virtual_data
               ? 0
               : 1;
}
