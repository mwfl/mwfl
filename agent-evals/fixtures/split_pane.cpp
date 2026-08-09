#include <mwtl/mwtl.h>

using mwtl::operator""_dip;

class SplitPaneEval final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this};
        ui.Add(splitter_, {200}, mwtl::RectDip{},
               mwtl::SplitterOptions{
                   .constraints = {160.0_dip, 240.0_dip, 6.0_dip},
                   .initial_position = 240.0_dip,
               });
        mwtl::ControlHost panes{splitter_};
        panes.Add(tree_, {201}, mwtl::RectDip{});
        panes.Add(list_, {202}, mwtl::RectDip{});
        mwtl::Must(splitter_.AttachPanes(tree_.GetHwnd(), list_.GetHwnd()),
                   "attach panes");
        SetLayout(mwtl::Column().Add(splitter_, mwtl::Stretch()));
    }

    mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
        if (event.Is(splitter_, mwtl::kSplitterPositionChanged))
            return mwtl::EventResult::Handled();
        return mwtl::EventResult::Propagate();
    }

private:
    mwtl::Splitter splitter_;
    mwtl::TreeView tree_;
    mwtl::ListView list_;
};
