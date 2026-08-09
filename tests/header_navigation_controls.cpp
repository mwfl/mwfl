#include <mwtl/navigation_controls.h>

#include <concepts>

static_assert(std::movable<mwtl::TreeView>);
static_assert(std::movable<mwtl::ListView>);

void ConsumeNavigationControls() {
    mwtl::TreeView tree;
    mwtl::ListView list;
    mwtl::Header header;
    mwtl::TabControl tabs;
    mwtl::ComboBoxEx combo;
    const mwtl::TreeItemId tree_id{1};
    const mwtl::ListItemId list_id{2};
    const mwtl::ListViewOptions virtual_options{.virtual_data = true};
    static_cast<void>(tree);
    static_cast<void>(list);
    static_cast<void>(header);
    static_cast<void>(tabs);
    static_cast<void>(combo);
    static_cast<void>(tree_id);
    static_cast<void>(list_id);
    static_cast<void>(virtual_options);
}
