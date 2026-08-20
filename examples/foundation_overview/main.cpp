#include <iostream>
#include <mwfl/core.h>
#include <mwfl/deployment.h>
#include <mwfl/diagnostics.h>
#include <mwfl/ipc.h>
#include <mwfl/process.h>
#include <mwfl/scheduler.h>
#include <mwfl/security.h>
#include <mwfl/service.h>
#include <string_view>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") {
        std::wcout << L"Usage: mwfl_foundation_overview --self-test\n";
        return 0;
    }
    auto unicode = mwfl::Utf8ToWide("foundation");
    if (!unicode) return 1;
    mwfl::ServiceStateMachine service;
    if (!service.Transition(mwfl::ServiceState::StartPending)) return 2;
    if (mwfl::QuoteWindowsArgument(L"a b") != L"\"a b\"") return 3;
    if (mwfl::ConnectPipe({L"invalid", 1},
                          mwfl::Deadline::After(std::chrono::milliseconds{0}))) return 4;
    auto event = mwfl::DiagnosticEventBuilder(mwfl::EventLevel::Information, L"overview", 1).Build();
    if (mwfl::FormatDiagnosticEvent(mwfl::SanitizeDiagnosticEvent(event)).empty())
        return 5;
    mwfl::SecureBuffer secret;
    if (secret.Size() != 0) return 6;
    auto identity = mwfl::QueryCurrentPackageIdentity();
    if (!identity) return 7;
    mwfl::TaskDefinition task{L"\\mwfl", L"overview", L"", L"test.exe"};
    if (!mwfl::ValidateScheduledTask(task)) return 8;
    std::wcout << L"0.3.0 Foundation targets compose successfully\n";
    return 0;
}
