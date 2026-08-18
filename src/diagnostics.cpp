#include <mwfl/diagnostics.h>
#include <TraceLoggingProvider.h>
#include <dbghelp.h>
#include <fstream>
#include <iomanip>
#include <sstream>
TRACELOGGING_DEFINE_PROVIDER(g_mwfl_provider, "mwfl.Foundation",
                             (0x46d29a71, 0x72c0, 0x4f9d, 0x93, 0xa1, 0xc8, 0xe8, 0x19, 0x21, 0x3b,
                              0x54));
namespace mwfl {
namespace {
std::mutex provider_mutex;
std::mutex dump_mutex;
std::filesystem::path dump_path;
MiniDumpKind dump_kind = MiniDumpKind::Normal;
LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = nullptr;
bool dump_handler_active = false;
std::wstring Level(EventLevel level) {
    switch (level) {
        case EventLevel::Trace:
            return L"trace";
        case EventLevel::Information:
            return L"info";
        case EventLevel::Warning:
            return L"warning";
        case EventLevel::Error:
            return L"error";
        case EventLevel::Critical:
            return L"critical";
    }
    return L"unknown";
}
WORD EventType(EventLevel level) {
    switch (level) {
        case EventLevel::Warning:
            return EVENTLOG_WARNING_TYPE;
        case EventLevel::Error:
        case EventLevel::Critical:
            return EVENTLOG_ERROR_TYPE;
        default:
            return EVENTLOG_INFORMATION_TYPE;
    }
}
MINIDUMP_TYPE DumpType(MiniDumpKind kind) {
    switch (kind) {
        case MiniDumpKind::WithDataSegments:
            return MiniDumpWithDataSegs;
        case MiniDumpKind::WithFullMemory:
            return MiniDumpWithFullMemory;
        default:
            return MiniDumpNormal;
    }
}
LONG WINAPI DumpFilter(EXCEPTION_POINTERS* exception) noexcept {
    LPTOP_LEVEL_EXCEPTION_FILTER previous = nullptr;
    {
        std::scoped_lock lock(dump_mutex);
        if (dump_handler_active) (void)WriteMiniDump(dump_path, dump_kind, exception);
        previous = previous_filter;
    }
    return previous ? previous(exception) : EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace
DiagnosticEventBuilder::DiagnosticEventBuilder(EventLevel level, std::wstring category,
                                               std::uint32_t event_id) {
    event_.level = level;
    event_.category = std::move(category);
    event_.event_id = event_id;
}
DiagnosticEventBuilder& DiagnosticEventBuilder::Public(std::wstring name, std::wstring value) {
    event_.fields.push_back({std::move(name), std::move(value), FieldSensitivity::Public});
    return *this;
}
DiagnosticEventBuilder& DiagnosticEventBuilder::Sensitive(std::wstring name) {
    event_.fields.push_back({std::move(name), {}, FieldSensitivity::Sensitive});
    return *this;
}
DiagnosticEventBuilder& DiagnosticEventBuilder::Secret(std::wstring name) {
    event_.fields.push_back({std::move(name), {}, FieldSensitivity::Secret});
    return *this;
}
DiagnosticEventBuilder& DiagnosticEventBuilder::Correlation(std::wstring value) {
    event_.correlation_id = std::move(value);
    return *this;
}
DiagnosticEvent DiagnosticEventBuilder::Build() { return std::move(event_); }
SanitizedDiagnosticEvent SanitizeDiagnosticEvent(const DiagnosticEvent& event) {
    SanitizedDiagnosticEvent copy{event.level, event.category, event.event_id, event.fields,
                                  event.timestamp, event.process_id, event.thread_id,
                                  event.correlation_id};
    if (copy.timestamp == std::chrono::system_clock::time_point{})
        copy.timestamp = std::chrono::system_clock::now();
    if (!copy.process_id) copy.process_id = GetCurrentProcessId();
    if (!copy.thread_id) copy.thread_id = GetCurrentThreadId();
    for (auto& field : copy.fields) {
        if (field.sensitivity == FieldSensitivity::Sensitive)
            field.value = L"<redacted>";
        else if (field.sensitivity == FieldSensitivity::Secret)
            field.value = L"<secret>";
        field.sensitivity = FieldSensitivity::Public;
    }
    return copy;
}
std::wstring FormatDiagnosticEvent(const SanitizedDiagnosticEvent& safe) {
    std::wstring text =
        L"[" + Level(safe.level) + L"] " + safe.category + L"/" + std::to_wstring(safe.event_id) +
        L" pid=" + std::to_wstring(safe.process_id) + L" tid=" + std::to_wstring(safe.thread_id);
    if (!safe.correlation_id.empty()) text += L" correlation=" + safe.correlation_id;
    for (const auto& field : safe.fields) text += L" " + field.name + L"=" + field.value;
    return text;
}
Result<void> DebugOutputSink::Write(const SanitizedDiagnosticEvent& event) noexcept {
    try {
        const auto line = FormatDiagnosticEvent(event) + L"\n";
        OutputDebugStringW(line.c_str());
        return {};
    } catch (...) {
        return SystemError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION, L"DebugOutputSink"};
    }
}
BoundedFileSink::BoundedFileSink(std::filesystem::path path, std::uintmax_t maximum,
                                 std::size_t rotations)
    : path_(std::move(path)), maximum_bytes_(maximum), rotation_count_(rotations) {}
Result<void> BoundedFileSink::Write(const SanitizedDiagnosticEvent& event) noexcept {
    try {
        std::scoped_lock lock(mutex_);
        if (path_.empty() || !maximum_bytes_)
            return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
        auto utf8 = WideToUtf8(FormatDiagnosticEvent(event) + L"\n");
        if (!utf8) return utf8.GetError();
        if (utf8.Value().size() > maximum_bytes_)
            return SystemError::FromWin32(ERROR_BUFFER_OVERFLOW)
                .WithOperation(L"Diagnostic record");
        std::error_code error;
        const auto current = std::filesystem::file_size(path_, error);
        if (!error && current + utf8.Value().size() > maximum_bytes_) {
            for (std::size_t index = rotation_count_; index > 0; --index) {
                auto from = path_;
                if (index > 1) from += L"." + std::to_wstring(index - 1);
                auto to = path_;
                to += L"." + std::to_wstring(index);
                std::filesystem::remove(to, error);
                error.clear();
                if (std::filesystem::exists(from, error)) {
                    error.clear();
                    std::filesystem::rename(from, to, error);
                    if (error)
                        return SystemError::FromWin32(static_cast<DWORD>(error.value()))
                            .WithOperation(L"Rotate diagnostic file");
                }
            }
        }
        std::ofstream output(path_, std::ios::binary | std::ios::app);
        if (!output) return SystemError::FromWin32(ERROR_OPEN_FAILED);
        output.write(utf8.Value().data(), static_cast<std::streamsize>(utf8.Value().size()));
        if (!output) return SystemError::FromWin32(ERROR_WRITE_FAULT);
        return {};
    } catch (...) {
        return SystemError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION, L"BoundedFileSink"};
    }
}
Result<void> TraceLoggingSink::Write(const SanitizedDiagnosticEvent& event) noexcept {
    try {
        std::scoped_lock lock(provider_mutex);
        const auto registered = TraceLoggingRegister(g_mwfl_provider);
        if (registered != ERROR_SUCCESS)
            return SystemError::FromWin32(registered).WithOperation(L"TraceLoggingRegister");
        auto utf8 = WideToUtf8(FormatDiagnosticEvent(event));
        if (!utf8) {
            TraceLoggingUnregister(g_mwfl_provider);
            return utf8.GetError();
        }
        TraceLoggingWrite(g_mwfl_provider, "DiagnosticEvent",
                          TraceLoggingString(utf8.Value().c_str(), "message"),
                          TraceLoggingUInt32(event.event_id, "event_id"));
        TraceLoggingUnregister(g_mwfl_provider);
        return {};
    } catch (...) {
        TraceLoggingUnregister(g_mwfl_provider);
        return SystemError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION,
                           L"TraceLoggingSink"};
    }
}
Result<void> EventLogSink::Write(const SanitizedDiagnosticEvent& event) noexcept {
    try {
        HANDLE source = RegisterEventSourceW(nullptr, source_.c_str());
        if (!source) return SystemError::LastWin32().WithOperation(L"RegisterEventSourceW");
        const std::wstring text = FormatDiagnosticEvent(event);
        const wchar_t* strings[]{text.c_str()};
        const BOOL written = ReportEventW(source, EventType(event.level), 0, event.event_id,
                                          nullptr, 1, 0, strings, nullptr);
        const DWORD error = written ? ERROR_SUCCESS : GetLastError();
        DeregisterEventSource(source);
        return written ? Result<void>{}
                       : Result<void>{SystemError::FromWin32(error).WithOperation(L"ReportEventW")};
    } catch (...) {
        return SystemError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION, L"EventLogSink"};
    }
}
Result<bool> EventLogSourceManager::Exists(std::wstring_view source) {
    HKEY key = nullptr;
    const std::wstring path =
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" + std::wstring(source);
    const LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &key);
    if (result == ERROR_FILE_NOT_FOUND) return false;
    if (result != ERROR_SUCCESS) return SystemError::FromWin32(result);
    RegCloseKey(key);
    return true;
}
Result<void> EventLogSourceManager::Install(std::wstring_view source,
                                            const std::filesystem::path& message_file) {
    if (source.empty() || message_file.empty())
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    const std::wstring path =
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" + std::wstring(source);
    HKEY key = nullptr;
    DWORD disposition = 0;
    LONG result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE,
                                  nullptr, &key, &disposition);
    if (result != ERROR_SUCCESS) return SystemError::FromWin32(result);
    const auto file = message_file.wstring();
    result = RegSetValueExW(key, L"EventMessageFile", 0, REG_EXPAND_SZ,
                            reinterpret_cast<const BYTE*>(file.c_str()),
                            static_cast<DWORD>((file.size() + 1) * sizeof(wchar_t)));
    const DWORD types = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    if (result == ERROR_SUCCESS)
        result = RegSetValueExW(key, L"TypesSupported", 0, REG_DWORD,
                                reinterpret_cast<const BYTE*>(&types), sizeof(types));
    RegCloseKey(key);
    return result == ERROR_SUCCESS ? Result<void>{} : Result<void>{SystemError::FromWin32(result)};
}
Result<bool> EventLogSourceManager::Remove(std::wstring_view source) {
    auto exists = Exists(source);
    if (!exists || !exists.Value()) return exists;
    const std::wstring path =
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" + std::wstring(source);
    const LONG result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, path.c_str());
    if (result == ERROR_FILE_NOT_FOUND) return false;
    if (result != ERROR_SUCCESS) return SystemError::FromWin32(result);
    return true;
}
void DiagnosticPipeline::Add(std::shared_ptr<DiagnosticSink> sink) {
    if (!sink) return;
    std::scoped_lock lock(mutex_);
    sinks_.push_back(std::move(sink));
}
DiagnosticWriteReport DiagnosticPipeline::Write(const DiagnosticEvent& event) noexcept {
    DiagnosticWriteReport report;
    try {
        std::vector<std::shared_ptr<DiagnosticSink>> sinks;
        {
            std::scoped_lock lock(mutex_);
            sinks = sinks_;
        }
        const auto redacted = SanitizeDiagnosticEvent(event);
        for (const auto& sink : sinks) {
            auto result = sink->Write(redacted);
            if (result)
                ++report.succeeded;
            else {
                ++report.failed;
                if (!report.first_error) report.first_error = result.GetError();
            }
        }
    } catch (...) {
        ++report.failed;
        report.first_error =
            SystemError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION, L"DiagnosticPipeline"};
    }
    return report;
}
Result<void> WriteMiniDump(const std::filesystem::path& path, MiniDumpKind kind,
                           EXCEPTION_POINTERS* exception) noexcept {
    try {
        if (path.empty() || std::filesystem::exists(path))
            return SystemError::FromWin32(path.empty() ? ERROR_INVALID_PARAMETER
                                                       : ERROR_FILE_EXISTS);
        KernelHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) return SystemError::LastWin32().WithOperation(L"Create minidump");
        MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
        if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file.Get(),
                               DumpType(kind), exception ? &info : nullptr, nullptr, nullptr))
            return SystemError::LastWin32().WithOperation(L"MiniDumpWriteDump");
        return {};
    } catch (...) {
        return SystemError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION, L"WriteMiniDump"};
    }
}
Result<ScopedUnhandledExceptionDump> ScopedUnhandledExceptionDump::Install(
    std::filesystem::path path, MiniDumpKind kind) {
    std::scoped_lock lock(dump_mutex);
    if (dump_handler_active || path.empty()) return SystemError::FromWin32(ERROR_ALREADY_EXISTS);
    dump_path = std::move(path);
    dump_kind = kind;
    previous_filter = SetUnhandledExceptionFilter(DumpFilter);
    dump_handler_active = true;
    return ScopedUnhandledExceptionDump(true);
}
ScopedUnhandledExceptionDump::~ScopedUnhandledExceptionDump() {
    if (active_) {
        std::scoped_lock lock(dump_mutex);
        SetUnhandledExceptionFilter(previous_filter);
        previous_filter = nullptr;
        dump_path.clear();
        dump_handler_active = false;
    }
}
ScopedUnhandledExceptionDump::ScopedUnhandledExceptionDump(
    ScopedUnhandledExceptionDump&& other) noexcept
    : active_(std::exchange(other.active_, false)) {}
ScopedUnhandledExceptionDump& ScopedUnhandledExceptionDump::operator=(
    ScopedUnhandledExceptionDump&& other) noexcept {
    if (this != &other) {
        if (active_) {
            std::scoped_lock lock(dump_mutex);
            SetUnhandledExceptionFilter(previous_filter);
            previous_filter = nullptr;
            dump_path.clear();
            dump_handler_active = false;
        }
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}
}  // namespace mwfl
