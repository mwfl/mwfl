#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

class MinMaxWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"WM_GETMINMAXINFO — minimum 480 × 280 pixels")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnMinMaxInfo(mwfl::MinMaxInfoEvent event) override {
        event.info.ptMinTrackSize = {480, 280};
        return mwfl::EventResult::Handled();
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<MinMaxWindow>(instance, show_command);
}
