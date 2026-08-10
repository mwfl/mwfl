#include <mwfl/mwfl.h>
#include <mwfl/scintilla.h>

class ScintillaFixture final : public mwfl::WindowBase {
public:
    bool CreateEditor(HWND parent) {
        return runtime_.LoadAdjacent() &&
            editor_.Create(parent, {700}, {}, runtime_) &&
            editor_.ConfigureCodeEditing() && editor_.SetText(L"// 你好\n");
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (!event.IsFrom(editor_)) return mwfl::EventResult::Propagate();
        const auto notification = editor_.DecodeNotification(event.header);
        if (!notification) return mwfl::EventResult::Propagate();
        if (notification->kind == mwfl::ScintillaNotificationKind::save_point_left)
            document_.MarkChanged();
        else if (notification->kind == mwfl::ScintillaNotificationKind::save_point_reached)
            document_.MarkSaved();
        else if (notification->kind == mwfl::ScintillaNotificationKind::modified &&
                 notification->lines_added != 0)
            static_cast<void>(editor_.UpdateLineNumberMargin());
        return mwfl::EventResult::Propagate();
    }

    void FindUnicode() {
        const std::optional<mwfl::ScintillaTextRange> bytes = editor_.Find(L"你好");
        if (bytes) static_cast<void>(editor_.SetSelection(*bytes));
    }

private:
    mwfl::ScintillaRuntime runtime_;
    mwfl::ScintillaEditor editor_;
    mwfl::DocumentState document_;
};
