#include <mwtl/mwtl.h>

using mwtl::operator""_dip;

namespace {
class DocumentStateWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this};
        mwtl::TextBoxOptions editor_options;
        editor_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        ui.Add(editor_, L"Edit this document to make it dirty.", editor_options);
        ui.Add(save_, L"Mark saved");
        ui.Add(reset_, L"New document");
        ui.Add(status_, L"Untitled — clean");
        SetLayout(mwtl::Column().Margin(20.0_dip).Gap(10.0_dip)
            .Add(editor_, mwtl::Stretch())
            .Add(mwtl::Row().Gap(10.0_dip)
                .Add(save_, mwtl::Fixed(120.0_dip))
                .Add(reset_, mwtl::Fixed(140.0_dip))
                .Add(status_, mwtl::Stretch()), mwtl::Fixed(34.0_dip)));
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.Is(editor_, EN_CHANGE) && !updating_) {
            document_.MarkChanged();
            SyncStatus();
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(save_)) {
            document_.MarkSaved();
            SyncStatus();
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(reset_)) {
            if (!MayDiscardChanges()) return mwtl::EventResult::Handled();
            updating_ = true;
            editor_.SetText(L"");
            updating_ = false;
            document_.ResetUntitled();
            SyncStatus();
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnClose() override {
        return MayDiscardChanges() ? mwtl::EventResult::Propagate()
                                   : mwtl::EventResult::Handled();
    }

private:
    bool MayDiscardChanges() const {
        if (document_.EvaluateTransition() == mwtl::DocumentTransition::proceed) {
            return true;
        }
        const int answer = ::MessageBoxW(GetHwnd(),
            L"Discard the unsaved changes?", L"Document state",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
        const auto choice = answer == IDYES ? mwtl::UnsavedChangesChoice::discard
                                            : mwtl::UnsavedChangesChoice::cancel;
        return document_.EvaluateTransition(choice) == mwtl::DocumentTransition::proceed;
    }

    void SyncStatus() {
        std::wstring text{document_.GetDisplayName()};
        text += document_.IsDirty() ? L" — modified" : L" — clean";
        status_.SetText(text);
    }

    mwtl::DocumentState document_;
    bool updating_{};
    mwtl::TextBox editor_;
    mwtl::Button save_, reset_;
    mwtl::Label status_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwtl::RunApplication<DocumentStateWindow>(instance, show,
        {.title = L"Document state", .initial_bounds = {{}, {760.0_dip, 480.0_dip}},
         .use_default_bounds = false});
}
