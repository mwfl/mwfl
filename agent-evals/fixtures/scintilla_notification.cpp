#include <mwtl/mwtl.h>
#include <mwtl/scintilla.h>

class ScintillaFixture final : public mwtl::WindowBase {
public:
    bool CreateEditor(HWND parent) {
        return runtime_.LoadAdjacent() &&
            editor_.Create(parent, {700}, {}, runtime_) &&
            editor_.ConfigureCodeEditing() && editor_.SetText(L"// 你好\n");
    }

    mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
        if (!event.IsFrom(editor_)) return mwtl::EventResult::Propagate();
        const auto notification = editor_.DecodeNotification(event.header);
        if (!notification) return mwtl::EventResult::Propagate();
        if (notification->kind == mwtl::ScintillaNotificationKind::save_point_left)
            document_.MarkChanged();
        else if (notification->kind == mwtl::ScintillaNotificationKind::save_point_reached)
            document_.MarkSaved();
        else if (notification->kind == mwtl::ScintillaNotificationKind::modified &&
                 notification->lines_added != 0)
            static_cast<void>(editor_.UpdateLineNumberMargin());
        return mwtl::EventResult::Propagate();
    }

    void FindUnicode() {
        const std::optional<mwtl::ScintillaTextRange> bytes = editor_.Find(L"你好");
        if (bytes) static_cast<void>(editor_.SetSelection(*bytes));
    }

private:
    mwtl::ScintillaRuntime runtime_;
    mwtl::ScintillaEditor editor_;
    mwtl::DocumentState document_;
};
