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
    const mwfl::PipeEndpoint options{
        L"\\\\.\\pipe\\mwfl-ipc-example-" + std::to_wstring(GetCurrentProcessId()), 1024};
    auto listener = mwfl::PipeListener::Create(options);
    if (!listener) return 1;
    int server_result = 1;
    std::jthread server([&] {
        auto accepted = listener.Value().Accept(mwfl::Deadline::After(5s));
        if (!accepted || accepted.Value().status != mwfl::CompletionStatus::Completed) return;
        auto channels = std::move(*accepted.Value().value).Split();
        auto request = channels.reader.ReadFrame(mwfl::Deadline::After(5s));
        if (!request || request.Value().status != mwfl::CompletionStatus::Completed ||
            *request.Value().value != Bytes("ping"))
            return;
        auto sent = channels.writer.WriteFrame(Bytes("pong"), mwfl::Deadline::After(5s));
        if (!sent || sent.Value().status != mwfl::CompletionStatus::Completed) return;
        server_result = 0;
    });
    auto connected = mwfl::ConnectPipe(options, mwfl::Deadline::After(5s));
    if (!connected || connected.Value().status != mwfl::CompletionStatus::Completed) return 2;
    auto channels = std::move(*connected.Value().value).Split();
    auto sent = channels.writer.WriteFrame(Bytes("ping"), mwfl::Deadline::After(5s));
    if (!sent || sent.Value().status != mwfl::CompletionStatus::Completed) return 2;
    auto response = channels.reader.ReadFrame(mwfl::Deadline::After(5s));
    if (!response || response.Value().status != mwfl::CompletionStatus::Completed ||
        *response.Value().value != Bytes("pong"))
        return 3;
    server.join();
    if (server_result != 0) return 4;
    std::wcout << L"bounded named-pipe frames passed\n";
    return 0;
}
