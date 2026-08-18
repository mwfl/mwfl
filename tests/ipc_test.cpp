#include <cassert>
#include <mwfl/ipc.h>
#include <thread>
int main() {
    auto invalid = mwfl::ConnectPipe({L"not-a-pipe", 32});
    assert(!invalid && invalid.Error().code == ERROR_INVALID_PARAMETER);
    auto zero = mwfl::ConnectPipe({L"\\\\.\\pipe\\mwfl-invalid", 0});
    assert(!zero && zero.Error().code == ERROR_INVALID_PARAMETER);
    auto oversized =
        mwfl::PipeServer::Create({L"\\\\.\\pipe\\mwfl-invalid", mwfl::MaximumPipeFrameSize + 1});
    assert(!oversized && oversized.Error().code == ERROR_INVALID_PARAMETER);

    const auto name = L"\\\\.\\pipe\\mwfl-ipc-reconnect-" + std::to_wstring(GetCurrentProcessId());
    mwfl::PipeOptions options{name, 128};
    auto server = mwfl::PipeServer::Create(options);
    assert(server);
    for (int attempt = 0; attempt != 2; ++attempt) {
        std::jthread client([&] {
            auto connected = mwfl::ConnectPipe(options, std::chrono::seconds(2));
            assert(connected && connected.Value().status == mwfl::PipeOperationStatus::Completed);
            const std::byte value{static_cast<unsigned char>(attempt + 1)};
            auto written = connected.Value().connection.WriteFrame(
                std::span<const std::byte>(&value, 1), std::chrono::seconds(2));
            assert(written && written.Value().status == mwfl::PipeOperationStatus::Completed);
        });
        auto accepted = server.Value().Accept(std::chrono::seconds(2));
        assert(accepted && accepted.Value().status == mwfl::PipeOperationStatus::Completed);
        auto frame = accepted.Value().connection.ReadFrame(std::chrono::seconds(2));
        assert(frame && frame.Value().status == mwfl::PipeOperationStatus::Completed);
        assert(frame.Value().payload.size() == 1);
    }
}
