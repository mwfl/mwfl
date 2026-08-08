#include <mwtl/mwtl.h>
#include <thread>
class EvalBackground final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this}; ui.Add(status_, L"Working");
        const auto wake = GetWakeup(); worker_ = std::jthread([wake]{ wake.TryWake(); });
    }
    mwtl::EventResult OnWakeup() noexcept override {
        status_.SetText(L"Done"); return mwtl::EventResult::Handled();
    }
private: mwtl::Label status_; std::jthread worker_;
};

