#include <mwtl/application.h>

#include "detail/diagnostics.h"
#include "detail/module.h"
#include "detail/testing.h"

#include <stdexcept>

namespace mwtl {

#ifdef MWTL_TESTING
namespace detail {
namespace {
LifecycleSnapshot lifecycle_snapshot{};

bool IsFailureInjected(const wchar_t* name) noexcept {
    wchar_t value[2]{};
    return ::GetEnvironmentVariableW(name, value, _countof(value)) != 0;
}
}

void ResetLifecycleSnapshotForTesting() noexcept {
    lifecycle_snapshot = {};
}

LifecycleSnapshot GetLifecycleSnapshotForTesting() noexcept {
    return lifecycle_snapshot;
}
}  // namespace detail
#endif

Application::Application(HINSTANCE instance) noexcept : instance_(instance) {}

Application::Application(HINSTANCE instance, ApplicationOptions options) noexcept
    : instance_(instance), options_(options) {}

Application::~Application() noexcept {
    EndRun();
}

bool Application::BeginRun() noexcept {
    if (running_) {
        detail::ReportHresult(L"Application::Run re-entry", E_UNEXPECTED, true);
        return false;
    }
    running_ = true;

    if (options_.com_apartment != ComApartment::none) {
        const bool use_ole = options_.com_apartment == ComApartment::ole_sta;
        const DWORD flags = options_.com_apartment == ComApartment::mta
            ? COINIT_MULTITHREADED
            : COINIT_APARTMENTTHREADED;
        const HRESULT com_result = use_ole ? ::OleInitialize(nullptr)
                                           : ::CoInitializeEx(nullptr, flags);
        if (FAILED(com_result)) {
            detail::ReportHresult(
                com_result == RPC_E_CHANGED_MODE
                    ? L"COM apartment mode conflict"
                    : (use_ole ? L"OleInitialize" : L"CoInitializeEx"),
                com_result,
                true);
            running_ = false;
            return false;
        }
        if (use_ole) ole_initialized_ = true;
        else com_uninitialize_.activate();
    }

    const HRESULT result = _Module.Init(nullptr, instance_);
#ifdef MWTL_TESTING
    if (SUCCEEDED(result)) {
        ++detail::lifecycle_snapshot.module_initialized;
    }
    const HRESULT effective_result =
        SUCCEEDED(result) && detail::IsFailureInjected(L"MWTL_TEST_FAIL_MODULE_INIT")
        ? E_OUTOFMEMORY
        : result;
#else
    const HRESULT effective_result = result;
#endif
    if (FAILED(effective_result)) {
        detail::ReportHresult(L"WTL CAppModule::Init", effective_result, true);
        _Module.Term();
#ifdef MWTL_TESTING
        ++detail::lifecycle_snapshot.module_terminated;
#endif
        if (ole_initialized_) {
            ::OleUninitialize();
            ole_initialized_ = false;
        } else {
            com_uninitialize_.reset();
        }
        running_ = false;
        return false;
    }
    module_initialized_ = true;

    const BOOL loop_added =
#ifdef MWTL_TESTING
        detail::IsFailureInjected(L"MWTL_TEST_FAIL_LOOP_REGISTRATION")
        ? FALSE
        :
#endif
        _Module.AddMessageLoop(&message_loop_);
    if (loop_added == FALSE) {
        const DWORD error = ::GetLastError();
        detail::ReportWin32(
            L"WTL CAppModule::AddMessageLoop",
            error == ERROR_SUCCESS ? ERROR_NOT_ENOUGH_MEMORY : error,
            true);
        EndRun();
        return false;
    }
    loop_registered_ = true;
#ifdef MWTL_TESTING
    ++detail::lifecycle_snapshot.loop_registered;
#endif
    return true;
}

void Application::EndRun() noexcept {
    if (loop_registered_) {
        if (_Module.RemoveMessageLoop() == FALSE) {
            const DWORD error = ::GetLastError();
            detail::ReportWin32(
                L"WTL CAppModule::RemoveMessageLoop",
                error == ERROR_SUCCESS ? ERROR_INVALID_STATE : error,
                false);
        }
        loop_registered_ = false;
#ifdef MWTL_TESTING
        ++detail::lifecycle_snapshot.loop_removed;
#endif
    }
    if (module_initialized_) {
        _Module.Term();
        module_initialized_ = false;
#ifdef MWTL_TESTING
        ++detail::lifecycle_snapshot.module_terminated;
#endif
    }
    if (ole_initialized_) {
        ::OleUninitialize();
        ole_initialized_ = false;
    } else {
        com_uninitialize_.reset();
    }
    running_ = false;
}

int Application::RunMessageLoop(MessagePump* message_pump) {
    return message_pump == nullptr
        ? message_loop_.Run()
        : message_pump->Run(message_loop_);
}

void Application::ReportWindowCreationFailure(DWORD error) noexcept {
    detail::ReportWin32(
        L"main window creation",
        error == ERROR_SUCCESS ? ERROR_CANNOT_MAKE : error,
        true);
}

void Application::ReportWindowDisplayFailure(DWORD error) noexcept {
    detail::ReportWin32(L"main window display", error, true);
}

void Application::ReportRunException(const char* description) noexcept {
    detail::ReportException(L"Application::Run", 0, description, true);
}

void Application::ReportUnknownRunException() noexcept {
    detail::ReportUnknownException(L"Application::Run", 0, true);
}

}  // namespace mwtl
