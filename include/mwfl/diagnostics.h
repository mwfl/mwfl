#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <mwfl/core.h>
#include <optional>
#include <string>
#include <vector>
namespace mwfl {
enum class EventLevel { Trace, Information, Warning, Error, Critical };
enum class FieldSensitivity { Public, Sensitive, Secret };
struct EventField {
    std::wstring name;
    std::wstring value;
    FieldSensitivity sensitivity = FieldSensitivity::Public;
};
struct DiagnosticEvent {
    EventLevel level = EventLevel::Information;
    std::wstring category;
    std::uint32_t event_id = 0;
    std::vector<EventField> fields;
    std::chrono::system_clock::time_point timestamp{};
    DWORD process_id = 0;
    DWORD thread_id = 0;
    std::wstring correlation_id;
};
struct SanitizedDiagnosticEvent {
    EventLevel level = EventLevel::Information;
    std::wstring category;
    std::uint32_t event_id = 0;
    std::vector<EventField> fields;
    std::chrono::system_clock::time_point timestamp{};
    DWORD process_id = 0;
    DWORD thread_id = 0;
    std::wstring correlation_id;
};
class DiagnosticEventBuilder final {
   public:
    DiagnosticEventBuilder(EventLevel level, std::wstring category, std::uint32_t event_id);
    DiagnosticEventBuilder& Public(std::wstring name, std::wstring value);
    DiagnosticEventBuilder& Sensitive(std::wstring name);
    DiagnosticEventBuilder& Secret(std::wstring name);
    DiagnosticEventBuilder& Correlation(std::wstring value);
    [[nodiscard]] DiagnosticEvent Build();
   private:
    DiagnosticEvent event_;
};
struct DiagnosticWriteReport {
    std::size_t succeeded = 0;
    std::size_t failed = 0;
    std::optional<SystemError> first_error;
};
class DiagnosticSink {
   public:
    virtual ~DiagnosticSink() = default;
    virtual Result<void> Write(const SanitizedDiagnosticEvent& event) noexcept = 0;
};
class DebugOutputSink final : public DiagnosticSink {
   public:
    Result<void> Write(const SanitizedDiagnosticEvent& event) noexcept override;
};
class BoundedFileSink final : public DiagnosticSink {
   public:
    BoundedFileSink(std::filesystem::path path, std::uintmax_t maximum_bytes,
                    std::size_t rotation_count = 1);
    Result<void> Write(const SanitizedDiagnosticEvent& event) noexcept override;

   private:
    std::filesystem::path path_;
    std::uintmax_t maximum_bytes_;
    std::size_t rotation_count_;
    std::mutex mutex_;
};
class TraceLoggingSink final : public DiagnosticSink {
   public:
    Result<void> Write(const SanitizedDiagnosticEvent& event) noexcept override;
};
class EventLogSink final : public DiagnosticSink {
   public:
    explicit EventLogSink(std::wstring source) : source_(std::move(source)) {}
    Result<void> Write(const SanitizedDiagnosticEvent& event) noexcept override;

   private:
    std::wstring source_;
};
class EventLogSourceManager final {
   public:
    [[nodiscard]] static Result<bool> Exists(std::wstring_view source);
    [[nodiscard]] static Result<void> Install(std::wstring_view source,
                                              const std::filesystem::path& message_file);
    [[nodiscard]] static Result<bool> Remove(std::wstring_view source);
};
class DiagnosticPipeline final {
   public:
    void Add(std::shared_ptr<DiagnosticSink> sink);
    [[nodiscard]] DiagnosticWriteReport Write(const DiagnosticEvent& event) noexcept;

   private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<DiagnosticSink>> sinks_;
};
enum class MiniDumpKind { Normal, WithDataSegments, WithFullMemory };
[[nodiscard]] Result<void> WriteMiniDump(const std::filesystem::path& path, MiniDumpKind kind,
                                         EXCEPTION_POINTERS* exception = nullptr) noexcept;
class ScopedUnhandledExceptionDump final {
   public:
    ScopedUnhandledExceptionDump() = default;
    ~ScopedUnhandledExceptionDump();
    ScopedUnhandledExceptionDump(ScopedUnhandledExceptionDump&& other) noexcept;
    ScopedUnhandledExceptionDump& operator=(ScopedUnhandledExceptionDump&& other) noexcept;
    ScopedUnhandledExceptionDump(const ScopedUnhandledExceptionDump&) = delete;
    ScopedUnhandledExceptionDump& operator=(const ScopedUnhandledExceptionDump&) = delete;
    [[nodiscard]] static Result<ScopedUnhandledExceptionDump> Install(std::filesystem::path path,
                                                                      MiniDumpKind kind);
    [[nodiscard]] bool Active() const noexcept { return active_; }

   private:
    explicit ScopedUnhandledExceptionDump(bool active) : active_(active) {}
    bool active_ = false;
};
[[nodiscard]] SanitizedDiagnosticEvent SanitizeDiagnosticEvent(const DiagnosticEvent& event);
[[nodiscard]] std::wstring FormatDiagnosticEvent(const SanitizedDiagnosticEvent& event);
}  // namespace mwfl
