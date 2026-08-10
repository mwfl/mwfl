#include <mwfl/mwfl.h>

#include "detail/testing.h"

#include <objbase.h>

#include <atomic>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ranges>
#include <span>
#include <stdexcept>
#include <thread>

namespace {

using mwfl::operator""_dip;

enum class Mode {
    custom_window,
    wait_handle,
    wakeup,
    wake_stress,
    wake_latency,
    idle_efficiency,
    com_sta,
    ole_sta,
    com_conflict,
    pump_exception,
    after_dispatch_exception,
    wait_callback_exception,
    wait_failure,
    exact_exit,
    dpi_change,
};

Mode mode = Mode::custom_window;
HWND test_window = nullptr;
HANDLE wait_event = nullptr;
mwfl::WindowWakeup wakeup;
std::thread producer;
std::atomic<bool> custom_verified{false};
std::atomic<bool> wake_posted{false};
std::atomic<bool> com_verified{false};
std::atomic<unsigned> wake_count{0};
std::atomic<long long> wake_posted_ns{0};
std::array<long long, 500> wake_latency_us{};
unsigned idle_count = 0;
bool wait_handle_closed = false;
RECT dpi_suggested{40, 50, 840, 650};
std::atomic<bool> dpi_verified{false};
std::atomic<bool> input_observed{false};

struct TestClassTraits {
    static const wchar_t* GetClassName() noexcept {
        return L"mwfl.AdvancedLifecycleTest";
    }
    static UINT GetClassStyle() noexcept {
        return CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    }
    static HICON GetIcon() noexcept { return nullptr; }
    static HICON GetSmallIcon() noexcept { return nullptr; }
    static HCURSOR GetCursor() noexcept { return ::LoadCursorW(nullptr, IDC_CROSS); }
    static HBRUSH GetBackground() noexcept { return nullptr; }
};

class AdvancedWindow final
    : public mwfl::Window<AdvancedWindow, TestClassTraits> {
public:
    using BaseWindow = mwfl::Window<AdvancedWindow, TestClassTraits>;
    explicit AdvancedWindow(int injected_value) noexcept
        : injected_value_(injected_value) {}

    void BuildUI() {
        test_window = GetHwnd();
        const ULONG_PTR style = static_cast<ULONG_PTR>(
            ::GetClassLongPtrW(GetHwnd(), GCL_STYLE));
        custom_verified.store(
            (style & CS_DBLCLKS) != 0 && injected_value_ == 42,
            std::memory_order_release);

        if (mode == Mode::custom_window) {
            ::PostMessageW(GetHwnd(), WM_APP + 8, 0, 0);
        } else if (mode == Mode::exact_exit) {
            ::PostQuitMessage(42);
        } else if (mode == Mode::dpi_change) {
            ::SendMessageW(GetHwnd(), WM_DPICHANGED, MAKELONG(144, 144),
                           reinterpret_cast<LPARAM>(&dpi_suggested));
        } else if (mode == Mode::after_dispatch_exception) {
            ::PostMessageW(GetHwnd(), WM_APP + 9, 0, 0);
        } else if (mode == Mode::wakeup || mode == Mode::wake_stress ||
                   mode == Mode::wake_latency) {
            wakeup = GetWakeup();
            producer = std::thread([] {
                const unsigned count = mode == Mode::wake_stress ? 2000u :
                    (mode == Mode::wake_latency ? 500u : 1u);
                bool posted = true;
                for (unsigned index = 0; index < count; ++index) {
                    if (mode == Mode::wake_latency) {
                        wake_posted_ns.store(
                            std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch()).count(),
                            std::memory_order_release);
                    }
                    posted = wakeup.TryWake() && posted;
                    if (mode == Mode::wake_stress && index == 999) {
                        posted = ::PostMessageW(test_window, WM_KEYDOWN, VK_SPACE, 0) != FALSE &&
                            posted;
                    }
                    if (mode == Mode::wake_latency) {
                        while (wake_count.load(std::memory_order_acquire) <= index) {
                            ::Sleep(0);
                        }
                    }
                }
                wake_posted.store(posted, std::memory_order_release);
            });
        } else if (mode == Mode::com_sta || mode == Mode::ole_sta) {
            APTTYPE apartment{};
            APTTYPEQUALIFIER qualifier{};
            const HRESULT repeated_ole = mode == Mode::ole_sta ? ::OleInitialize(nullptr) : S_OK;
            com_verified.store(
                SUCCEEDED(::CoGetApartmentType(&apartment, &qualifier)) &&
                    (apartment == APTTYPE_STA || apartment == APTTYPE_MAINSTA) &&
                    (mode != Mode::ole_sta || repeated_ole == S_FALSE),
                std::memory_order_release);
            if (mode == Mode::ole_sta && SUCCEEDED(repeated_ole)) ::OleUninitialize();
            ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
        }
    }

