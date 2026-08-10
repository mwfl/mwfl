#include <mwfl/mwfl.h>

using mwfl::operator""_dip;

namespace {
class DocumentStateWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        mwfl::TextBoxOptions editor_options;
        editor_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        ui.Add(editor_, L"Edit this document to make it dirty.", editor_options);
        ui.Add(save_, L"Mark saved");
        ui.Add(reset_, L"New document");
        ui.Add(status_, L"Untitled — clean");
        SetLayout(mwfl::Column().Margin(20.0_dip).Gap(10.0_dip)
            .Add(editor_, mwfl::Stretch())
            .Add(mwfl::Row().Gap(10.0_dip)
                .Add(save_, mwfl::Fixed(120.0_dip))
                .Add(reset_, mwfl::Fixed(140.0_dip))
                .Add(status_, mwfl::Stretch()), mwfl::Fixed(34.0_dip)));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.Is(editor_, EN_CHANGE) && !updating_) {
            document_.MarkChanged();
            SyncStatus();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(save_)) {
            document_.MarkSaved();
            SyncStatus();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(reset_)) {
            if (!MayDiscardChanges()) return mwfl::EventResult::Handled();
            updating_ = true;
            editor_.SetText(L"");
            updating_ = false;
            document_.ResetUntitled();
            SyncStatus();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnClose() override {
        return MayDiscardChanges() ? mwfl::EventResult::Propagate()
                                   : mwfl::EventResult::Handled();
    }

private:
    bool MayDiscardChanges() const {
        if (document_.EvaluateTransition() == mwfl::DocumentTransition::proceed) {
            return true;
        }
        const int answer = ::MessageBoxW(GetHwnd(),
            L"Discard the unsaved changes?", L"Document state",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
        const auto choice = answer == IDYES ? mwfl::UnsavedChangesChoice::discard
                                            : mwfl::UnsavedChangesChoice::cancel;
        return document_.EvaluateTransition(choice) == mwfl::DocumentTransition::proceed;
    }

    void SyncStatus() {
        std::wstring text{document_.GetDisplayName()};
        text += document_.IsDirty() ? L" — modified" : L" — clean";
        status_.SetText(text);
    }

    mwfl::DocumentState document_;
    bool updating_{};
    mwfl::TextBox editor_;
    mwfl::Button save_, reset_;
    mwfl::Label status_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<DocumentStateWindow>(instance, show,
        {.title = L"Document state", .initial_bounds = {{}, {760.0_dip, 480.0_dip}},
         .use_default_bounds = false});
}
