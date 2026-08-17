#include <mwfl/ipc.h>
#include <array>
#include <cstring>
namespace mwfl { namespace {
bool Valid(const PipeOptions& options) { return options.name.starts_with(L"\\\\.\\pipe\\") && options.maximum_frame_size > 0; }
Result<void> WriteAll(HANDLE pipe, const void* data, std::uint32_t size) {
    auto* cursor = static_cast<const std::byte*>(data); std::uint32_t remaining = size;
    while (remaining > 0) { DWORD written = 0; if (!WriteFile(pipe, cursor, remaining, &written, nullptr)) return NativeError::LastWin32(); if (written == 0) return NativeError::FromWin32(ERROR_BROKEN_PIPE); cursor += written; remaining -= written; }
    return {};
}
Result<void> ReadAll(HANDLE pipe, void* data, std::uint32_t size) {
    auto* cursor = static_cast<std::byte*>(data); std::uint32_t remaining = size;
    while (remaining > 0) { DWORD read = 0; if (!ReadFile(pipe, cursor, remaining, &read, nullptr)) return NativeError::LastWin32(); if (read == 0) return NativeError::FromWin32(ERROR_BROKEN_PIPE); cursor += read; remaining -= read; }
    return {};
}
}  // namespace
PipeConnection::PipeConnection(KernelHandle pipe, std::uint32_t maximum_frame_size) noexcept : pipe_(std::move(pipe)), maximum_frame_size_(maximum_frame_size) {}
Result<void> PipeConnection::WriteFrame(std::span<const std::byte> payload) {
    if (!pipe_ || payload.size() > maximum_frame_size_ || payload.size() > UINT32_MAX) return NativeError::FromWin32(ERROR_INVALID_DATA);
    const auto size = static_cast<std::uint32_t>(payload.size()); auto header = WriteAll(pipe_.Get(), &size, sizeof(size)); if (!header) return header.Error();
    return size == 0 ? Result<void>{} : WriteAll(pipe_.Get(), payload.data(), size);
}
Result<std::vector<std::byte>> PipeConnection::ReadFrame() {
    if (!pipe_) return NativeError::FromWin32(ERROR_INVALID_HANDLE);
    std::uint32_t size = 0; auto header = ReadAll(pipe_.Get(), &size, sizeof(size)); if (!header) return header.Error();
    if (size > maximum_frame_size_) return NativeError::FromWin32(ERROR_BUFFER_OVERFLOW);
    std::vector<std::byte> payload(size); if (size != 0) { auto body = ReadAll(pipe_.Get(), payload.data(), size); if (!body) return body.Error(); }
    return payload;
}
Result<PipeConnection> PipeServer::Accept() const {
    if (!Valid(options_)) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    KernelHandle pipe(CreateNamedPipeW(options_.name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, options_.maximum_frame_size + 4, options_.maximum_frame_size + 4, 0, nullptr));
    if (!pipe) return NativeError::LastWin32();
    if (!ConnectNamedPipe(pipe.Get(), nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) return NativeError::LastWin32();
    return PipeConnection(std::move(pipe), options_.maximum_frame_size);
}
Result<PipeConnection> ConnectPipe(const PipeOptions& options) {
    if (!Valid(options)) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    KernelHandle pipe(CreateFileW(options.name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!pipe) return NativeError::LastWin32();
    return PipeConnection(std::move(pipe), options.maximum_frame_size);
}
}  // namespace mwfl
