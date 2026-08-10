#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

class ClosePolicyWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Close policy demo — close twice to exit")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnClose() override {
        if (!close_confirmed_) {
            close_confirmed_ = true;
            SetTitle(L"Close requested once — close again to confirm");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    bool close_confirmed_ = false;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<ClosePolicyWindow>(instance, show_command);
}
