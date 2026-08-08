#include <mwtl/mwtl.h>
class EvalPlacement final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::SavedWindowPlacement value;
        if (mwtl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, key_, L"Window", value))
            mwtl::RestoreWindowPlacement(GetHwnd(), value);
    }
    mwtl::EventResult OnClose() override {
        mwtl::SavedWindowPlacement value;
        if (mwtl::CaptureWindowPlacement(GetHwnd(), value))
            mwtl::SaveWindowPlacementToRegistry(HKEY_CURRENT_USER, key_, L"Window", value);
        return mwtl::EventResult::Propagate();
    }
private: static constexpr wchar_t key_[]=L"Software\\mwtl\\AgentEval";
};

