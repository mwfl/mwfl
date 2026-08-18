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
enum class ErrorDomain { Win32, HResult, Application };
struct NativeError {
    ErrorDomain domain = ErrorDomain::Win32;
    std::uint32_t code = ERROR_SUCCESS;
    std::wstring operation;
    [[nodiscard]] static NativeError FromWin32(DWORD value) noexcept;
    [[nodiscard]] static NativeError LastWin32() noexcept;
    [[nodiscard]] static NativeError FromHResult(HRESULT value) noexcept;
    [[nodiscard]] NativeError WithOperation(std::wstring value) const;
    [[nodiscard]] std::wstring Message() const;
    [[nodiscard]] bool IsSuccess() const noexcept { return code == 0; }
    friend bool operator==(const NativeError&, const NativeError&) = default;
};
template <typename T>
class Result final {
   public:
    Result(T value) : storage_(std::move(value)) {}
    Result(NativeError error) : storage_(error) {}
    [[nodiscard]] bool HasValue() const noexcept { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const noexcept { return HasValue(); }
    [[nodiscard]] T& Value() & { return std::get<T>(storage_); }
    [[nodiscard]] const T& Value() const& { return std::get<T>(storage_); }
    [[nodiscard]] T&& Value() && { return std::get<T>(std::move(storage_)); }
    [[nodiscard]] NativeError Error() const { return std::get<NativeError>(storage_); }

   private:
    std::variant<T, NativeError> storage_;
};
template <>
class Result<void> final {
   public:
    Result() = default;
    Result(NativeError error) : error_(error) {}
    [[nodiscard]] bool HasValue() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return HasValue(); }
    [[nodiscard]] NativeError Error() const { return error_.value(); }

   private:
    std::optional<NativeError> error_;
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
enum class WaitStatus { Signaled, Timeout, Cancelled, Abandoned };
struct WaitResult {
    WaitStatus status = WaitStatus::Timeout;
};
[[nodiscard]] Result<WaitResult> WaitForHandle(HANDLE handle, std::chrono::milliseconds timeout,
                                               std::stop_token stop = {});
[[nodiscard]] Result<std::wstring> Utf8ToWide(std::string_view text);
[[nodiscard]] Result<std::string> WideToUtf8(std::wstring_view text);
}  // namespace mwfl
