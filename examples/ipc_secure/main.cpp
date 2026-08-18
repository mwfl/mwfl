#include <mwfl/ipc.h>
#include <chrono>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    mwfl::PipeOptions options{L"\\\\.\\pipe\\mwfl-secure-" + std::to_wstring(GetCurrentProcessId()), 256};
    auto server = mwfl::PipeServer::Create(options); if (!server) return 1;
    std::jthread client([&] { auto connection = mwfl::ConnectPipe(options, 5s); if (connection && connection.Value().status == mwfl::PipeOperationStatus::Completed) (void)connection.Value().connection.WriteFrame({}, 5s); });
    auto accepted = server.Value().Accept(5s); if (!accepted || accepted.Value().status != mwfl::PipeOperationStatus::Completed) return 2;
    auto peer = accepted.Value().connection.QueryPeerIdentity(); if (!peer || peer.Value().user_sid.empty()) return 3;
    auto frame = accepted.Value().connection.ReadFrame(5s); if (!frame || frame.Value().status != mwfl::PipeOperationStatus::Completed) return 4;
    std::wcout << L"secure local IPC peer identity passed\n"; return 0;
}
