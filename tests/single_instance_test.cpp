#include <mwfl/single_instance.h>

#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <string>

namespace {
mwfl::SingleInstance* receiver_instance{};
std::wstring received;

LRESULT CALLBACK ReceiverProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (receiver_instance) {
        if (auto payload = receiver_instance->DecodeActivation(message, lparam)) {
            received = std::move(*payload);
            return TRUE;
        }
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
}
}  // namespace

int main() {
    using namespace std::chrono_literals;
    const std::wstring id = L"mwfl.single-instance-test." +
        std::to_wstring(::GetCurrentProcessId()) + L"." + std::to_wstring(::GetTickCount64());
    mwfl::SingleInstance primary{id};
    mwfl::SingleInstance secondary{id};
    if (!primary.IsPrimary() || secondary.IsPrimary()) return EXIT_FAILURE;
    if (secondary.ForwardActivation(L"before-window", 0ms).status !=
        mwfl::ActivationStatus::receiver_not_found) return EXIT_FAILURE;
    if (primary.ForwardActivation(L"invalid", 0ms).status !=
        mwfl::ActivationStatus::io_error) return EXIT_FAILURE;

    const std::wstring class_name = L"mwfl.SingleInstance.TestWindow." +
                                    std::to_wstring(::GetCurrentProcessId());
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = ReceiverProcedure;
    window_class.hInstance = ::GetModuleHandleW(nullptr);
    window_class.lpszClassName = class_name.c_str();
    if (!::RegisterClassW(&window_class) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return EXIT_FAILURE;
    HWND window = ::CreateWindowExW(0, class_name.c_str(), L"", WS_OVERLAPPED,
        0, 0, 0, 0, nullptr, nullptr, window_class.hInstance, nullptr);
    if (!window) return EXIT_FAILURE;
    receiver_instance = &primary;
    if (!primary.RegisterWindow(window)) return EXIT_FAILURE;

    const std::wstring payload = LR"(C:\Documents\hello world.txt)";
    if (!secondary.ForwardActivation(payload, 1s).Delivered() || received != payload)
        return EXIT_FAILURE;
    received = L"not empty";
    if (!secondary.ForwardActivation(L"", 1s).Delivered() || !received.empty())
        return EXIT_FAILURE;
    std::wstring oversized(32768, L'x');
    if (secondary.ForwardActivation(oversized, 1s).status != mwfl::ActivationStatus::io_error)
        return EXIT_FAILURE;

    COPYDATASTRUCT malformed{1, 1, const_cast<wchar_t*>(L"x")};
    if (primary.DecodeActivation(WM_COPYDATA, reinterpret_cast<LPARAM>(&malformed)))
        return EXIT_FAILURE;
    primary.UnregisterWindow();
    if (secondary.ForwardActivation(L"after-window", 0ms).status !=
        mwfl::ActivationStatus::receiver_not_found) return EXIT_FAILURE;
    receiver_instance = nullptr;
    ::DestroyWindow(window);
    ::UnregisterClassW(class_name.c_str(), window_class.hInstance);
    return EXIT_SUCCESS;
}