    BEGIN_MSG_MAP(AdvancedWindow)
        MESSAGE_HANDLER(mwfl::WindowWakeup::Message(), OnWake)
        MESSAGE_HANDLER(WM_APP + 8, OnVerifyResources)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
        MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
        CHAIN_MSG_MAP(BaseWindow)
    END_MSG_MAP()

private:
    int injected_value_ = 0;

    LRESULT OnVerifyResources(UINT, WPARAM, LPARAM, BOOL&) noexcept {
        const HICON expected = ::LoadIconW(nullptr, IDI_APPLICATION);
        const HICON actual = reinterpret_cast<HICON>(
            ::SendMessageW(GetHwnd(), WM_GETICON, ICON_BIG, 0));
        custom_verified.store(
            custom_verified.load(std::memory_order_acquire) && actual == expected,
            std::memory_order_release);
        ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
        return 0;
    }

    LRESULT OnWake(UINT, WPARAM, LPARAM, BOOL&) noexcept {
        const unsigned received = wake_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (mode == Mode::wake_latency && received <= wake_latency_us.size()) {
            const long long now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            wake_latency_us[received - 1] =
                (now - wake_posted_ns.load(std::memory_order_acquire)) / 1000;
        }
        if (mode == Mode::wake_stress && received == 2000) {
            ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
        } else if (mode == Mode::wake_latency && received == wake_latency_us.size()) {
            ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
        }
        return 0;
    }

    LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL& handled) noexcept {
        RECT actual{};
        ::GetWindowRect(GetHwnd(), &actual);
        dpi_verified.store(
            actual.left == dpi_suggested.left && actual.top == dpi_suggested.top &&
            actual.right == dpi_suggested.right && actual.bottom == dpi_suggested.bottom,
            std::memory_order_release);
        ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
        handled = FALSE;
        return 0;
    }

    LRESULT OnKeyDown(UINT, WPARAM, LPARAM, BOOL&) noexcept {
        input_observed.store(true, std::memory_order_release);
        return 0;
    }
};

void AfterDispatch(const MSG& message) {
    if (mode == Mode::after_dispatch_exception && message.message == WM_APP + 9) {
        throw std::runtime_error("injected AfterDispatch failure");
    }
    if (mode == Mode::wakeup &&
        message.message == mwfl::WindowWakeup::Message()) {
        ::PostMessageW(test_window, WM_CLOSE, 0, 0);
    }
}

void OnIdle() {
    if (mode == Mode::pump_exception) {
        throw std::runtime_error("injected wait-aware idle failure");
    }
    if (mode == Mode::idle_efficiency && ++idle_count == 10) {
        ::PostMessageW(test_window, WM_CLOSE, 0, 0);
    }
    if (mode == Mode::wait_failure && !wait_handle_closed) {
        ::CloseHandle(wait_event);
        wait_handle_closed = true;
    }
}

