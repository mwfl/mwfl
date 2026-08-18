#include "protocol.h"

#include <mwfl/diagnostics.h>
#include <mwfl/ipc.h>
#include <mwfl/process.h>

#include <array>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace {
class FailingSink final : public mwfl::DiagnosticSink {
  public:
    mwfl::Result<void> Write(const mwfl::SanitizedDiagnosticEvent&) noexcept override {
        return mwfl::SystemError::FromWin32(ERROR_WRITE_FAULT);
    }
};

int Worker(std::wstring_view name, std::wstring_view mode) {
    if (mode == L"malformed") {
        HANDLE pipe = CreateFileW(std::wstring(name).c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                  nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) return 20;
        const std::array<std::byte, 4> malicious{std::byte{0xff}, std::byte{0xff},
                                                std::byte{0xff}, std::byte{0x7f}};
        DWORD written = 0;
        const BOOL ok = WriteFile(pipe, malicious.data(), static_cast<DWORD>(malicious.size()),
                                  &written, nullptr);
        CloseHandle(pipe);
        return ok && written == malicious.size() ? 0 : 21;
    }
    auto connected = mwfl::ConnectPipe({std::wstring(name), 1024}, mwfl::Deadline::After(2s));
    if (!connected || connected.Value().status != mwfl::CompletionStatus::Completed) return 20;
    auto channels = std::move(*connected.Value().value).Split();
    auto hello = channels.writer.WriteFrame(example::Encode("hello"), mwfl::Deadline::After(2s));
    if (!hello || hello.Value().status != mwfl::CompletionStatus::Completed) return 21;
    if (mode == L"crash") TerminateProcess(GetCurrentProcess(), 0xDEAD);
    if (mode == L"hang") Sleep(INFINITE);
    if (mode == L"disconnect") return 42;
    auto command = channels.reader.ReadFrame(mwfl::Deadline::After(2s));
    if (!command || command.Value().status != mwfl::CompletionStatus::Completed ||
        !example::Is(*command.Value().value, "shutdown")) return 22;
    return 37;
}

mwfl::Result<mwfl::SupervisedProcess> LaunchWorker(const wchar_t* executable,
                                                   std::wstring_view pipe,
                                                   std::wstring_view mode) {
    return mwfl::ProcessBuilder{}
        .Executable(executable)
        .Argument(L"--worker")
        .Argument(std::wstring(pipe))
        .Argument(std::wstring(mode))
        .LaunchSupervised();
}

int RunScenario(const wchar_t* executable, std::wstring_view mode, int ordinal) {
    const std::wstring name = L"\\\\.\\pipe\\mwfl-supervised-" +
                              std::to_wstring(GetCurrentProcessId()) + L"-" +
                              std::to_wstring(ordinal);
    mwfl::PipeEndpoint options{name, mode == L"malformed" ? 128u : 1024u};
    if (mode == L"unauthorized")
        options.access = mwfl::PipeAccessPolicy::Explicit({L"S-1-5-18"});
    auto listener = mwfl::PipeListener::Create(options);
    if (!listener) return 100 + ordinal;
    auto worker = LaunchWorker(executable, name, mode == L"unauthorized" ? L"normal" : mode);
    if (!worker) return 110 + ordinal;
    auto accepted = listener.Value().Accept(
        mwfl::Deadline::After(mode == L"unauthorized" ? 500ms : 2s));
    if (mode == L"unauthorized") {
        auto exited = worker.Value().Wait(mwfl::Deadline::After(2s));
        return accepted && accepted.Value().status == mwfl::CompletionStatus::TimedOut && exited &&
                       exited.Value().status == mwfl::CompletionStatus::Completed &&
                       *exited.Value().value == 20
                   ? 0
                   : 120 + ordinal;
    }
    if (!accepted || accepted.Value().status != mwfl::CompletionStatus::Completed)
        return 130 + ordinal;
    if (mode == L"malformed") {
        auto channels = std::move(*accepted.Value().value).Split();
        auto frame = channels.reader.ReadFrame(mwfl::Deadline::After(2s));
        auto exited = worker.Value().Wait(mwfl::Deadline::After(2s));
        return !frame && frame.GetError().code == ERROR_BUFFER_OVERFLOW && exited &&
                       exited.Value().status == mwfl::CompletionStatus::Completed
                   ? 0
                   : 140 + ordinal;
    }
    auto peer = accepted.Value().value->QueryPeer();
    if (!peer || peer.Value().process_id != worker.Value().Id()) return 150 + ordinal;
    auto channels = std::move(*accepted.Value().value).Split();
    auto hello = channels.reader.ReadFrame(mwfl::Deadline::After(2s));
    if (!hello || hello.Value().status != mwfl::CompletionStatus::Completed ||
        !example::Is(*hello.Value().value, "hello")) return 160 + ordinal;
    if (mode == L"crash") {
        auto exited = worker.Value().Wait(mwfl::Deadline::After(2s));
        return exited && exited.Value().status == mwfl::CompletionStatus::Completed &&
                       *exited.Value().value == 0xDEAD
                   ? 0
                   : 170 + ordinal;
    }
    if (mode == L"hang") {
        auto timed = worker.Value().Wait(mwfl::Deadline::After(100ms));
        if (!timed || timed.Value().status != mwfl::CompletionStatus::TimedOut) return 180 + ordinal;
        if (!worker.Value().TerminateTree(99)) return 181 + ordinal;
        auto exited = worker.Value().Wait(mwfl::Deadline::After(2s));
        return exited && exited.Value().status == mwfl::CompletionStatus::Completed ? 0 : 182 + ordinal;
    }
    if (mode == L"disconnect") {
        auto command = channels.writer.WriteFrame(example::Encode("shutdown"),
                                                   mwfl::Deadline::After(2s));
        auto exited = worker.Value().Wait(mwfl::Deadline::After(2s));
        return command && exited && exited.Value().status == mwfl::CompletionStatus::Completed &&
                       *exited.Value().value == 42
                   ? 0
                   : 190 + ordinal;
    }
    mwfl::DiagnosticPipeline diagnostics;
    diagnostics.Add(std::make_shared<mwfl::DebugOutputSink>());
    if (mode == L"log-failure") diagnostics.Add(std::make_shared<FailingSink>());
    auto report = diagnostics.Write({mwfl::EventLevel::Information,
                                     L"worker",
                                     1,
                                     {{L"phase", L"ready"}},
                                     {},
                                     0,
                                     0,
                                     std::to_wstring(worker.Value().Id())});
    if ((mode == L"log-failure" && (report.succeeded != 1 || report.failed != 1)) ||
        (mode != L"log-failure" && report.failed != 0))
        return 200 + ordinal;
    auto shutdown = channels.writer.WriteFrame(example::Encode("shutdown"),
                                                mwfl::Deadline::After(2s));
    auto exited = worker.Value().Wait(mwfl::Deadline::After(2s));
    return shutdown && exited && exited.Value().status == mwfl::CompletionStatus::Completed &&
                   *exited.Value().value == 37
               ? 0
               : 210 + ordinal;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 4 && std::wstring_view(argv[1]) == L"--worker")
        return Worker(argv[2], argv[3]);
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    wchar_t executable[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return 2;
    constexpr std::array modes{L"normal", L"crash", L"hang", L"malformed", L"disconnect",
                               L"log-failure", L"unauthorized"};
    for (int index = 0; index != static_cast<int>(modes.size()); ++index) {
        const int result = RunScenario(executable, modes[index], index);
        if (result) return result;
    }
    std::wcout << L"supervised worker reference scenarios passed\n";
    return 0;
}
