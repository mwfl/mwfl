#include <mwfl/mwfl.h>
class EvalPlacement final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::SavedWindowPlacement value;
        if (mwfl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, key_, L"Window", value))
            mwfl::RestoreWindowPlacement(GetHwnd(), value);
    }
    mwfl::EventResult OnClose() override {
        mwfl::SavedWindowPlacement value;
        if (mwfl::CaptureWindowPlacement(GetHwnd(), value))
            mwfl::SaveWindowPlacementToRegistry(HKEY_CURRENT_USER, key_, L"Window", value);
        return mwfl::EventResult::Propagate();
    }
private: static constexpr wchar_t key_[]=L"Software\\mwfl\\AgentEval";
};

