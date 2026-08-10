#include <mwfl/mwfl.h>

static_assert(mwfl::Dip{1.0f} < mwfl::Dip{2.0f});
static_assert(mwfl::WindowLike<mwfl::Label>);
static_assert(mwfl::ControlLike<mwfl::Label>);

int main() {
    mwfl::ComboBox combo;
    mwfl::SelectionAdapter<mwfl::ComboBox, int> selections{combo};
    static_cast<void>(selections.Size());
    mwfl::TabWorkspaceModel tabs;
    const bool tab_added = tabs.Add({{7}, L"Installed API", false, true});
    mwfl::DocumentWorkspaceModel documents{{1}};
    const bool document_added = static_cast<bool>(
        documents.Add({{1}, L"Installed document", L"C:\\installed.txt"}));
    const auto document_commands = mwfl::BuildActiveDocumentCommandProjection(documents);
    const auto document_close = mwfl::BuildCoordinatedClosePlan(documents, {});
    mwfl::DocumentSession document_session;
    document_session.workspaces.push_back(mwfl::CaptureWorkspaceSession(documents));
    const auto serialized_session = mwfl::SerializeDocumentSession(document_session);
    mwfl::DocumentTabWorkspaceAdapter document_tabs;
    static_cast<void>(document_tabs.GetPages());
    mwfl::DockLayoutModel docks{{10}, {100}, mwfl::DockGroupRole::document};
    const bool dock_group_added = static_cast<bool>(docks.AddDockedGroup(
        {20}, {200}, mwfl::DockGroupRole::tool, {10}, {300},
        mwfl::DockEdge::bottom, 0.7));
    const bool dock_panel_added = static_cast<bool>(docks.AddPanel(
        {{1}, L"Installed output", mwfl::DockPanelRole::tool}, {20}));
    const auto serialized_docks = mwfl::SerializeDockingSession(docks.GetSnapshot());
    mwfl::WindowOptions auxiliary_window;
    auxiliary_window.quit_on_destroy = false;
    const auto dpi = mwfl::DpiContext::FromDpi(144);
    const mwfl::RectDip concise{mwfl::Dip{1.0f}, mwfl::Dip{2.0f}, mwfl::Dip{3.0f}, mwfl::Dip{4.0f}};
    mwfl::LayoutHost layout(mwfl::Column().Margin(mwfl::Dip{8.0f}));
    mwfl::SplitterModel splitter;
    splitter.SetRatio(0.5f, {mwfl::Dip{400.0f}, mwfl::Dip{200.0f}});
    const auto split = splitter.Arrange({mwfl::Dip{400.0f}, mwfl::Dip{200.0f}});
    mwfl::PropertySheetModel property_pages;
    const bool property_page_added = property_pages.Add({{1}, L"Installed property page"});
    mwfl::TaskDialogOptions task_dialog{.title = L"Installed Task Dialog API"};
    mwfl::DialogOptions custom_dialog{.title = L"Installed custom Dialog API"};
    mwfl::TrayIconOptions tray_icon;
    mwfl::TooltipOptions tooltip_options{.balloon = true};
    mwfl::PopupMenuResult popup_result{mwfl::PopupMenuStatus::cancelled};
    const mwfl::TreeItemId tree_item{8};
    const mwfl::ListItemId list_item{9};
    const mwfl::ListViewOptions virtual_list{.virtual_data = true};
    mwfl::Must(layout.HasRoot(), "installed layout");
    return tab_added && tabs.GetSelectedId() == mwfl::TabId{7} && document_added &&
                   documents.GetActiveId() == mwfl::DocumentId{1} &&
                   document_commands.document == mwfl::DocumentId{1} &&
                   document_close.status == mwfl::CoordinatedCloseStatus::ready &&
                   !serialized_session.empty() &&
                   dock_group_added && dock_panel_added && !serialized_docks.empty() &&
                   !auxiliary_window.quit_on_destroy &&
                   dpi.ToPixels(mwfl::Dip{2.0f}) == 3 && concise.size.width == mwfl::Dip{3.0f} &&
                   layout.HasRoot() && split.constraints_satisfied &&
                   split.first.size.width.value > 0.0f && property_page_added &&
                   property_pages.GetSelectedId() == mwfl::PropertyPageId{1} &&
                   task_dialog.title == L"Installed Task Dialog API" &&
                   custom_dialog.title == L"Installed custom Dialog API" && tray_icon.id == 1 &&
                   tooltip_options.balloon && popup_result.Cancelled() && tree_item && list_item &&
                   virtual_list.virtual_data
               ? 0
               : 1;
}
