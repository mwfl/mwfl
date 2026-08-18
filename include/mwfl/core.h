#pragma once
#include <windows.h>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
namespace mwfl {
enum class ErrorDomain { Win32, HResult, Application, Protocol, Policy, InvalidUsage };
struct SystemError {
    ErrorDomain domain = ErrorDomain::Win32;
    std::uint32_t code = ERROR_SUCCESS;
    std::wstring operation;
    std::wstring detail;
    [[nodiscard]] static SystemError FromWin32(DWORD value) noexcept;
    [[nodiscard]] static SystemError LastWin32() noexcept;
    [[nodiscard]] static SystemError FromHResult(HRESULT value) noexcept;
    [[nodiscard]] static SystemError Application(std::uint32_t code, std::wstring detail = {});
    [[nodiscard]] static SystemError Protocol(std::uint32_t code, std::wstring detail = {});
    [[nodiscard]] static SystemError Policy(std::uint32_t code, std::wstring detail = {});
    [[nodiscard]] static SystemError InvalidUsage(std::uint32_t code, std::wstring detail = {});
    [[nodiscard]] SystemError WithOperation(std::wstring value) const;
    [[nodiscard]] std::wstring Message() const;
    [[nodiscard]] bool IsSuccess() const noexcept { return code == 0; }
    friend bool operator==(const SystemError&, const SystemError&) = default;
};
template <typename T>
class Result final {
   public:
    Result(T value) : storage_(std::move(value)) {}
    Result(mwfl::SystemError error) : storage_(std::move(error)) {}
    [[nodiscard]] bool HasValue() const noexcept { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const noexcept { return HasValue(); }
    [[nodiscard]] T& Value() & { return std::get<T>(storage_); }
    [[nodiscard]] const T& Value() const& { return std::get<T>(storage_); }
    [[nodiscard]] T&& Value() && { return std::get<T>(std::move(storage_)); }
    [[nodiscard]] const mwfl::SystemError& GetError() const& {
        return std::get<mwfl::SystemError>(storage_);
    }
    [[nodiscard]] mwfl::SystemError&& GetError() && {
        return std::get<mwfl::SystemError>(std::move(storage_));
    }

   private:
    std::variant<T, mwfl::SystemError> storage_;
};
template <>
class Result<void> final {
   public:
    Result() = default;
    Result(mwfl::SystemError error) : error_(std::move(error)) {}
    [[nodiscard]] bool HasValue() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return HasValue(); }
    [[nodiscard]] const mwfl::SystemError& GetError() const& { return error_.value(); }
    [[nodiscard]] mwfl::SystemError&& GetError() && { return std::move(error_.value()); }

   private:
    std::optional<mwfl::SystemError> error_;
};

class Deadline final {
   public:
    [[nodiscard]] static Deadline After(std::chrono::milliseconds duration) noexcept;
    [[nodiscard]] static Deadline Infinite() noexcept;
    [[nodiscard]] bool IsInfinite() const noexcept { return infinite_; }
    [[nodiscard]] bool Expired() const noexcept;
    [[nodiscard]] std::chrono::milliseconds Remaining() const noexcept;

   private:
    explicit Deadline(std::chrono::steady_clock::time_point end, bool infinite) noexcept
        : end_(end), infinite_(infinite) {}
    std::chrono::steady_clock::time_point end_{};
    bool infinite_ = false;
};

enum class CompletionStatus { Completed, TimedOut, Cancelled, Disconnected };
template <typename T>
struct OperationOutcome final {
    CompletionStatus status = CompletionStatus::Completed;
    std::optional<T> value;
    [[nodiscard]] static OperationOutcome Completed(T value) {
        return {CompletionStatus::Completed, std::move(value)};
    }
    [[nodiscard]] static OperationOutcome Control(CompletionStatus status) { return {status, {}}; }
};
template <>
struct OperationOutcome<void> final {
    CompletionStatus status = CompletionStatus::Completed;
    [[nodiscard]] static OperationOutcome Completed() { return {}; }
    [[nodiscard]] static OperationOutcome Control(CompletionStatus status) { return {status}; }
};
template <typename ClosePolicy>
class UniqueHandle final {
   public:
    using handle_type = typename ClosePolicy::handle_type;
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(handle_type handle) noexcept : handle_(handle) {}
    ~UniqueHandle() { Reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.Release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) Reset(other.Release());
        return *this;
    }
    [[nodiscard]] handle_type Get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ClosePolicy::IsValid(handle_); }
    [[nodiscard]] handle_type Release() noexcept {
        return std::exchange(handle_, ClosePolicy::Invalid());
    }
    void Reset(handle_type replacement = ClosePolicy::Invalid()) noexcept {
        if (ClosePolicy::IsValid(handle_)) ClosePolicy::Close(handle_);
        handle_ = replacement;
    }

   private:
    handle_type handle_ = ClosePolicy::Invalid();
};
struct KernelHandlePolicy {
    using handle_type = HANDLE;
    [[nodiscard]] static HANDLE Invalid() noexcept { return nullptr; }
    [[nodiscard]] static bool IsValid(HANDLE handle) noexcept {
        return handle != nullptr && handle != INVALID_HANDLE_VALUE;
    }
    static void Close(HANDLE handle) noexcept { CloseHandle(handle); }
};
using KernelHandle = UniqueHandle<KernelHandlePolicy>;
enum class WaitObservation { Signaled, Abandoned };
[[nodiscard]] Result<OperationOutcome<WaitObservation>> WaitForHandle(
    HANDLE handle, Deadline deadline, std::stop_token stop = {});
[[nodiscard]] Result<std::wstring> Utf8ToWide(std::string_view text);
[[nodiscard]] Result<std::string> WideToUtf8(std::wstring_view text);
}  // namespace mwfl
