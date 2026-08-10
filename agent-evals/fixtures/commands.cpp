#include <mwfl/mwfl.h>
class EvalCommands final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        commands_.Add(mwfl::Command(kSave, L"Save", [this]{ SetTitle(L"Saved"); })
            .SetShortcut({FVIRTKEY | FCONTROL, 'S'}));
        mwfl::ControlHost ui{*this}; ui.Add(save_, L"Save");
        mwfl::Must(accelerators_.Create(commands_), "create accelerators");
        SetAccelerators(accelerators_.GetHandle());
    }
    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(save_)) { commands_.Find(kSave)->Invoke(); return mwfl::EventResult::Handled(); }
        return commands_.Dispatch(event);
    }
private:
    static constexpr mwfl::ControlId kSave{700};
    mwfl::CommandSet commands_; mwfl::AcceleratorTable accelerators_; mwfl::Button save_;
};

