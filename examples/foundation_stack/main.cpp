#include <mwfl/diagnostics.h>
#include <mwfl/ipc.h>
#include <mwfl/process.h>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
std::vector<std::byte> Bytes(std::string_view text) { const auto* first = reinterpret_cast<const std::byte*>(text.data()); return {first, first + text.size()}; }
int Worker(std::wstring_view pipe_name) {
    const mwfl::PipeOptions options{std::wstring(pipe_name), 1024};
    mwfl::Result<mwfl::PipeConnection> connection = mwfl::NativeError::FromWin32(ERROR_PIPE_BUSY);
    for (int attempt = 0; attempt < 100 && !connection; ++attempt) { connection = mwfl::ConnectPipe(options); if (!connection) std::this_thread::sleep_for(10ms); }
    if (!connection || !connection.Value().WriteFrame(Bytes("worker-ready"))) return 20;
    auto ack = connection.Value().ReadFrame(); return ack && ack.Value() == Bytes("controller-ack") ? 23 : 21;
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wstring_view(argv[1]) == L"--worker") return Worker(argv[2]);
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") { std::wcout << L"Usage: mwfl_foundation_stack --self-test\n"; return 0; }
    mwfl::DiagnosticPipeline diagnostics; diagnostics.Add(std::make_shared<mwfl::DebugOutputSink>());
    (void)diagnostics.Write({mwfl::EventLevel::Information, L"foundation-stack", 1, {{L"phase", L"launch"}}});
    wchar_t executable[MAX_PATH]{}; if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) return 1;
    const std::wstring pipe_name = L"\\\\.\\pipe\\mwfl-foundation-stack-" + std::to_wstring(GetCurrentProcessId());
    auto child = mwfl::ProcessBuilder{}.Executable(executable).Argument(L"--worker").Argument(pipe_name).Launch(); if (!child) return 2;
    auto connection = mwfl::PipeServer({pipe_name, 1024}).Accept(); if (!connection) return 3;
    auto ready = connection.Value().ReadFrame(); if (!ready || ready.Value() != Bytes("worker-ready")) return 4;
    if (!connection.Value().WriteFrame(Bytes("controller-ack"))) return 5;
    auto exit = child.Value().Wait(5s); if (!exit || exit.Value().code != 23) return 6;
    (void)diagnostics.Write({mwfl::EventLevel::Information, L"foundation-stack", 2, {{L"phase", L"complete"}}});
    std::wcout << L"process + IPC + diagnostics foundation stack passed\n"; return 0;
}
