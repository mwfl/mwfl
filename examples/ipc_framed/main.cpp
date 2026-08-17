#include <mwfl/ipc.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
std::vector<std::byte> Bytes(std::string_view text) { auto p = reinterpret_cast<const std::byte*>(text.data()); return {p, p + text.size()}; }
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") { std::wcout << L"Usage: mwfl_ipc_framed --self-test\n"; return 0; }
    const mwfl::PipeOptions options{L"\\\\.\\pipe\\mwfl-ipc-example-" + std::to_wstring(GetCurrentProcessId()), 1024};
    int server_result = 1;
    std::jthread server([&] {
        auto accepted = mwfl::PipeServer(options).Accept(); if (!accepted) return;
        auto request = accepted.Value().ReadFrame(); if (!request || request.Value() != Bytes("ping")) return;
        auto sent = accepted.Value().WriteFrame(Bytes("pong")); if (!sent) return; server_result = 0;
    });
    mwfl::Result<mwfl::PipeConnection> connected = mwfl::NativeError::FromWin32(ERROR_PIPE_BUSY);
    for (int attempt = 0; attempt < 50 && !connected; ++attempt) { connected = mwfl::ConnectPipe(options); if (!connected) std::this_thread::sleep_for(10ms); }
    if (!connected || !connected.Value().WriteFrame(Bytes("ping"))) return 2;
    auto response = connected.Value().ReadFrame(); if (!response || response.Value() != Bytes("pong")) return 3;
    server.join(); if (server_result != 0) return 4;
    std::wcout << L"bounded named-pipe frames passed\n"; return 0;
}
