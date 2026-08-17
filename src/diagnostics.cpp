#include <mwfl/diagnostics.h>
#include <fstream>
namespace mwfl { namespace {
std::wstring Level(EventLevel level) { switch (level) { case EventLevel::Trace:return L"trace"; case EventLevel::Information:return L"info"; case EventLevel::Warning:return L"warning"; case EventLevel::Error:return L"error"; case EventLevel::Critical:return L"critical"; } return L"unknown"; }
}  // namespace
std::wstring FormatDiagnosticEvent(const DiagnosticEvent& event) {
    std::wstring text = L"[" + Level(event.level) + L"] " + event.category + L"/" + std::to_wstring(event.event_id);
    for (const auto& field : event.fields) { text += L" "; text += field.name; text += L"="; text += field.sensitivity == FieldSensitivity::Public ? field.value : field.sensitivity == FieldSensitivity::Sensitive ? L"<redacted>" : L"<secret>"; }
    return text;
}
Result<void> DebugOutputSink::Write(const DiagnosticEvent& event) noexcept { try { auto line = FormatDiagnosticEvent(event) + L"\n"; OutputDebugStringW(line.c_str()); return {}; } catch (...) { return NativeError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION}; } }
BoundedFileSink::BoundedFileSink(std::filesystem::path path, std::uintmax_t maximum_bytes) : path_(std::move(path)), maximum_bytes_(maximum_bytes) {}
Result<void> BoundedFileSink::Write(const DiagnosticEvent& event) noexcept {
    try {
        if (path_.empty() || maximum_bytes_ == 0) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
        std::error_code error; const auto current = std::filesystem::file_size(path_, error);
        if (!error && current >= maximum_bytes_) { auto rotated = path_; rotated += L".1"; std::filesystem::remove(rotated, error); error.clear(); std::filesystem::rename(path_, rotated, error); if (error) return NativeError::FromWin32(static_cast<DWORD>(error.value())); }
        auto utf8 = WideToUtf8(FormatDiagnosticEvent(event)); if (!utf8) return utf8.Error();
        std::ofstream output(path_, std::ios::binary | std::ios::app); if (!output) return NativeError::FromWin32(ERROR_OPEN_FAILED);
        output << utf8.Value() << '\n'; if (!output) return NativeError::FromWin32(ERROR_WRITE_FAULT); return {};
    } catch (...) { return NativeError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION}; }
}
void DiagnosticPipeline::Add(std::shared_ptr<DiagnosticSink> sink) { if (sink) sinks_.push_back(std::move(sink)); }
Result<void> DiagnosticPipeline::Write(const DiagnosticEvent& event) noexcept { for (const auto& sink : sinks_) { auto result = sink->Write(event); if (!result) return result.Error(); } return {}; }
}  // namespace mwfl
