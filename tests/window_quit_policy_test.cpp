#include <mwtl/mwtl.h>

#include <memory>
#include <stdexcept>

namespace {

constexpr UINT kVerifyQuitPolicy = WM_APP + 0x171;

class AuxiliaryWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {}
};

class MainWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        if (::PostMessageW(GetHwnd(), kVerifyQuitPolicy, 0, 0) == FALSE)
            throw std::runtime_error("post quit policy verification failed");
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& message) override {
        if (message.id != kVerifyQuitPolicy)
            return mwtl::EventResult::Propagate();

        auxiliary_ = std::make_unique<AuxiliaryWindow>();
        mwtl::WindowOptions options;
        options.quit_on_destroy = false;
        auxiliary_->ConfigureWindowOptions(options);
        RECT bounds{50, 50, 250, 200};
        if (!auxiliary_->Create(nullptr, bounds, L"Auxiliary",
                                WS_OVERLAPPEDWINDOW, 0)) {
            ::PostQuitMessage(11);
            return mwtl::EventResult::Handled();
        }
        MSG queued_message{};
        if (::PeekMessageW(&queued_message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
            ::PostQuitMessage(14);
            return mwtl::EventResult::Handled();
        }
        if (!::DestroyWindow(auxiliary_->GetHwnd())) {
            ::PostQuitMessage(12);
            return mwtl::EventResult::Handled();
        }
        if (::PeekMessageW(&queued_message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
            ::PostQuitMessage(13);
            return mwtl::EventResult::Handled();
        }
        ::PostQuitMessage(0);
        return mwtl::EventResult::Handled();
    }

private:
    std::unique_ptr<AuxiliaryWindow> auxiliary_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    return mwtl::RunApplication<MainWindow>(instance, SW_HIDE,
        {.title = L"Window quit policy test"});
}
