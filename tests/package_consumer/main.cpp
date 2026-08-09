#include <mwtl/mwtl.h>

static_assert(mwtl::Dip{1.0f} < mwtl::Dip{2.0f});
static_assert(mwtl::WindowLike<mwtl::Label>);
static_assert(mwtl::ControlLike<mwtl::Label>);

int main() {
    mwtl::TabWorkspaceModel tabs;
    const bool tab_added = tabs.Add({{7}, L"Installed API", false, true});
    const auto dpi = mwtl::DpiContext::FromDpi(144);
    const mwtl::RectDip concise{
        mwtl::Dip{1.0f}, mwtl::Dip{2.0f},
        mwtl::Dip{3.0f}, mwtl::Dip{4.0f}};
    mwtl::LayoutHost layout(
        mwtl::Column().Margin(mwtl::Dip{8.0f}));
    mwtl::SplitterModel splitter;
    splitter.SetRatio(0.5f, {mwtl::Dip{400.0f}, mwtl::Dip{200.0f}});
    const auto split = splitter.Arrange({mwtl::Dip{400.0f}, mwtl::Dip{200.0f}});
    mwtl::Must(layout.HasRoot(), "installed layout");
    return tab_added && tabs.GetSelectedId() == mwtl::TabId{7} &&
           dpi.ToPixels(mwtl::Dip{2.0f}) == 3 &&
           concise.size.width == mwtl::Dip{3.0f} && layout.HasRoot() &&
           split.constraints_satisfied && split.first.size.width.value > 0.0f
        ? 0 : 1;
}
