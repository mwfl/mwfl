#pragma once

#include <windows.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace mwfl {

enum class ActivationStatus { delivered, receiver_not_found, timed_out, access_denied, io_error };

struct ActivationResult {
    ActivationStatus status = ActivationStatus::io_error;
    DWORD native_error{};
    bool Delivered() const noexcept { return status == ActivationStatus::delivered; }
};

class SingleInstance final {
public:
    explicit SingleInstance(std::wstring_view application_id);
    ~SingleInstance() noexcept;
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool IsPrimary() const noexcept { return primary_; }
    bool RegisterWindow(HWND window) noexcept;
    void UnregisterWindow() noexcept;
    ActivationResult ForwardActivation(
        std::wstring_view payload,
        std::chrono::milliseconds timeout = std::chrono::seconds(2)) const noexcept;
    std::optional<std::wstring> DecodeActivation(
        UINT message, LPARAM lparam) const;

private:
    HANDLE mutex_{};
    HWND window_{};
    std::wstring property_name_;
    ULONG_PTR payload_tag_{};
    bool primary_{};
};

}  // namespace mwfl
