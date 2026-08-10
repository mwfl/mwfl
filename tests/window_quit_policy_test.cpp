#include <mwtl/mwtl.h>

#include <memory>
#include <stdexcept>

namespace {

class AuxiliaryWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {}
};

class MainWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        auxiliary_ = std::make_unique<AuxiliaryWindow>();
        mwtl::WindowOptions options;
        options.quit_on_destroy = false;
        auxiliary_->ConfigureWindowOptions(options);
        RECT bounds{50, 50, 250, 200};
        if (!auxiliary_->Create(nullptr, bounds, L"Auxiliary",
                                WS_OVERLAPPEDWINDOW, 0))
            throw std::runtime_error("create auxiliary window failed");
        if (!::DestroyWindow(auxiliary_->GetHwnd()))
            throw std::runtime_error("destroy auxiliary window failed");
        MSG message{};
        if (::PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE))
            throw std::runtime_error("auxiliary window posted WM_QUIT");
        ::PostQuitMessage(0);
    }

private:
    std::unique_ptr<AuxiliaryWindow> auxiliary_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    return mwtl::RunApplication<MainWindow>(instance, SW_HIDE,
        {.title = L"Window quit policy test"});
}
