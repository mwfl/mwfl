#include <mwfl/mwfl.h>

using mwfl::operator""_dip;

class SplitPaneEval final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(splitter_, {200}, mwfl::RectDip{},
               mwfl::SplitterOptions{
                   .constraints = {160.0_dip, 240.0_dip, 6.0_dip},
                   .initial_position = 240.0_dip,
               });
        mwfl::ControlHost panes{splitter_};
        panes.Add(tree_, {201}, mwfl::RectDip{});
        panes.Add(list_, {202}, mwfl::RectDip{});
        mwfl::Must(splitter_.AttachPanes(tree_.GetHwnd(), list_.GetHwnd()),
                   "attach panes");
        SetLayout(mwfl::Column().Add(splitter_, mwfl::Stretch()));
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.Is(splitter_, mwfl::kSplitterPositionChanged))
            return mwfl::EventResult::Handled();
        return mwfl::EventResult::Propagate();
    }

private:
    mwfl::Splitter splitter_;
    mwfl::TreeView tree_;
    mwfl::ListView list_;
};
