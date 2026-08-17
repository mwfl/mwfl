#pragma once
#include <mwfl/core.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>
namespace mwfl {
enum class EventLevel { Trace, Information, Warning, Error, Critical };
enum class FieldSensitivity { Public, Sensitive, Secret };
struct EventField { std::wstring name; std::wstring value; FieldSensitivity sensitivity = FieldSensitivity::Public; };
struct DiagnosticEvent { EventLevel level = EventLevel::Information; std::wstring category; std::uint32_t event_id = 0; std::vector<EventField> fields; };
class DiagnosticSink {
public:
    virtual ~DiagnosticSink() = default;
    virtual Result<void> Write(const DiagnosticEvent& event) noexcept = 0;
};
class DebugOutputSink final : public DiagnosticSink { public: Result<void> Write(const DiagnosticEvent& event) noexcept override; };
class BoundedFileSink final : public DiagnosticSink {
public:
    BoundedFileSink(std::filesystem::path path, std::uintmax_t maximum_bytes);
    Result<void> Write(const DiagnosticEvent& event) noexcept override;
private:
    std::filesystem::path path_; std::uintmax_t maximum_bytes_;
};
class DiagnosticPipeline final {
public:
    void Add(std::shared_ptr<DiagnosticSink> sink);
    [[nodiscard]] Result<void> Write(const DiagnosticEvent& event) noexcept;
private:
    std::vector<std::shared_ptr<DiagnosticSink>> sinks_;
};
[[nodiscard]] std::wstring FormatDiagnosticEvent(const DiagnosticEvent& event);
}  // namespace mwfl
