#include <mwfl/ipc.h>
#include <chrono>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    mwfl::PipeEndpoint options{L"\\\\.\\pipe\\mwfl-secure-" + std::to_wstring(GetCurrentProcessId()), 256};
    auto server = mwfl::PipeListener::Create(options); if (!server) return 1;
    std::jthread client([&] {
        auto connection = mwfl::ConnectPipe(options, mwfl::Deadline::After(5s));
        if (connection && connection.Value().status == mwfl::CompletionStatus::Completed) {
            auto channels = std::move(*connection.Value().value).Split();
            (void)channels.writer.WriteFrame({}, mwfl::Deadline::After(5s));
        }
    });
    auto accepted = server.Value().Accept(mwfl::Deadline::After(5s));
    if (!accepted || accepted.Value().status != mwfl::CompletionStatus::Completed) return 2;
    auto peer = accepted.Value().value->QueryPeer();
    if (!peer || peer.Value().user_sid.empty()) return 3;
    auto channels = std::move(*accepted.Value().value).Split();
    auto frame = channels.reader.ReadFrame(mwfl::Deadline::After(5s));
    if (!frame || frame.Value().status != mwfl::CompletionStatus::Completed) return 4;
    std::wcout << L"secure local IPC peer identity passed\n"; return 0;
}
