#include <cassert>
#include <mwfl/ipc.h>
#include <thread>
int main() {
    auto invalid = mwfl::ConnectPipe({L"not-a-pipe", 32},
                                     mwfl::Deadline::After(std::chrono::milliseconds{0}));
    assert(!invalid && invalid.GetError().code == ERROR_INVALID_PARAMETER);
    auto zero = mwfl::ConnectPipe({L"\\\\.\\pipe\\mwfl-invalid", 0},
                                  mwfl::Deadline::After(std::chrono::milliseconds{0}));
    assert(!zero && zero.GetError().code == ERROR_INVALID_PARAMETER);
    auto oversized =
        mwfl::PipeListener::Create({L"\\\\.\\pipe\\mwfl-invalid", mwfl::MaximumPipeFrameSize + 1});
    assert(!oversized && oversized.GetError().code == ERROR_INVALID_PARAMETER);

    const auto name = L"\\\\.\\pipe\\mwfl-ipc-reconnect-" + std::to_wstring(GetCurrentProcessId());
    mwfl::PipeEndpoint options{name, 128};
    auto server = mwfl::PipeListener::Create(options);
    assert(server);
    for (int attempt = 0; attempt != 2; ++attempt) {
        std::jthread client([&] {
            auto connected = mwfl::ConnectPipe(options, mwfl::Deadline::After(std::chrono::seconds(2)));
            assert(connected && connected.Value().status == mwfl::CompletionStatus::Completed);
            auto channels = std::move(*connected.Value().value).Split();
            const std::byte value{static_cast<unsigned char>(attempt + 1)};
            auto written = channels.writer.WriteFrame(
                std::span<const std::byte>(&value, 1), mwfl::Deadline::After(std::chrono::seconds(2)));
            assert(written && written.Value().status == mwfl::CompletionStatus::Completed);
        });
        auto accepted = server.Value().Accept(mwfl::Deadline::After(std::chrono::seconds(2)));
        assert(accepted && accepted.Value().status == mwfl::CompletionStatus::Completed);
        auto channels = std::move(*accepted.Value().value).Split();
        auto frame = channels.reader.ReadFrame(mwfl::Deadline::After(std::chrono::seconds(2)));
        assert(frame && frame.Value().status == mwfl::CompletionStatus::Completed);
        assert(frame.Value().value->size() == 1);
    }
}
