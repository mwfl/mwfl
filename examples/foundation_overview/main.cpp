#include <mwfl/core.h>
#include <mwfl/deployment.h>
#include <mwfl/diagnostics.h>
#include <mwfl/ipc.h>
#include <mwfl/process.h>
#include <mwfl/security.h>
#include <mwfl/service.h>
#include <iostream>
#include <string_view>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") { std::wcout << L"Usage: mwfl_foundation_overview --self-test\n"; return 0; }
    auto unicode = mwfl::Utf8ToWide("foundation"); if (!unicode) return 1;
    mwfl::ServiceStateMachine service; if (!service.Transition(mwfl::ServiceState::StartPending)) return 2;
    if (mwfl::QuoteWindowsArgument(L"a b") != L"\"a b\"") return 3;
    if (mwfl::ConnectPipe({L"invalid", 1})) return 4;
    if (mwfl::FormatDiagnosticEvent({mwfl::EventLevel::Information, L"overview", 1, {}}).empty()) return 5;
    mwfl::SecureBytes secret; if (secret.Size() != 0) return 6;
    auto identity = mwfl::QueryCurrentPackageIdentity(); if (!identity) return 7;
    std::wcout << L"0.1.1-0.1.9 Foundation targets compose successfully\n"; return 0;
}
