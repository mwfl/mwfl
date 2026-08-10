#include <mwfl/layout.h>

void mwfl_header_layout_smoke() {
    auto root = mwfl::Column();
    root.Margin(mwfl::Dip{8.0f}).Gap(mwfl::Dip{4.0f});
    mwfl::LayoutHost layout(std::move(root));
    static_cast<void>(layout.HasRoot());
}
