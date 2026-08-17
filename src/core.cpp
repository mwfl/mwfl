#include <mwfl/core.h>
#include <algorithm>
#include <array>
#include <limits>
namespace mwfl { namespace {
std::wstring FormatSystemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) return L"Windows error " + std::to_wstring(code);
    std::wstring message(buffer, length); LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) message.pop_back();
    return message;
}
DWORD BoundedTimeout(std::chrono::milliseconds timeout) noexcept {
    if (timeout.count() < 0) return INFINITE;
    return static_cast<DWORD>((std::min)(timeout.count(), static_cast<long long>(INFINITE - 1)));
}
}  // namespace
NativeError NativeError::FromWin32(DWORD value) noexcept { return {ErrorDomain::Win32, value == ERROR_SUCCESS ? ERROR_GEN_FAILURE : value}; }
NativeError NativeError::LastWin32() noexcept { return FromWin32(GetLastError()); }
NativeError NativeError::FromHResult(HRESULT value) noexcept { return {ErrorDomain::HResult, static_cast<std::uint32_t>(value)}; }
std::wstring NativeError::Message() const { return domain == ErrorDomain::Application ? L"Application error " + std::to_wstring(code) : FormatSystemMessage(code); }
Result<WaitResult> WaitForHandle(HANDLE handle, std::chrono::milliseconds timeout, std::stop_token stop) {
    if (!KernelHandlePolicy::IsValid(handle)) return NativeError::FromWin32(ERROR_INVALID_HANDLE);
    if (stop.stop_requested()) return WaitResult{WaitStatus::Cancelled};
    KernelHandle cancelled(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!cancelled) return NativeError::LastWin32();
    std::stop_callback callback(stop, [event = cancelled.Get()] { SetEvent(event); });
    const std::array<HANDLE, 2> handles{handle, cancelled.Get()};
    const DWORD result = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, BoundedTimeout(timeout));
    if (result == WAIT_OBJECT_0) return WaitResult{WaitStatus::Signaled};
    if (result == WAIT_OBJECT_0 + 1) return WaitResult{WaitStatus::Cancelled};
    if (result == WAIT_TIMEOUT) return WaitResult{WaitStatus::Timeout};
    if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + handles.size()) return WaitResult{WaitStatus::Abandoned};
    return NativeError::LastWin32();
}
Result<std::wstring> Utf8ToWide(std::string_view text) {
    if (text.empty()) return std::wstring{};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size == 0) return NativeError::LastWin32();
    std::wstring converted(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), converted.data(), size) == 0) return NativeError::LastWin32();
    return converted;
}
Result<std::string> WideToUtf8(std::wstring_view text) {
    if (text.empty()) return std::string{};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size == 0) return NativeError::LastWin32();
    std::string converted(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), converted.data(), size, nullptr, nullptr) == 0) return NativeError::LastWin32();
    return converted;
}
}  // namespace mwfl
