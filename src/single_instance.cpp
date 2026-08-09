#include <mwtl/single_instance.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace mwtl {
namespace {
constexpr std::size_t kMaximumPayloadCharacters = 32767;

std::uint64_t Hash(std::wstring_view value) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (const wchar_t character : value) {
        hash ^= static_cast<std::uint16_t>(character);
        hash *= 1099511628211ull;
    }
    return hash;
}

struct FindWindowContext {
    const wchar_t* property{};
    HWND found{};
};

BOOL CALLBACK FindRegisteredWindow(HWND window, LPARAM parameter) noexcept {
    auto& context = *reinterpret_cast<FindWindowContext*>(parameter);
    if (::GetPropW(window, context.property)) {
        context.found = window;
        return FALSE;
    }
    return TRUE;
}
}  // namespace

SingleInstance::SingleInstance(std::wstring_view application_id) {
    if (application_id.empty()) throw std::invalid_argument("single-instance application ID must not be empty");
    const auto hash = Hash(application_id);
    const std::wstring suffix = std::to_wstring(hash);
    property_name_ = L"mwtl.SingleInstance.Window." + suffix;
    payload_tag_ = static_cast<ULONG_PTR>(hash == 0 ? 1 : hash);
    const std::wstring mutex_name = L"Local\\mwtl.SingleInstance.Mutex." + suffix;
    mutex_ = ::CreateMutexW(nullptr, FALSE, mutex_name.c_str());
    if (!mutex_) throw std::system_error(static_cast<int>(::GetLastError()),
                                         std::system_category(), "CreateMutexW");
    primary_ = ::GetLastError() != ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance() noexcept {
    UnregisterWindow();
    if (mutex_) ::CloseHandle(mutex_);
}

bool SingleInstance::RegisterWindow(HWND window) noexcept {
    if (!primary_ || !::IsWindow(window)) return false;
    UnregisterWindow();
    if (!::SetPropW(window, property_name_.c_str(), reinterpret_cast<HANDLE>(payload_tag_)))
        return false;
    window_ = window;
    return true;
}

void SingleInstance::UnregisterWindow() noexcept {
    if (window_ && ::IsWindow(window_)) ::RemovePropW(window_, property_name_.c_str());
    window_ = nullptr;
}

ActivationResult SingleInstance::ForwardActivation(
    std::wstring_view payload, std::chrono::milliseconds timeout) const noexcept {
    if (primary_ || payload.size() > kMaximumPayloadCharacters)
        return {.status = ActivationStatus::io_error, .native_error = ERROR_INVALID_PARAMETER};
    const auto deadline = std::chrono::steady_clock::now() + (std::max)(timeout, std::chrono::milliseconds::zero());
    do {
        FindWindowContext context{property_name_.c_str()};
        ::EnumWindows(FindRegisteredWindow, reinterpret_cast<LPARAM>(&context));
        if (context.found) {
            const std::wstring stable{payload};
            COPYDATASTRUCT data{payload_tag_,
                static_cast<DWORD>((stable.size() + 1) * sizeof(wchar_t)),
                const_cast<wchar_t*>(stable.c_str())};
            DWORD_PTR receiver_result{};
            ::SetLastError(ERROR_SUCCESS);
            if (::SendMessageTimeoutW(context.found, WM_COPYDATA, 0,
                    reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG | SMTO_BLOCK,
                    static_cast<UINT>((std::max)(timeout.count(), 1ll)), &receiver_result)) {
                return receiver_result != 0
                    ? ActivationResult{.status = ActivationStatus::delivered}
                    : ActivationResult{.status = ActivationStatus::io_error, .native_error = ERROR_INVALID_DATA};
            }
            const DWORD error = ::GetLastError();
            if (error == ERROR_TIMEOUT) return {.status = ActivationStatus::timed_out, .native_error = error};
            if (error == ERROR_ACCESS_DENIED) return {.status = ActivationStatus::access_denied, .native_error = error};
            return {.status = ActivationStatus::io_error, .native_error = error};
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    } while (true);
    return {.status = ActivationStatus::receiver_not_found, .native_error = ERROR_NOT_FOUND};
}

std::optional<std::wstring> SingleInstance::DecodeActivation(
    UINT message, LPARAM lparam) const {
    if (!primary_ || message != WM_COPYDATA || lparam == 0) return std::nullopt;
    const auto& data = *reinterpret_cast<const COPYDATASTRUCT*>(lparam);
    if (data.dwData != payload_tag_ || !data.lpData || data.cbData < sizeof(wchar_t) ||
        data.cbData % sizeof(wchar_t) != 0 ||
        data.cbData > (kMaximumPayloadCharacters + 1) * sizeof(wchar_t)) return std::nullopt;
    const std::size_t count = data.cbData / sizeof(wchar_t);
    const auto* text = static_cast<const wchar_t*>(data.lpData);
    if (text[count - 1] != L'\0') return std::nullopt;
    return std::wstring{text, count - 1};
}

}  // namespace mwtl
