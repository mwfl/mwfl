#include <mwfl/navigation_controls.h>

#include <concepts>

static_assert(std::movable<mwfl::TreeView>);
static_assert(std::movable<mwfl::ListView>);

void ConsumeNavigationControls() {
    mwfl::TreeView tree;
    mwfl::ListView list;
    mwfl::Header header;
    mwfl::TabControl tabs;
    mwfl::ComboBoxEx combo;
    const mwfl::TreeItemId tree_id{1};
    const mwfl::ListItemId list_id{2};
    const mwfl::ListViewOptions virtual_options{.virtual_data = true};
    static_cast<void>(tree);
    static_cast<void>(list);
    static_cast<void>(header);
    static_cast<void>(tabs);
    static_cast<void>(combo);
    static_cast<void>(tree_id);
    static_cast<void>(list_id);
    static_cast<void>(virtual_options);
}
