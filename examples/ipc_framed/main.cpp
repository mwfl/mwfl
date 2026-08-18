#include <chrono>
#include <cstring>
#include <iostream>
#include <mwfl/ipc.h>
#include <string_view>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
std::vector<std::byte> Bytes(std::string_view text) {
    auto p = reinterpret_cast<const std::byte*>(text.data());
    return {p, p + text.size()};
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") {
        std::wcout << L"Usage: mwfl_ipc_framed --self-test\n";
        return 0;
    }
    const mwfl::PipeOptions options{
        L"\\\\.\\pipe\\mwfl-ipc-example-" + std::to_wstring(GetCurrentProcessId()), 1024};
    auto listener = mwfl::PipeServer::Create(options);
    if (!listener) return 1;
    int server_result = 1;
    std::jthread server([&] {
        auto accepted = listener.Value().Accept(5s);
        if (!accepted || accepted.Value().status != mwfl::PipeOperationStatus::Completed) return;
        auto request = accepted.Value().connection.ReadFrame(5s);
        if (!request || request.Value().status != mwfl::PipeOperationStatus::Completed ||
            request.Value().payload != Bytes("ping"))
            return;
        auto sent = accepted.Value().connection.WriteFrame(Bytes("pong"), 5s);
        if (!sent || sent.Value().status != mwfl::PipeOperationStatus::Completed) return;
        server_result = 0;
    });
    auto connected = mwfl::ConnectPipe(options, 5s);
    if (!connected || connected.Value().status != mwfl::PipeOperationStatus::Completed) return 2;
    auto sent = connected.Value().connection.WriteFrame(Bytes("ping"), 5s);
    if (!sent || sent.Value().status != mwfl::PipeOperationStatus::Completed) return 2;
    auto response = connected.Value().connection.ReadFrame(5s);
    if (!response || response.Value().status != mwfl::PipeOperationStatus::Completed ||
        response.Value().payload != Bytes("pong"))
        return 3;
    server.join();
    if (server_result != 0) return 4;
    std::wcout << L"bounded named-pipe frames passed\n";
    return 0;
}