void OnHandleSignaled(std::size_t index) {
    if (mode == Mode::wait_callback_exception) {
        throw std::runtime_error("injected wait-handle callback failure");
    }
    if (mode == Mode::wait_handle && index == 0) {
        ::PostMessageW(test_window, WM_CLOSE, 0, 0);
    }
}

std::chrono::milliseconds NextInterval() noexcept {
    if (mode == Mode::pump_exception) return std::chrono::milliseconds{1};
    if (mode == Mode::idle_efficiency) return std::chrono::milliseconds{20};
    return std::chrono::milliseconds{5000};
}

bool LifecycleWasBalanced() {
    const mwfl::detail::LifecycleSnapshot value =
        mwfl::detail::GetLifecycleSnapshotForTesting();
    return value.module_initialized == 1 && value.loop_registered == 1 &&
        value.loop_removed == 1 && value.module_terminated == 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 20;
    if (std::strcmp(argv[1], "custom_window") == 0) mode = Mode::custom_window;
    else if (std::strcmp(argv[1], "wait_handle") == 0) mode = Mode::wait_handle;
    else if (std::strcmp(argv[1], "wakeup") == 0) mode = Mode::wakeup;
    else if (std::strcmp(argv[1], "wake_stress") == 0) mode = Mode::wake_stress;
    else if (std::strcmp(argv[1], "wake_latency") == 0) mode = Mode::wake_latency;
    else if (std::strcmp(argv[1], "idle_efficiency") == 0) mode = Mode::idle_efficiency;
    else if (std::strcmp(argv[1], "com_sta") == 0) mode = Mode::com_sta;
    else if (std::strcmp(argv[1], "ole_sta") == 0) mode = Mode::ole_sta;
    else if (std::strcmp(argv[1], "com_conflict") == 0) mode = Mode::com_conflict;
    else if (std::strcmp(argv[1], "pump_exception") == 0) mode = Mode::pump_exception;
    else if (std::strcmp(argv[1], "after_dispatch_exception") == 0) mode = Mode::after_dispatch_exception;
    else if (std::strcmp(argv[1], "wait_callback_exception") == 0) mode = Mode::wait_callback_exception;
    else if (std::strcmp(argv[1], "wait_failure") == 0) mode = Mode::wait_failure;
    else if (std::strcmp(argv[1], "exact_exit") == 0) mode = Mode::exact_exit;
    else if (std::strcmp(argv[1], "dpi_change") == 0) mode = Mode::dpi_change;
    else return 21;

    mwfl::detail::ResetLifecycleSnapshotForTesting();
    if (mode == Mode::wait_handle || mode == Mode::wait_callback_exception ||
        mode == Mode::wait_failure) {
        wait_event = ::CreateEventW(
            nullptr,
            TRUE,
            mode == Mode::wait_failure ? FALSE : TRUE,
            nullptr);
        if (wait_event == nullptr) return 22;
    }
    const HANDLE handles[] = {wait_event};
    const bool waits_on_handle = mode == Mode::wait_handle ||
        mode == Mode::wait_callback_exception || mode == Mode::wait_failure;
    const std::span<const HANDLE> wait_handles = waits_on_handle
        ? std::span<const HANDLE>{handles}
        : std::span<const HANDLE>{};
    mwfl::WaitAwareMessagePump pump({
        .handles = wait_handles,
        .idle_interval = std::chrono::milliseconds{
            (mode == Mode::pump_exception || mode == Mode::wait_failure) ? 1 :
            (mode == Mode::idle_efficiency ? 20 : 5000)},
        .after_dispatch = AfterDispatch,
        .on_idle = OnIdle,
        .on_signal = OnHandleSignaled,
        .next_interval = NextInterval,
    });

    mwfl::WindowOptions window_options;
    window_options.title = L"mwfl advanced lifecycle test";
    window_options.use_default_bounds = false;
    window_options.initial_bounds = {
        {20.0_dip, 20.0_dip}, {800.0_dip, 500.0_dip}};
    window_options.center_in_work_area = false;
    if (mode == Mode::custom_window) {
        window_options.icon = ::LoadIconW(nullptr, IDI_APPLICATION);
    }

    const bool injected_com_mta = mode == Mode::com_conflict &&
        SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    if (mode == Mode::com_conflict && !injected_com_mta) return 31;
    const mwfl::ApplicationOptions application_options{
        mode == Mode::ole_sta ? mwfl::ComApartment::ole_sta :
        (mode == Mode::com_sta || mode == Mode::com_conflict) ? mwfl::ComApartment::sta
                              : mwfl::ComApartment::none};
    mwfl::Application application(::GetModuleHandleW(nullptr), application_options);
    FILETIME created{}, exited{}, kernel_before{}, user_before{};
    ::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &kernel_before, &user_before);
    const auto wall_start = std::chrono::steady_clock::now();
    const int result = application.Run<AdvancedWindow>(
        SW_HIDE, window_options, pump, 42);
    const auto wall_elapsed = std::chrono::steady_clock::now() - wall_start;
    FILETIME kernel_after{}, user_after{};
    ::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &kernel_after, &user_after);

    if (producer.joinable()) producer.join();
    if (wait_event != nullptr && mode != Mode::wait_failure) ::CloseHandle(wait_event);
    if (injected_com_mta) ::CoUninitialize();

    if (mode == Mode::com_conflict) {
        const mwfl::detail::LifecycleSnapshot snapshot =
            mwfl::detail::GetLifecycleSnapshotForTesting();
        return result != 0 && snapshot.module_initialized == 0 &&
            snapshot.loop_registered == 0 && snapshot.loop_removed == 0 &&
            snapshot.module_terminated == 0 ? 0 : 32;
    }

    const bool expected_failure = mode == Mode::pump_exception ||
        mode == Mode::after_dispatch_exception || mode == Mode::wait_callback_exception ||
        mode == Mode::wait_failure;
    if (mode == Mode::exact_exit ? result != 42 : ((result != 0) != expected_failure)) return 23;
    if (!LifecycleWasBalanced()) return 24;
    if (!custom_verified.load(std::memory_order_acquire)) return 25;
    if (mode == Mode::wakeup &&
        (!wake_posted.load(std::memory_order_acquire) || wakeup.TryWake())) return 26;
    if ((mode == Mode::com_sta || mode == Mode::ole_sta) &&
        !com_verified.load(std::memory_order_acquire)) return 27;
    if (mode == Mode::wake_stress &&
        (!wake_posted.load(std::memory_order_acquire) || wake_count.load() != 2000 ||
         !input_observed.load(std::memory_order_acquire))) return 28;
    if (mode == Mode::wake_latency) {
        if (!wake_posted.load(std::memory_order_acquire) || wake_count.load() != 500) return 33;
        std::ranges::sort(wake_latency_us);
        std::printf("wake p95=%lld us, median=%lld us\n",
                    wake_latency_us[474], wake_latency_us[249]);
        if (wake_latency_us[474] > 16000) return 34;
    }
    if (mode == Mode::idle_efficiency) {
        const auto filetime_value = [](FILETIME value) noexcept {
            return (static_cast<unsigned long long>(value.dwHighDateTime) << 32) |
                value.dwLowDateTime;
        };
        const unsigned long long cpu_100ns =
            filetime_value(kernel_after) - filetime_value(kernel_before) +
            filetime_value(user_after) - filetime_value(user_before);
        std::printf("idle wall=%lld ms, process-cpu=%.3f ms\n",
                    static_cast<long long>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(wall_elapsed).count()),
                    static_cast<double>(cpu_100ns) / 10000.0);
        if (wall_elapsed < std::chrono::milliseconds(180) || cpu_100ns > 1000000ULL) return 29;
    }
    if (mode == Mode::dpi_change && !dpi_verified.load(std::memory_order_acquire)) return 30;
    return 0;
}
