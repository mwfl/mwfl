#include <mwtl/mwtl.h>
class EvalCommands final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        commands_.Add(mwtl::Command(kSave, L"Save", [this]{ SetTitle(L"Saved"); })
            .SetShortcut({FVIRTKEY | FCONTROL, 'S'}));
        mwtl::ControlHost ui{*this}; ui.Add(save_, L"Save");
        mwtl::Must(accelerators_.Create(commands_), "create accelerators");
        SetAccelerators(accelerators_.GetHandle());
    }
    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(save_)) { commands_.Find(kSave)->Invoke(); return mwtl::EventResult::Handled(); }
        return commands_.Dispatch(event);
    }
private:
    static constexpr mwtl::ControlId kSave{700};
    mwtl::CommandSet commands_; mwtl::AcceleratorTable accelerators_; mwtl::Button save_;
};

