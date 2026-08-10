#include <mwfl/mwfl.h>
#include <thread>
class EvalBackground final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this}; ui.Add(status_, L"Working");
        const auto wake = GetWakeup(); worker_ = std::jthread([wake]{ wake.TryWake(); });
    }
    mwfl::EventResult OnWakeup() noexcept override {
        status_.SetText(L"Done"); return mwfl::EventResult::Handled();
    }
private: mwfl::Label status_; std::jthread worker_;
};

