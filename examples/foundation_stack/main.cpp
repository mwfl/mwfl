#include <chrono>
#include <iostream>
#include <mwfl/diagnostics.h>
#include <mwfl/ipc.h>
#include <mwfl/process.h>
#include <string_view>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
std::vector<std::byte> Bytes(std::string_view text) {
    const auto* first = reinterpret_cast<const std::byte*>(text.data());
    return {first, first + text.size()};
}
int Worker(std::wstring_view pipe_name) {
    const mwfl::PipeOptions options{std::wstring(pipe_name), 1024};
    auto connection = mwfl::ConnectPipe(options, 5s);
    if (!connection || connection.Value().status != mwfl::PipeOperationStatus::Completed) return 20;
    auto sent = connection.Value().connection.WriteFrame(Bytes("worker-ready"), 5s);
    if (!sent || sent.Value().status != mwfl::PipeOperationStatus::Completed) return 20;
    auto ack = connection.Value().connection.ReadFrame(5s);
    return ack && ack.Value().status == mwfl::PipeOperationStatus::Completed &&
                   ack.Value().payload == Bytes("controller-ack")
               ? 23
               : 21;
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wstring_view(argv[1]) == L"--worker") return Worker(argv[2]);
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") {
        std::wcout << L"Usage: mwfl_foundation_stack --self-test\n";
        return 0;
    }
    mwfl::DiagnosticPipeline diagnostics;
    diagnostics.Add(std::make_shared<mwfl::DebugOutputSink>());
    (void)diagnostics.Write(
        {mwfl::EventLevel::Information, L"foundation-stack", 1, {{L"phase", L"launch"}}});
    wchar_t executable[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) return 1;
    const std::wstring pipe_name =
        L"\\\\.\\pipe\\mwfl-foundation-stack-" + std::to_wstring(GetCurrentProcessId());
    auto listener = mwfl::PipeServer::Create({pipe_name, 1024});
    if (!listener) return 2;
    auto child = mwfl::ProcessBuilder{}
                     .Executable(executable)
                     .Argument(L"--worker")
                     .Argument(pipe_name)
                     .Supervise()
                     .Launch();
    if (!child) return 2;
    auto connection = listener.Value().Accept(5s);
    if (!connection || connection.Value().status != mwfl::PipeOperationStatus::Completed) return 3;
    auto ready = connection.Value().connection.ReadFrame(5s);
    if (!ready || ready.Value().status != mwfl::PipeOperationStatus::Completed ||
        ready.Value().payload != Bytes("worker-ready"))
        return 4;
    auto sent = connection.Value().connection.WriteFrame(Bytes("controller-ack"), 5s);
    if (!sent || sent.Value().status != mwfl::PipeOperationStatus::Completed) return 5;
    auto exit = child.Value().Wait(5s);
    if (!exit || exit.Value().status != mwfl::ProcessWaitStatus::Exited ||
        exit.Value().exit_code != 23)
        return 6;
    (void)diagnostics.Write(
        {mwfl::EventLevel::Information, L"foundation-stack", 2, {{L"phase", L"complete"}}});
    std::wcout << L"process + IPC + diagnostics foundation stack passed\n";
    return 0;
}
