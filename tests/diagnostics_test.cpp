#include <mwfl/diagnostics.h>
int main() {
    const auto raw = mwfl::DiagnosticEventBuilder(mwfl::EventLevel::Error, L"test", 42)
                         .Public(L"public", L"yes")
                         .Secret(L"token")
                         .Build();
    const auto text = mwfl::FormatDiagnosticEvent(mwfl::SanitizeDiagnosticEvent(raw));
    if (text.find(L"yes") == std::wstring::npos || text.find(L"hidden") != std::wstring::npos ||
        text.find(L"<secret>") == std::wstring::npos)
        return 1;
    struct FailingSink final : mwfl::DiagnosticSink {
        mwfl::Result<void> Write(const mwfl::SanitizedDiagnosticEvent&) noexcept override {
            return mwfl::SystemError::FromWin32(ERROR_WRITE_FAULT);
        }
    };
    struct InspectingSink final : mwfl::DiagnosticSink {
        bool redacted = false;
        mwfl::Result<void> Write(const mwfl::SanitizedDiagnosticEvent& event) noexcept override {
            redacted = event.fields.size() == 2 && event.fields[1].value == L"<secret>";
            return {};
        }
    };
    mwfl::DiagnosticPipeline pipeline;
    auto inspecting = std::make_shared<InspectingSink>();
    pipeline.Add(inspecting);
    pipeline.Add(std::make_shared<mwfl::DebugOutputSink>());
    pipeline.Add(std::make_shared<FailingSink>());
    const auto report = pipeline.Write(
        {mwfl::EventLevel::Information,
         L"fanout",
         1,
         {{L"public", L"yes"}, {L"token", L"sentinel", mwfl::FieldSensitivity::Secret}}});
    return report.succeeded == 2 && report.failed == 1 && report.first_error.has_value() &&
                   inspecting->redacted
               ? 0
               : 2;
}
